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

namespace kick_ui
{
    Looper::Looper()
        : kick_ui::IScreen( kick_ui::Screen::MenuLooper )
    {
        options.showPanSlider( false );
        options.showRecordButton( false );
        options.showOutputs( false );

        options.setInputs( { "IN L - OUT MONO", "IN R - OUT MONO", "IN L+R - OUT MONO", "IN L - OUT STEREO", "IN R - OUT STEREO", "IN L+R - OUT STEREO" }, 5 );

        clearBtn.setEnabled( false );
        exportBtn.setEnabled( false );

        mt::utils::addChildComponents( this, header, options, recorder, exportBtn, clearBtn, undoBtn );

        header.onBackButtonPressed = [this]( bool ) { closeScreen(); };

        // Recorder buttons...
        // Export / Clear follow the engine state (the recorder reports it), not the screen buttons: a loop recorded with
        // the hardware REC / STOP counts as well.
        recorder.recordingStarted.connect( [this]( bool value ) {
            clearBtn.setEnabled( value );
            exportBtn.setEnabled( value );
        } );
        undoBtn.setValueColour( ui::Theme::textColour( 1 ) );
        undoBtn.setValueFontHeight( 16 );

        // Bottom panel
        exportBtn.onClick = [&]() {
            auto p = new KeepDiscardSampleComponent();
            mt::utils::showOptions( "EXPORT", p, [p, this]( int R ) {
                if( R == UI::BUTTONS::APPLY )
                    exportLoop( p->getSampleName(), p->getSelectedProgram(), p->getPadIndex() );
                if( p ) delete p;
                exportBtn.setToggleState( false, juce::dontSendNotification );
            } );
        };

        clearBtn.onClick = [&]() {
            clearBtn.setToggleState( false, juce::dontSendNotification );
            auto p = new ui::FilePopup( "Clear the recorded loop?" );
            mt::utils::showDialog(
                "WARNING!", p, [p, this]( int R ) {
                    if( R == UI::APPLY )
                    {
                        recorder.clearRecording();
                        mt::utils::setEnabled( false, exportBtn, clearBtn, undoBtn );
                    }
                    delete p;
                },
                ui::layout::warningPopup );
        };

        // Looper-level undo (and redo: a second press swaps back).
        undoBtn.onClick = [&]() { recorder.undoRecording(); };

        options.onInsertsClicked = [&]() {
            kick_ui::signals::onScreenData( kick_ui::Screen::Inserts, (void*)( dynamic_cast<mt::MTIInsertsInterface*>( mt::getAudioInterface().getLooperInsertsInterface() ) ) );
            kick_ui::signals::sigScreenOpen( kick_ui::Screen::Inserts );
        };

        ui::themeChanged.connect( [&]() {
            exportBtn.setImages( "Song_Export", "Song_Export_Active" );
            clearBtn.setImages( "Song_Clear", "Song_Clear_Active" );
            header.setIcon( "Icon_Looper" );
        } );
    }

    void Looper::paint( juce::Graphics& g )
    {
        g.setColour( ui::Theme::backgroundColour( 1 ) );
        g.fillRoundedRectangle( options.getBounds().toFloat(), 5.0f );
        g.fillRoundedRectangle( recorder.getBounds().toFloat(), 5.0f );
    }

    void Looper::resized()
    {
        auto b = getLocalBounds();
        // Set header bar...
        header.setBounds( b.removeFromTop( ui::layout::header.getHeight() ) );
        undoBtn.setBoundsRelative( .6450f, .0299f, .0780f, .0823f );
        exportBtn.setBoundsRelative( .7318f, .0299f, .0489f, .0823f );
        clearBtn.setBoundsRelative( .7847f, .0299f, .0489f, .0823f );

        options.setBounds( b.removeFromLeft( 362 ).reduced( 10 ) );
        b.removeFromRight( 10 );
        recorder.setBounds( b.reduced( 0, 10 ) );
    }

    // Saves the loop into <project>/samples under a name that does not exist yet, checks what landed on disk and only then
    // registers it (the old code wrote next to the project folder, with a name that could collide, and ignored every error).
    void Looper::exportLoop( const juce::String& Name, int SelectedProgram, int PadIndex )
    {
        auto&      ae         = mt::getAudioInterface();
        const auto samplesDir = ae.getProjectDir().getChildFile( "samples" );
        auto       fail       = [this]( const juce::String& Message ) {
            kick_log( "Looper", "export failed: %s", Message.toRawUTF8() );
            ui::KickAlertWindow::showCancelAlert( ui::KickAlertWindow::KickAlertWindowType::Warning, Message, "Looper", []( int ) {} );
        };
        if( !samplesDir.createDirectory() )
            return fail( "Cannot create the folder " + samplesDir.getFullPathName() );

        const auto name = sampler_logic::uniqueFileName( Name.toStdString(), ".wav", [&]( const std::string& n ) { return samplesDir.getChildFile( n ).existsAsFile(); } );
        const auto file = samplesDir.getChildFile( name );
        auto       path = file.getFullPathName();

        recorder.exportRecording( path );
        if( !file.existsAsFile() || file.getSize() < 64 )
        {
            file.deleteFile();
            const auto reason = ae.getLooperError();
            return fail( "The loop could not be saved to " + file.getFullPathName() + ( reason.isNotEmpty() ? ( ": " + reason ) : juce::String() ) );
        }

        if( ae.addAsset( path, mt::AssetTypes::Samples ) < 0 )
            return fail( "The saved loop could not be added to the project assets: " + path );

        // "None" is entry 0 of the dialog's list; entry n is program n - 1.
        if( SelectedProgram > 0 )
        {
            const int program = SelectedProgram - 1;
            if( auto* p = ae.getProgram( program ) )
            {
                const auto type = p->getType();
                if( type == mt::MTProgramType::Drum || type == mt::MTProgramType::Keygroup || type == mt::MTProgramType::Clip || type == mt::MTProgramType::Audio )
                    ae.addSampleToProgram( program, PadIndex, path );
            }
        }
    }

    inline void Looper::handleCommand( CPktMgr::HWMsg_t& hwMsg )
    {
        recorder.handleCommand( hwMsg ); // Forward
    }

    void Looper::visibilityChanged()
    {
        auto visible = isVisible();
        for( auto it : getChildren() )
            it->setVisible( visible );
        if( isVisible() )
        {
            std::vector<juce::String> insertList;
            auto                      pEffects = mt::getAudioInterface().getLooperInsertsInterface();
            pEffects->getInserts( insertList );
            options.setInsertsList( insertList );
        }

    }
}
