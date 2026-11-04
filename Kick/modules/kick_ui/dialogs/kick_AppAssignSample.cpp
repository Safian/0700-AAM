/*
 * Copyright (c) 2021 MUSIC Tribe
 *
 * This file is part of Kick.
 *
 * Kick is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Kick is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Kick. If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#include <kick_ui/modes/filebrowser/kick_fileUtils.h>
#include <kick_ui_components/source/kick_WarningComponent.h>
#include "kick_assign_sample_logic.h"

namespace kick_ui
{
    AssignSampleContent::AssignSampleContent()
        : pads( std::make_unique<ui::KickPadButtonGroupComponent>( ui::KickPadButton::PadButtonType::assignSample ) ), ae( mt::getAudioInterface() )
    {
        fileList.setFocus( true );
        fileList.setButtonHeight( 48 ); // AS-14: touch ergonomic row height
        setLookAndFeel( ui::KickLooknFeel::getLnf() );

        mt::utils::addChildComponents( this, pads.get(), fileList, sampleLabel, layerSelector,
                                       assignButton, clearPadButton, auditionToggle, auditionGain, deleteSampleButton );

        layerSelector.setOptions( { "Layer 1", "Layer 2", "Layer 3", "Layer 4" } );
        layerSelector.setSelected( 0, juce::dontSendNotification );
        layerSelector.onSelectionChanged = [this]( int index, const juce::String& ) {
            currentLayer = juce::jlimit( 0, mt::MAX_LAYERS - 1, index );
            refresh();
            updateLabelText();
        };

        assignButton.onClick = [this] {
            const int highlighted = pads->getHighlightedPad();
            if( highlighted >= 0 )
                assignSampleIntoPad( highlighted );
        };

        clearPadButton.onClick = [this] {
            const int highlighted = pads->getHighlightedPad();
            if( highlighted >= 0 )
                confirmClearEntirePad( highlighted );
        };

        auditionToggle.setToggleState( true, juce::dontSendNotification );
        auditionToggle.onClick = [this] {
            if( !auditionToggle.getToggleState() )
                ae.stopPreview();
            else
                scheduleAudition( fileList.getSelectedIndex() );
        };

        auditionGain.setRange( 0.0, 1.0, 0.01 );
        auditionGain.setValue( mt::getSettings().preview.gain, juce::dontSendNotification );
        auditionGain.onValueChanged = []( double v ) {
            mt::getSettings().preview.gain = (float)v;
        };

        deleteSampleButton.onClick = [this] {
            confirmDeleteSample();
        };

        fileList.setBackgroundColour( ui::Theme::backgroundColour( 1 ) );
        fileList.setSpacing( 4, 4, 10 );
        files.insert( files.begin(), "Clear Assignment" );
        fileList.setContent( files, 0 );

        sampleLabel.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        sampleLabel.setValueJustification( juce::Justification::centred );
        sampleLabel.setValueColour( ui::Theme::textColour( 1 ) );

        pads->setFocusOutline( false );
        pads->OnButtonClicked = [&]( int Index ) { //software padbutton pressed
            pads->setHighlight( Index, ui::Theme::padButtonColour( 3 ) );
            ui::fileUtils::setHighlightLED( Index - pads->getBankOffset() );
            refreshLEDs();
            updateLabelText();
        };

        pads->OnDoubleClicked = [&]( int Index ) {
            ui::fileUtils::setHighlightLED( Index - pads->getBankOffset() );
            assignSampleIntoPad( Index );
        };

        pads->onShiftPadPressed = [&]( int id ) { //from PadButtonGroup
            assignSampleIntoPad( id + pads->getBankOffset() );
            ui::fileUtils::setHighlightLED( id );
        };

        pads->onKnobClicked = [&]( int hwPadIndex ) {
            assignSampleIntoPad( hwPadIndex + pads->getBankOffset() );
            ui::fileUtils::setHighlightLED( hwPadIndex );
        };

        fileList.onSelectionChanged = [this]( int Index, juce::String ) {
            updateLabelText();
            if( auditionToggle.getToggleState() )
                scheduleAudition( Index );
            else
                ae.stopPreview();
        };
    }

    // Selection -> asset path straight from the pool index (no directory walk, no wildcard interpretation); only the selection that
    // is still current after a short delay is auditioned, so a fast Data Dial spin does not open a file per step.
    void AssignSampleContent::scheduleAudition( int listIndex )
    {
        const auto ticket = auditionDebounce.request();
        juce::Component::SafePointer<AssignSampleContent> safe( this );
        juce::Timer::callAfterDelay( kAuditionDelayMs, [safe, ticket, listIndex] {
            if( safe != nullptr && safe->auditionDebounce.isCurrent( ticket ) )
                safe->auditionListIndex( listIndex );
        } );
    }

    void AssignSampleContent::auditionListIndex( int listIndex )
    {
        const auto poolIndex = assign_logic::poolIndexFromListIndex( listIndex, (int)files.size() - 1 );
        if( poolIndex < 0 )
        {
            ae.stopPreview();
            return;
        }
        const juce::File file( ae.getAsset( mt::AssetTypes::Samples, poolIndex ) );
        if( file.existsAsFile() )
            audition( file );
        else
        {
            kick_log( "AssignSample", "sample not found for audition: %s", file.getFullPathName().toRawUTF8() );
            ae.stopPreview();
        }
    }

    void AssignSampleContent::assignSampleIntoPad( int padIndex )
    {
        const auto* programData = ae.getSelectedProgramData();
        if( programData == nullptr )
        {
            kick_log( "AssignSample", "no selected program" );
            return;
        }
        const mt::MTProgramType programType = programData->type;
        if( !assign_logic::programTypeAcceptsSamples( programType == mt::MTProgramType::Drum, programType == mt::MTProgramType::Clip, programType == mt::MTProgramType::Keygroup ) )
        {
            kick_log( "AssignSample", "program type has no pad samples" );
            return;
        }

        const auto hwIndex = assign_logic::hwPadIndex( padIndex, pads->getBankOffset(), _NUM_PADS_IN_COMPONENT );
        if( hwIndex < 0 ) // no pad highlighted / pad outside the visible bank: nothing to assign
            return;

        const auto sampleIndex = fileList.getSelectedIndex();
        if( sampleIndex == 0 )
        {
            // "Clear Assignment": clears this layer only (undoable); the pad's levels, envelope, LFO and the other layers stay.
            ae.clearSampleLayerInProgram( ae.getSelectedProgram(), padIndex, currentLayer );
        }
        else
        {
            const auto poolIndex = assign_logic::poolIndexFromListIndex( sampleIndex, (int)files.size() - 1 );
            if( poolIndex < 0 )
                return;
            const auto sample = ae.getAsset( mt::AssetTypes::Samples, poolIndex );
            ae.addSampleToProgram( ae.getSelectedProgram(), padIndex, sample, 0, currentLayer );
        }
        juce::MessageManager::callAsync( [safe = juce::Component::SafePointer<AssignSampleContent>( this ), padIndex] {
            if( safe == nullptr )
                return;
            safe->refresh();
            safe->pads->setHighlight( padIndex, ui::Theme::padButtonColour( 3 ) );
            safe->refreshLEDs(); // after the highlight is set: the hardware LEDs follow assign / clear without a bank change
        } );
    }

    void AssignSampleContent::paint( juce::Graphics& g )
    {
        g.fillAll( ui::Theme::backgroundColour( 5 ) );
        mt::utils::fillRoundedRect( g, { 10, 10, 342, 440 }, ui::Theme::backgroundColour( 1 ), 6.f );
        mt::utils::renderRoundedRect( g, { 10, 10, 342, 440 }, ui::Theme::backgroundColour( 0 ), 1, 6.f );
    }

    void AssignSampleContent::resized()
    {
        sampleLabel.setBounds( 10, 10, 342, 35 );
        fileList.setBounds( 10, 50, 342, 255 );
        auditionToggle.setBounds( 10, 312, 75, 36 );
        auditionGain.setBounds( 90, 312, 95, 36 );
        deleteSampleButton.setBounds( 190, 312, 162, 36 );
        layerSelector.setBounds( 10, 354, 342, 40 );
        assignButton.setBounds( 10, 400, 166, 40 );
        clearPadButton.setBounds( 186, 400, 166, 40 );
        pads->setBounds( 362, 10, 652, 440 );
    }

    void AssignSampleContent::confirmClearEntirePad( int padIndex )
    {
        if( padIndex < 0 )
            return;
        juce::String padName = "Pad " + juce::String( padIndex + 1 );
        auto* p = new ui::FilePopup( "Clear entire " + padName + " (all layers and settings)?" );
        mt::utils::showDialog(
            "CONFIRM CLEAR", p, [this, p, padIndex]( int R ) {
                if( R == UI::APPLY )
                {
                    ae.removeSampleFromProgram( ae.getSelectedProgram(), padIndex );
                    refresh();
                    pads->setHighlight( padIndex, ui::Theme::padButtonColour( 3 ) );
                    refreshLEDs();
                    updateLabelText();
                }
                delete p;
            },
            ui::layout::warningPopup );
    }

    void AssignSampleContent::confirmDeleteSample()
    {
        const auto sel = fileList.getSelectedIndex();
        const auto poolIndex = assign_logic::poolIndexFromListIndex( sel, (int)files.size() - 1 );
        if( poolIndex < 0 )
            return;
        const juce::String samplePath = ae.getAsset( mt::AssetTypes::Samples, poolIndex );
        const juce::String sampleName = juce::File( samplePath ).getFileName();

        // Check if sample is used by any layer of any pad in the current program
        std::vector<std::string> sources;
        for( auto* s : samples )
        {
            if( s == nullptr )
                continue;
            for( const auto& src : s->getSources() )
                if( src.isNotEmpty() )
                    sources.push_back( src.toStdString() );
        }
        const bool used = assign_logic::isSampleUsed( samplePath.toStdString(), sources );
        const juce::String prompt = used ? ( "WARNING: '" + sampleName + "' is used by pads in this program! Delete from pool anyway?" )
                                         : ( "Delete '" + sampleName + "' from project pool?" );

        auto* p = new ui::FilePopup( prompt );
        mt::utils::showDialog(
            "DELETE SAMPLE", p, [this, p, samplePath]( int R ) {
                if( R == UI::APPLY )
                {
                    ae.stopPreview();
                    ae.removeAsset( samplePath, mt::AssetTypes::Samples );
                    files = ae.getAssetNames( mt::AssetTypes::Samples );
                    files.insert( files.begin(), "Clear Assignment" );
                    fileList.setContent( files, 0 );
                    refresh();
                    refreshLEDs();
                    updateLabelText();
                }
                delete p;
            },
            ui::layout::warningPopup );
    }

    void AssignSampleContent::updateLabelText()
    {
        juce::String text = "Samples";
        const int pad = pads->getHighlightedPad();
        if( pad >= 0 )
        {
            int letterAscii = 'A' + ( pad / 16 );
            int padNum = ( pad % 16 ) + 1;
            text = juce::String::formatted( "Pad %c%02d [L%d]", letterAscii, padNum, currentLayer + 1 );
        }
        const int sel = fileList.getSelectedIndex();
        if( sel == 0 )
            text += " | Action: Clear Assignment";
        else if( sel > 0 && sel < (int)files.size() )
            text += " | " + files[(size_t)sel];
        sampleLabel.setText( text, juce::dontSendNotification );
    }

    void AssignSampleContent::mouseDown( const juce::MouseEvent& )
    {
    }

    void AssignSampleContent::mouseDrag( const juce::MouseEvent& e )
    {
        if( fileList.getBounds().contains( e.getMouseDownPosition() ) )
        {
            const auto pt = e.getPosition();
            if( pads->getBounds().contains( pt ) )
            {
                const int pad = assign_logic::padIndexFromGridPoint( pt.getX(), pt.getY(),
                                                                     pads->getX(), pads->getY(),
                                                                     pads->getWidth(), pads->getHeight(),
                                                                     pads->getBankOffset() );
                if( pad >= 0 )
                    pads->setHighlight( pad, ui::Theme::padButtonColour( 2 ) );
            }
        }
    }

    void AssignSampleContent::mouseUp( const juce::MouseEvent& e )
    {
        if( fileList.getBounds().contains( e.getMouseDownPosition() ) )
        {
            const auto pt = e.getPosition();
            if( pads->getBounds().contains( pt ) )
            {
                const int pad = assign_logic::padIndexFromGridPoint( pt.getX(), pt.getY(),
                                                                     pads->getX(), pads->getY(),
                                                                     pads->getWidth(), pads->getHeight(),
                                                                     pads->getBankOffset() );
                if( pad >= 0 )
                {
                    pads->setHighlight( pad, ui::Theme::padButtonColour( 3 ) );
                    assignSampleIntoPad( pad );
                }
            }
        }
    }

    void AssignSampleContent::visibilityChanged()
    {
        bool visible = isVisible();
        if( visible )
        {
            files.clear();
            files = mt::getAudioInterface().getAssetNames( mt::AssetTypes::Samples );
            files.insert( files.begin(), "Clear Assignment" );
            fileList.setContent( files, fileList.getSelectedIndex() );
            fileList.refreshViewPosition();
            pads->setBank( ae.getBank() );
            refresh();
            refreshLEDs();
            ui::fileUtils::lastPressedPad = -1;
            updateLabelText();
        }
        else
            ae.stopPreview();

        for( auto& it : getChildren() )
            it->setVisible( visible );
    }

    void AssignSampleContent::refresh()
    {
        pads->clearAllPadData();
        pads->setActive( -1, true );
        pads->setBorderColor( ui::Theme::backgroundColour( 0 ), 1, juce::dontSendNotification );

        auto progIdx  = ae.getSelectedProgram();
        auto progType = ae.getProgramType( progIdx );

        samples.clear();
        ae.getProgramSamples( progIdx, samples );

        for( const auto& sample : samples )
        {
            if( sample )
            {
                auto text = juce::File( sample->layers[(size_t)currentLayer]->source.get() ).getFileNameWithoutExtension();
                const bool l0 = sample->layers.size() > 0 && sample->layers[0]->source.get().isNotEmpty();
                const bool l1 = sample->layers.size() > 1 && sample->layers[1]->source.get().isNotEmpty();
                const bool l2 = sample->layers.size() > 2 && sample->layers[2]->source.get().isNotEmpty();
                const bool l3 = sample->layers.size() > 3 && sample->layers[3]->source.get().isNotEmpty();
                const juce::String layerInd = assign_logic::formatLayerIndicator( l0, l1, l2, l3 );

                // Keygroup: label the zone minNote..maxNote; Drum / Clip: the key note only.
                const auto range = assign_logic::zoneNotes( sample->minNote.get(), sample->maxNote.get(), sample->keyNote.get() );
                for( auto i = range.first; i <= range.last; i++ )
                {
                    if( !text.isEmpty() )
                    {
                        pads->setRow1Text( i, text, juce::dontSendNotification );
                        pads->setBorderColor( i, ui::Theme::backgroundColour( 6 ), 1, juce::dontSendNotification );
                    }
                    if( !layerInd.isEmpty() )
                    {
                        pads->setRow2Text( i, layerInd, juce::dontSendNotification );
                        pads->setRow2Colour( ui::Theme::textColour( 2 ) );
                    }
                }
            }
        }
    }

    void AssignSampleContent::refreshLEDs()
    {
        using DCC = contracts::DeviceControlContracts;
        std::vector<contracts::PadData> padDataList;
        const auto                      bankOffset = pads->getBankOffset();

        ae.getProgramSamples( ae.getSelectedProgram(), samples );
        if( ae.getSelectedProgramData() )
        {
            // Zone-aware for every program type: a Keygroup pad is lit only when a sample zone covers its note.
            std::vector<assign_logic::ZoneInfo> zones;
            for( auto* it : samples )
            {
                if( it == nullptr )
                    continue;
                bool any = false;
                for( const auto& src : it->getSources() )
                    any = any || src.isNotEmpty();
                zones.push_back( { it->minNote.get(), it->maxNote.get(), it->keyNote.get(), any } );
            }
            for( int hw = 0; hw < NUM_PADS_IN_BANK; hw++ )
            {
                if( assign_logic::padAssigned( zones, bankOffset, hw ) )
                    padDataList.push_back( { hw, DCC::PadButtonState::StateSampleAssigned, DCC::padColourDarkYellow } );
                else
                    padDataList.push_back( { hw, DCC::PadButtonState::StateSampleNotAssigned, DCC::PadColourTrans } );
            }
        }
        // The highlighted pad is part of the same full-bank colour message (the bridge only processes that message type on the
        // MPC; single-pad commands are dropped there), so the highlight survives assign / clear without a bank change.
        const auto highlightedPad = pads->getHighlightedPad();
        const auto highlightHw    = assign_logic::hwPadIndex( highlightedPad, bankOffset, _NUM_PADS_IN_COMPONENT );
        if( highlightHw >= 0 )
        {
            for( auto& pd : padDataList )
                if( pd.colourIndex == highlightHw )
                    pd = { highlightHw, DCC::PadButtonState::StateOn, DCC::padColourlightGreen1 };
            ui::fileUtils::lastPressedPad = highlightHw;
        }
        contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::setPadColourAndStateInfoMessage( padDataList ) );
    }
    //Not used
    void AssignSampleContent::sendAssignedSamplesAtStartup( void )
    {
        refresh();
        auto bankOffset = pads->getBankOffset();
        for( auto& it : samples )
        {
            auto index = (int)it->keyNote.get() - bankOffset;
            if( juce::isPositiveAndBelow( index, 16 ) && it->getSource().length() )
            {
                DBG( "Assign Sample Start Up" );
                contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::
                                                             setPadDataMessage( index, contracts::DeviceControlContracts::PadButtonState::StateOn, contracts::DeviceControlContracts::PadColourTrans ) );
            }
        }
    }

    void AssignSampleContent::audition( juce::File f )
    {
        ae.stopPreview();
        if( f.existsAsFile() )
            ae.createPreview( f, mt::getSettings().preview.gain );
    }

    void AssignSampleContent::handleCommand( CPktMgr::HWMsg_t& f_HwMsg )
    {
        auto padIndex = pads->getHighlightedPad();
        auto ledIndex = padIndex - pads->getBankOffset();
        if( contracts::DclContractsHelper::isButtonMessage( f_HwMsg ) )
        {
            auto buttonCode  = contracts::DclContractsHelper::getButtonCode( f_HwMsg );
            auto buttonState = contracts::DclContractsHelper::getButtonState( f_HwMsg );
            if( buttonCode == contracts::DeviceControlContracts::EN_ERASE )
            {
                if( buttonState == contracts::DeviceControlContracts::BTN_PRESSED )
                {
                    const bool isShift = contracts::DclContractsHelper::isShiftPressed();
                    if( isShift && padIndex >= 0 )
                    {
                        confirmClearEntirePad( padIndex );
                    }
                    else if( ledIndex >= 0 && assign_logic::hwPadIndex( padIndex, pads->getBankOffset(), _NUM_PADS_IN_COMPONENT ) >= 0 )
                    {
                        // ERASE clears the highlighted pad's current layer (undoable); without a highlighted pad it does nothing.
                        ae.clearSampleLayerInProgram( ae.getSelectedProgram(), padIndex, currentLayer );
                        refresh();
                        pads->setHighlight( padIndex, ui::Theme::padButtonColour( 3 ) );
                        refreshLEDs();
                        updateLabelText();
                    }

                    contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::setLEDONMessage( contracts::DeviceControlContracts::EN_ERASE ) );

                    refresh();
                    if( padIndex >= 0 )
                        pads->setHighlight( padIndex, ui::Theme::padButtonColour( 3 ) ); //highlight UI pad
                }
                else if( buttonState == contracts::DeviceControlContracts::BTN_RELEASED )
                {
                    contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::setLEDOFFMessage( contracts::DeviceControlContracts::EN_ERASE ) );
                }
            }
            else if( buttonCode == contracts::DeviceControlContracts::EN_KNOB_ENTER && buttonState == contracts::DeviceControlContracts::BTN_PRESSED )
            {
                assignSampleIntoPad( padIndex );
                ui::fileUtils::setHighlightLED( padIndex );
            }
            else
            {
                auto& aeSel = ae.getSelected();
                switch( buttonCode )
                {
                    case contracts::DeviceControlContracts::EN_PLAY: ae.setSequencePlay( aeSel.sequence, false, true ); break;
                    case contracts::DeviceControlContracts::EN_PLAYSTART: ae.setSequencePlay( aeSel.sequence, true, true ); break;
                    case contracts::DeviceControlContracts::EN_STOP: ae.setSequencePause(); break;
                }
            }
        }
        else if( contracts::DclContractsHelper::isKnobMessage( f_HwMsg ) )
        {
            if( contracts::DclContractsHelper::getKnobCode( f_HwMsg ) == contracts::DeviceControlContracts::EN_KNOB_CENTER )
            {
                fileList.knobRotated( contracts::DclContractsHelper::getKnobValue( f_HwMsg ) );
                auto selectedIndex = juce::jlimit( 0, fileList.getSize() - 1, fileList.getSelectedIndex() + contracts::DclContractsHelper::getKnobValue( f_HwMsg ) );
                fileList.setSelected( selectedIndex, juce::sendNotification );
            }
        }
        else if( contracts::DclContractsHelper::isPadButtonMessage( f_HwMsg ) )
        {
            if( auto prog = ae.getProgram( ae.getSelected().program ) )
            {
                auto bankOffset = ae.getBankOffset();
                auto padNo      = bankOffset + contracts::DclContractsHelper::getPadNumber( f_HwMsg );
                auto padState   = contracts::DclContractsHelper::getPadState( f_HwMsg );
                auto velocity   = contracts::DclContractsHelper::getPadVelocity( f_HwMsg );

                if( padState == contracts::DeviceControlContracts::BTN_PRESSED )
                    prog->noteOn( padNo, velocity );
                else if( padState == contracts::DeviceControlContracts::BTN_RELEASED )
                {
                    switch( prog->getType() )
                    {
                        default: prog->noteOff( padNo ); break;
                        case mt::MTProgramType::Clip: break;
                    }
                }
            }
        }
        else if( auto* focused = ui::getComponentInFocus() ) // no focused control (e.g. strip message): ignore
            focused->handleCommand( f_HwMsg );
    }
#if NOT_USED
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    DrumAssignSampleContent::DrumAssignSampleContent()
        : ae( mt::getAudioInterface() )
        , pads( std::make_unique<ui::KickPadButtonGroupComponent>( ui::KickPadButton::PadButtonType::assignSample ) )
    {
        setLookAndFeel( ui::KickLooknFeel::getLnf() );

        mt::utils::addChildComponents( this, pads.get(), pads->getBankComponent() );

        fileList.setBackgroundColour( ui::Theme::backgroundColour( 4 ) );

        pads->OnButtonClicked = [this]( int Id ) { //software pad button clicked
            auto bankOffset = pads->getBankOffset();
            fileList.setContent( ( files ), 0 );
            fileList.show( "Pad - " + juce::String( Id ), [this, Id]( int R ) {
                if( ( R == UI::APPLY ) || ( 0 <= R ) )
                {
                    auto index = fileList.getSelectedIndex();
                    if( 0 == index )
                    {
                        ae.removeSampleFromProgram( ae.getSelectedProgram(), Id );
                        contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::
                                                                     setPadDataMessage( index, contracts::DeviceControlContracts::PadButtonState::StateOff ) );
                    }
                    else
                    {
                        auto sample = mt::getAudioInterface().getAsset( mt::AssetTypes::Samples, index - 1 );
                        ae.addSampleToProgram( ae.getSelectedProgram(), Id, sample, 0 );
                    }
                    refresh();
                }
            } );
        };

        pads->OnBankChanged = [this]( juce::String Id ) {
            refreshLEDs();
        };
    }

    void DrumAssignSampleContent::refresh()
    {
        pads->clearAllPadData();
        pads->setActive( -1, true );
        pads->setBorderColor( ui::Theme::selectionColour( 1 ), 2.5f, juce::dontSendNotification );

        auto progIdx  = ae.getSelectedProgram();
        auto progType = ae.getProgramType( progIdx );

        switch( progType )
        {
            case mt::MTProgramType::Drum:
                pads->setPrefix( "DR" );
                break;
            case mt::MTProgramType::Keygroup:
                pads->setPrefix( "KG" );
                break;
            case mt::MTProgramType::Audio:
                pads->setPrefix( "AU" );
                break;
            case mt::MTProgramType::Clip:
                pads->setPrefix( "CL" );
                break;
            default:
            case mt::MTProgramType::Unknown:
            case mt::MTProgramType::Plugin:
            case mt::MTProgramType::Cv:
            case mt::MTProgramType::Midi:
                break;
        }

        samples.clear();
        ae.getProgramSamples( progIdx, samples );
        for( const auto& sample : samples )
        {
            auto text = juce::File( sample.sources[0] ).getFileNameWithoutExtension();
            pads->setRow1Text( sample.keyNote, text, juce::dontSendNotification );
        }

        refreshLEDs();
    }

    void DrumAssignSampleContent::refreshLEDs()
    {
        auto bankOffset = pads->getBankOffset();
        //DBG( "Get BankOFfset -> " << bankOffset );
        // Clear all pad LED's.
        contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::clearAllPadsMessage() );
        //Controls::InputEvents::clearAllPads();
        // Set required pad LED's
        for( auto& it : samples )
        {
            auto index = it.keyNote - bankOffset;
            if( juce::isPositiveAndBelow( index, 16 ) && it.sources[0].length() )
            {
                contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::
                                                             setPadDataMessage( index, contracts::DeviceControlContracts::PadButtonState::StateOn, contracts::DeviceControlContracts::PadColourOrange ) );
            }
        }
    }

    void DrumAssignSampleContent::sendAssignedSamplesAtStartup( void )
    {
        refresh();
        auto bankOffset = pads->getBankOffset();
        for( auto& it : samples )
        {
            auto index = it.keyNote - bankOffset;
            if( juce::isPositiveAndBelow( index, 16 ) && it.sources[0].length() )
            {
                DBG( "Assign Sample Start Up" );
                contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::
                                                             setPadDataMessage( index, contracts::DeviceControlContracts::PadButtonState::StateOn, contracts::DeviceControlContracts::PadColourTrans ) );
            }
        }
    }

    void DrumAssignSampleContent::visibilityChanged()
    {
        bool visible = isVisible();
        if( visible )
        {
            files = mt::getAudioInterface().getAssetNames( mt::AssetTypes::Samples );
            files.insert( files.begin(), "Clear Assignment" );

            refresh();
        }

        for( auto& it : getChildren() )
            it->setVisible( visible );
    }

    void DrumAssignSampleContent::resized()
    {
        pads->setBoundsRelative( .02f, .02f, 0.96f, 0.90f );
        pads->getBankComponent().setBoundsRelative( .60f, .93f, 0.35f, 0.06f );
    }

    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ///
    ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    ///
    KeysAssignSampleContent::KeysAssignSampleContent()
        : ae( mt::getAudioInterface() )
    {
        setLookAndFeel( ui::KickLooknFeel::getLnf() );
        mt::utils::addChildComponents( this, left, right, bottom );

        left.provider.numRows = [&]() {
            return (int)ae.getAssets().size();
        };
        right.provider.numRows = [&]() {
            return numSamples;
        };

        left.provider.cellComponent = [&]( int Row, int Col, int IsSelected, Component* Existing ) {
            auto comp = static_cast<ui::KickLabel*>( Existing );
            if( juce::isPositiveAndBelow( Row, ae.getAssets().size() ) )
            {
                if( comp == nullptr )
                {
                    comp = new ui::KickLabel( "" );
                    comp->setInterceptsMouseClicks( false, false );
                }
                if( 1 == Col ) comp->setText( juce::String( Row + 1 ), juce::dontSendNotification );
                if( 2 == Col ) comp->setText( juce::File( ae.getAssets()[(size_t)Row] ).getFileName(), juce::dontSendNotification );
                comp->setColours( IsSelected ? ui::Theme::textColour( 2 ) : ui::Theme::textColour( 1 ), ui::Theme::backgroundColour( 9 ) );
            }
            return comp;
        };

        right.provider.cellComponent = [&]( int Row, int Col, int IsSelected, Component* Existing ) {
            auto comp = static_cast<ui::KickLabel*>( Existing );
            if( juce::isPositiveAndBelow( Row, numSamples ) )
            {
                if( comp == nullptr )
                {
                    comp = new ui::KickLabel( "" );
                    comp->setInterceptsMouseClicks( false, false );
                }

                if( 1 == Col ) comp->setText( juce::String( Row + 1 ), juce::dontSendNotification );
                if( 2 == Col ) comp->setText( juce::File( samples[Row].sources[0] ).getFileName(), juce::dontSendNotification );

                comp->setColours( IsSelected ? ui::Theme::textColour( 2 ) : ui::Theme::textColour( 1 ), ui::Theme::backgroundColour( 9 ) );
            }
            return comp;
        };

        left.table->onRowSelected = [&]( int Row ) {
            selectedAsset = ae.getAssets()[(size_t)Row];
        };
        right.table->onRowSelected = [&]( int Row ) {
            if( juce::isPositiveAndBelow( Row, samples.size() ) ) selectedRow = Row;
        };

        bottom.clear.onClick = [&]() {
            if( juce::isPositiveAndBelow( selectedRow, samples.size() ) )
            {
                ae.removeSampleFromProgram( ae.getSelectedProgram(), samples[selectedRow].keyNote, samples[selectedRow].sources[0] );
                refreshSampleList();
                right.table->refresh();
            }
        };

        bottom.load.onClick = [&]() {
            auto progNum = ae.getSelectedProgram();
            auto idx     = ae.getNumProgramSamples( progNum );
            ae.addSampleToProgram( progNum, idx, selectedAsset, 0 );
            refreshSampleList();
            right.table->refresh();
        };

        bottom.audition.onClick = [&]() {
            ae.createPreview( { selectedAsset }, 0.0f );
        };
    }

    void KeysAssignSampleContent::paint( juce::Graphics& g )
    {
        g.fillAll( ui::Theme::backgroundColour( 5 ) );
    }

    void KeysAssignSampleContent::refreshSampleList()
    {
        ae.getProgramSamples( ae.getSelectedProgram(), samples );
        numSamples = ae.getNumProgramSamples( ae.getSelectedProgram() );
    }

    void KeysAssignSampleContent::visibilityChanged()
    {
        bool visible = isVisible();
        if( visible )
        {
            refreshSampleList();
            left.table->refresh();
            right.table->refresh();
        }
        else
        {
            samples.clear();
            numSamples = 0;
            ae.stopPreview();
        }

        for( auto it : getChildren() )
            it->setVisible( visible );
    }

    void KeysAssignSampleContent::resized()
    {
        left.setBoundsRelative( .0f, .0f, 0.35f, .9f );
        right.setBoundsRelative( .35f, .0f, 0.65f, .9f );
        bottom.setBoundsRelative( .0f, .9f, 1.0f, .090f );
    }

    /// ///
    SamplesList::SamplesList()
    {
        setLookAndFeel( ui::KickLooknFeel::getLnf() );
        table.reset( new ui::KickTable( "List", provider ) );

        addAndMakeVisible( *table );
        table->addColumn( { "#", 1, 30 } );
        table->addColumn( { "Name", 2, 280 } );
        table->setHeight( 30, 30 );
        table->setRowBackgroundColour( juce::Colours::transparentBlack );
    }

    void SamplesList::paint( juce::Graphics& g )
    {
        juce::Path p;
        auto       b = getLocalBounds().withTrimmedLeft( 10 );
        p.addRoundedRectangle( (float)b.getX(), (float)b.getY(), (float)b.getWidth(), (float)b.getHeight(), 5.0f, 5.0f, true, false, true, false );
        g.setColour( ui::Theme::backgroundColour( 2 ) );
        g.fillPath( p );
    }

    void SamplesList::resized()
    {
        table->setBoundsRelative( .02f, .02f, .96f, .96f );
    }

    KeysPanel::KeysPanel()
    {
        setLookAndFeel( ui::KickLooknFeel::getLnf() );
        table.reset( new ui::KickTable( "List", provider ) );

        addAndMakeVisible( *table );
        table->addColumn( { "#", 1, 30 } );
        table->addColumn( { "Name", 2, 400 } );
        table->addColumn( { "Min", 3, 60 } );
        table->addColumn( { "Root", 4, 60 } );
        table->addColumn( { "Max", 5, 60 } );
        table->setHeight( 30, 40 );
        table->setRowBackgroundColour( juce::Colours::transparentBlack );
    }

    void KeysPanel::paint( juce::Graphics& g )
    {
        juce::Path p;
        auto       b = getLocalBounds().withTrimmedRight( 10 );
        p.addRoundedRectangle( (float)b.getX(), (float)b.getY(), (float)b.getWidth(), (float)b.getHeight(), 5.0f, 5.0f, false, true, false, true );
        g.setColour( ui::Theme::backgroundColour( 4 ) );
        g.fillPath( p );
    }

    void KeysPanel::resized()
    {
        table->setBoundsRelative( .02f, .02f, .96f, .96f );
    }

    KeysBottomControls::KeysBottomControls()
    {
        mt::utils::addAndMakeVisible( this, audition, clear, load );
    }

    void KeysBottomControls::paint( juce::Graphics& g )
    {
        g.setColour( ui::Theme::backgroundColour( 3 ) );
    }
    void KeysBottomControls::resized()
    {
        auto y          = (float)( 10 ) / (float)getLocalBounds().getHeight();
        auto bottomBtnH = 1.0f - ( 2 * y );
        audition.setBoundsRelative( .61f, y, .11f, bottomBtnH );
        clear.setBoundsRelative( .74f, y, .11f, bottomBtnH );
        load.setBoundsRelative( .87f, y, .11f, bottomBtnH );
    }

    void KeysBottomControls::visibilityChanged() {}
    ///
#endif
    /// @brief
    /// Class AppAssignSample...
    ///
    AppAssignSample::AppAssignSample()
        : kick_ui::IScreen( kick_ui::Screen::MenuAssignSample )
    {
        setLookAndFeel( ui::KickLooknFeel::getLnf() );

        mt::utils::addChildComponents( this, header, trackSelector, drum, bankLabel, leftNav, rightNav, fileBrowser );
        mt::utils::makeHidden( drum );

        header.onBackButtonPressed = [this]( bool ) { closeScreen(); };

        header.showOptions( false, false );

        bankLabel.setText( drum.pads->getBank(), juce::dontSendNotification );
        bankLabel.setTextColour( ui::Theme::textColour( 1 ) );
        bankLabel.setValueFontHeight( 24 );
        bankLabel.setValueBackgroundColour( ui::Theme::backgroundColour( 3 ) );
        bankLabel.setValueJustification( juce::Justification::centred );

        leftNav.onClick = [&]() {
            auto index = drum.pads->getBankIndex();
            if( index == 0 )
                return; // already at bank A
            drum.pads->setBank( index - 1 );
            bankLabel.setText( drum.pads->getBank(), juce::dontSendNotification );
            mt::getAudioInterface().setBank( drum.pads->getBank() );
        };

        rightNav.onClick = [&] {
            auto index = drum.pads->getBankIndex();
            if( index == 7 )
                return; // already at bank H
            drum.pads->setBank( index + 1 );
            bankLabel.setText( drum.pads->getBank(), juce::dontSendNotification );
            mt::getAudioInterface().setBank( drum.pads->getBank() );
        };

        fileBrowser.onClick = [&]() {
            kick_ui::signals::sigScreenOpen( kick_ui::Screen::MenuFileBrowser );
        };

        ui::themeChanged.connect( [&]() {
            leftNav.setImages( "Icon_minus" );
            rightNav.setImages( "Icon_plus" );
            header.setIcon( "sEdit_Icon" );
        } );

        mt::signals::bankChanged.connect( [&]( int Index, juce::String Bank ) {
            setBank( Index, Bank );
        } );
    }

    void AppAssignSample::resized()
    {
        auto b = getLocalBounds();
        header.setBounds( b.removeFromTop( ui::layout::header.getHeight() ) );

        // Childrens...
        drum.setBounds( b );
        trackSelector.setBoundsRelative( .38f, .034f, .18f, .09f );
        fileBrowser.setBoundsRelative( .6892f, .04f, .0429f, .08f );

        bankLabel.setBoundsRelative( .74f, .038f, .19f, .085f );
        leftNav.setBoundsRelative( .74f, .034f, .043f, .09f );
        rightNav.setBoundsRelative( .9f, .034f, .043f, .09f );
    }
    void AppAssignSample::visibilityChanged()
    {
        auto& ae = mt::getAudioInterface();
        for( auto it : getChildren() )
            it->setVisible( isVisible() );
        if( isVisible() )
        {
            contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::
                                                         setPadColorModeMessage( mt::PadColorMode::DISABLE_PAD_COLOUR_MODE ) );
            bankLabel.setText( ae.getBank(), juce::dontSendNotification );
        }
        else
        {
            auto MTIProg = ae.getProgramData( ae.getSelectedProgram() );
            contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::
                                                         setPadColorModeMessage( MTIProg ? MTIProg->colourMode.get() : mt::OFF ) );

            contracts::DclSignals::refreshPadLeds();
        }
    }

    void AppAssignSample::handleCommand( CPktMgr::HWMsg_t& f_HwMsg )
    {
        drum.handleCommand( f_HwMsg );
    }

    void AppAssignSample::setBank( int Index, juce::String Bank )
    {
        if( kick_ui ::KickHome::Instance().getCurrentScreen() == kick_ui::Screen::MenuAssignSample )
        {
            juce::MessageManager::callAsync( [this, Index, Bank] {
                bankLabel.setText( Bank, juce::dontSendNotification );
                ui::fileUtils::lastPressedPad = -1;
                drum.pads->setBank( Index );
                drum.refreshLEDs();
            } );
        }
    }
}
