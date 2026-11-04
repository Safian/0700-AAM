
namespace kick_ui
{
    ProgramEditHeaderComponent::ProgramEditHeaderComponent()
    {
        mt::utils::addChildComponents( this, programs, icon );
        mt::utils::addAndMakeVisible( this, icon );
        icon.setInterceptsMouseClicks( false, false );
    }

    void ProgramEditHeaderComponent::setHeaderSubComponent( juce::Component* Comp )
    {
        if( progHeader )
            removeChildComponent( progHeader );

        progHeader = Comp;
        if( progHeader )
            addAndMakeVisible( progHeader );

        resized();
    }

    void ProgramEditHeaderComponent::resized()
    {
        auto b = getLocalBounds();
        //programs.setBounds( b.removeFromLeft( .66 * b.getWidth() ).reduced( 0, 16 ) );
        programs.setBounds( b.removeFromLeft( .4 * b.getWidth() ).reduced( 0, 16 ) );
        icon.setBounds( b.removeFromLeft( 64 ).reduced( 10, 16 ) );
        if( progHeader )
            progHeader->setBounds( b );
    }

    void ProgramEditHeaderComponent::setIcon( const juce::String& Icon )
    {
        icon.setImages( Icon );
        repaint();
    }

    void ProgramEditHeaderComponent::visibilityChanged()
    {
        for( auto it : getChildren() )
            it->setVisible( isVisible() );

        if( isVisible() )
        {
            auto pProg = mt::getAudioInterface().getSelectedProgramData();
            // Display only: the program is loaded once, by ProgramEdit::visibilityChanged (one source: the track's program).
            programs.setValues( pProg ? pProg->name.get() : "", pProg ? pProg->type.get() : mt::MTProgramType::Unknown, juce::dontSendNotification );
        }
    }

    AudioHeaderTools::AudioHeaderTools()
    {
        // Copy / Paste buttons: no handler and no clipboard behind them, so they are not shown (paste was hidden already).
        mt::utils::addAndMakeVisible( this, bars, selectAll, selectSample, pencil, erase, paste, cut, settings, magnify );
        copy.setVisible( false );
        mt::utils::setRadioGroupID( (int)RadioGroupId::AudioHeader, selectAll, selectSample, pencil, erase, magnify, cut );
        paste.setVisible( false );
        pencil.triggerClick();

        mt::utils::setTitleColour( ui::Theme::textColour( 1 ), bars );
        mt::utils::setTitleFontHeight( 16, bars );
        bars.setTitleProportion( .4f );
    }

    void AudioHeaderTools::resized()
    {
        bars.setBoundsRelative( .0f, .21f, .1574f, .5789f );
        magnify.setBoundsRelative( 0.1829f, .21f, 0.0936f, 0.5789f );
        selectAll.setBoundsRelative( 0.2851f, .21f, 0.0936f, 0.5789f );
        selectSample.setBoundsRelative( 0.3872f, .21f, 0.0936f, 0.5789f );
        pencil.setBoundsRelative( 0.4893f, .21f, 0.0936f, 0.5789f );
        erase.setBoundsRelative( 0.5914f, .21f, 0.0936f, 0.5789f );
        cut.setBoundsRelative( 0.6936f, .21f, 0.0936f, 0.5789f );
        copy.setBoundsRelative( 0.7957f, .21f, 0.0936f, 0.5789f );
        paste.setBoundsRelative( 0.7957f, .21f, 0.0936f, 0.5789f );
        settings.setBoundsRelative( 0.8978f, .21f, 0.0936f, 0.5789f );
    }

    void AudioHeaderTools::paint( juce::Graphics& g )
    {
    }

    ProgramEdit::ProgramEdit()
        : kick_ui::IScreen( kick_ui::Screen::MenuProgramEdit )
    {
        header.addComponent( &hdrComp );

        programs.emplace_back( std::make_unique<CvProgramEdit>() );
        programs.emplace_back( std::make_unique<MidiProgramEdit>() );
        programs.emplace_back( std::make_unique<ClipProgramEdit>() );
        programs.emplace_back( std::make_unique<DrumProgramEdit>() );
        programs.emplace_back( std::make_unique<KeyGroupProgramEdit>() );
        programs.emplace_back( std::make_unique<AudioProgramEdit>() );

        for( const auto& it : programs )
            it->setVisible( false );

        // Add child components...
        mt::utils::addChildComponents( this, selectedPadLabel, header, infoLabel, audioHdrTools, tracks, audioheader, pluginOpen, padSelect );
        padSelect.setVisible( false );
        padSelect.onSelectionChanged = [this]( int Index, const juce::String& ) {
            if( Index >= 0 ) selectPad( Index );
        };
        pluginOpen.onClick = [this]() { openPlugin( mt::getAudioInterface().getSelectedProgram() ); };
        pluginOpen.setVisible( false );
        header.onBackButtonPressed = [this]( bool ) { closeScreen(); };
        audioHdrTools.setVisible( false );
        tracks.setVisible( false );
        header.showOptions( false, false );
        infoLabel.setValueColour( ui::Theme::textColour( 1 ) );
        infoLabel.setValueFontHeight( 24 );
        infoLabel.setValueJustification( juce::Justification::centred );
        selectedPadLabel.setValueColour( ui::Theme::textColour( 1 ) );
        selectedPadLabel.setValueFontHeight( 24 );
        selectedPadLabel.setVisible( false );
        selectedPadLabel.toFront( true );

        hdrComp.programs.onSelectionChanged = [&]( const juce::String& Program, const mt::MTProgramType Type ) {
            auto& ae       = mt::getAudioInterface();
            auto  progType = ae.getProgramType( Program );
            auto  sel      = ae.getProgramIndexByName( Program );

            ae.setSelectedProgram( sel );
            loadProgram( progType, sel );
        };

        tracks.onSelectionChanged = [&]( int Index, const juce::String& Option ) {
            if( Index >= 0 )
            {
                auto& ae    = mt::getAudioInterface();
                auto  pProg = ae.getProgramDataForTrack( ae.getSelectedSequence(), Index );
                if( pProg )
                {
                    if( pProg->type.get() != mt::MTProgramType::Audio )
                    {
                        ae.setSelectedTrack( Index );
                        kick_ui::signals::sigScreenClose( kick_ui::Screen::HomeGridView );
                    }
                    else
                    {
                        ae.setSelectedTrack( Index ); // the grid reads the selected track: keep it with the program
                        ae.setSelectedProgram( ae.getProgramIndexByName( pProg->name.get() ) );
                        loadProgram( pProg->type.get(), ae.getProgramIndexByName( pProg->name.get() ) );
                    }
                }
                else
                {
                    kick_ui::signals::sigScreenClose( kick_ui::Screen::HomeGridView );
                }
            }
        };

        ui::themeChanged.connect( [&]() {
            if( selectedProgram ) // a theme change before the first program was loaded has nothing to set up
                selectedProgram->setupHeader( header );
        } );
    }

    void ProgramEdit::loadProgram( mt::MTProgramType Type, int Index )
    {
        header.removeAllButtons();
        audioHdrTools.setVisible( false );
        if( selectedProgram )
        {
            if( selectedProgram->getType() == mt::MTProgramType::Audio && Type != mt::MTProgramType::Audio )
            {
                auto& headerComp = dynamic_cast<AudioProgramEdit*>( selectedProgram )->getHeaderComponent();
                headerComp.setVisible( false );
            }
            selectedProgram->setVisible( false );
            removeChildComponent( selectedProgram );
            selectedProgram = nullptr;
        }

        pluginMode_ = false;
        pluginOpen.setVisible( false );
        padSelect.setVisible( false );
        switch( Type )
        {
            case mt::MTProgramType::Cv: selectedProgram = programs[0].get(); break;
            case mt::MTProgramType::Midi: selectedProgram = programs[1].get(); break;
            case mt::MTProgramType::Clip: selectedProgram = programs[2].get(); break;
            case mt::MTProgramType::Drum: selectedProgram = programs[3].get(); break;
            case mt::MTProgramType::Keygroup: selectedProgram = programs[4].get(); break;
            case mt::MTProgramType::Audio: selectedProgram = programs[5].get(); break;
        }

        if( selectedProgram )
        {
            audioheader.setVisible( false );
            header.setVisible( true );
            hdrComp.setIcon( getProgramIcon( Type ) );
            selectedProgram->setupHeader( header );
            selectedProgram->setProgram( Index );
            infoLabel.setVisible( false );
            mt::utils::addAndMakeVisible( this, selectedProgram );
            if( Type == mt::MTProgramType::Drum || Type == mt::MTProgramType::Clip )
                selectedPadLabel.setVisible( true );
            else
                selectedPadLabel.setVisible( false );
            padSelect.setVisible( isVisible() && ( Type == mt::MTProgramType::Drum || Type == mt::MTProgramType::Clip || Type == mt::MTProgramType::Keygroup ) );
            padSelect.toFront( false );
            if( Type == mt::MTProgramType::Audio )
            {
                header.setVisible( false );
                audioheader.setVisible( true );
                selectedProgram->setupHeader( audioheader );
                audioheader.onBackButtonPressed = [this]( bool ) { closeScreen(); };
                audioheader.addComponent( &tracks );

                audioHdrTools.setVisible( true );
                header.showOptions( false, false );
                auto& headerComp = dynamic_cast<AudioProgramEdit*>( selectedProgram )->getHeaderComponent();
                mt::utils::addAndMakeVisible( this, headerComp, tracks );

                headerComp.setVisible( true );
                headerComp.setBoundsRelative( .391f, .0f, .4598f, .1423f );
                tracks.setBoundsRelative( .225f, .03f, .1565f, 0.09f );

                auto& ae  = mt::getAudioInterface();
                auto& sel = ae.getSelected();
                tracks.setListOptions( ae.getTrackList( sel.sequence ), -1 );
                tracks.setSelected( mt::getAudioInterface().getSelectedTrack(), juce::dontSendNotification );
            }
            else
            {
                header.toFront( true );
                selectedPadLabel.toFront( true );
            }
        }
        else
        {
            for( const auto& it : programs )
                it->setVisible( false );
            if( Type == mt::MTProgramType::Plugin )
            {
                // A plugin program has no editor page of its own: the CLAP panel is its editor (same one the Program tab opens).
                infoLabel.setText( "Plugin program: edited in the plugin panel", juce::dontSendNotification );
                pluginMode_ = true;
                pluginOpen.setVisible( isVisible() );
                pluginOpen.toFront( false );
                openPlugin( Index );
            }
            else if( Type == mt::MTProgramType::Audio )
                infoLabel.setText( "Program Edit is not available for Audio Program", juce::dontSendNotification );
            else
                infoLabel.setText( "No Programs Available", juce::dontSendNotification );
            infoLabel.setVisible( true );
        }
    }

    void ProgramEdit::openPlugin( int Index )
    {
        auto& ae = mt::getAudioInterface();
        if( auto& open = pluginOpener() )
            open( ae.getProgram( Index ) );
    }

    // Live note of a pad on the selected track. The note-off ends the note of the PRESS (the bank may have changed since).
    void ProgramEdit::padNote( int padNo, bool pressed, int velocity )
    {
        auto& ae = mt::getAudioInterface();
        if( ! juce::isPositiveAndBelow( padNo, 16 ) ) return;
        if( pressed )
        {
            const int bankOffset = ae.getBankOffset();
            selectedPadLabel.setText( ae.getBank() + juce::String::formatted( "%02d", padNo + 1 ), juce::dontSendNotification );
            padSelect.setSelected( juce::jlimit( 0, 127, bankOffset + padNo ), juce::dontSendNotification );
            if( heldPad[padNo].note >= 0 ) // a lost release: end the old note first
                ae.addLiveMidiMsg( heldPad[padNo].target, juce::MidiMessage::noteOff( 1, heldPad[padNo].note ) );
            heldPad[padNo].note   = juce::jlimit( 0, 127, padNo + bankOffset );
            heldPad[padNo].target = ae.getSelected();
            ae.addLiveMidiMsg( heldPad[padNo].target, juce::MidiMessage::noteOn( 1, heldPad[padNo].note, (juce::uint8)juce::jlimit( 1, 127, velocity ) ) );
        }
        else if( heldPad[padNo].note >= 0 )
        {
            ae.addLiveMidiMsg( heldPad[padNo].target, juce::MidiMessage::noteOff( 1, heldPad[padNo].note ) );
            heldPad[padNo].note = -1;
        }
    }

    // The value controls of the showing page in reading order (top to bottom, left to right): rotaries and number boxes.
    static void collectValueControls( juce::Component* parent, juce::Component* root, std::vector<std::pair<juce::Point<int>, ui::KickControlsProcessor*>>& out )
    {
        for( auto* child : parent->getChildren() )
        {
            if( child == nullptr || ! child->isVisible() ) continue;
            const bool isValue = dynamic_cast<ui::KickSlider*>( child ) || dynamic_cast<ui::KickNumberT<int>*>( child ) || dynamic_cast<ui::KickNumberT<float>*>( child )
                                 || dynamic_cast<ui::KickNumberT<double>*>( child ) || dynamic_cast<ui::KickNumberT<juce::int64>*>( child );
            if( isValue && child->isEnabled() )
            {
                if( auto* proc = dynamic_cast<ui::KickControlsProcessor*>( child ) )
                {
                    out.push_back( { root->getLocalPoint( child, juce::Point<int>() ), proc } );
                    continue;
                }
            }
            collectValueControls( child, root, out );
        }
    }

    bool ProgramEdit::qLinkToPage( int knob, int value )
    {
        if( ! selectedProgram || value == 0 ) return false;
        std::vector<std::pair<juce::Point<int>, ui::KickControlsProcessor*>> controls;
        collectValueControls( selectedProgram, this, controls );
        std::stable_sort( controls.begin(), controls.end(), []( const auto& a, const auto& b ) {
            const int ra = a.first.y / 24, rb = b.first.y / 24; // same row within 24 px
            return ra != rb ? ra < rb : a.first.x < b.first.x;
        } );
        if( knob < 0 || knob >= (int)controls.size() ) return false;
        auto* c = controls[(size_t)knob].second;
        c->knobRotated( value * c->getIncrement() );
        return true;
    }

    // Touch selection of a pad: the bank of the pad becomes the engine bank, the page then gets what a hardware pad press gives it.
    void ProgramEdit::selectPad( int padIndex )
    {
        if( ! selectedProgram || ! juce::isPositiveAndBelow( padIndex, 128 ) ) return;
        auto& ae = mt::getAudioInterface();
        if( ae.getBankOffset() / 16 != padIndex / 16 )
            ae.setBank( mt::Kick::padBankNames[(size_t)( padIndex / 16 )] );
        CPktMgr::HWMsg_t msg;
        msg.pktType = CPktMgr::EN_PKT_TYPE_PAD;
        msg.pktCmd  = contracts::DeviceControlContracts::PadCmd::EN_PAD_STATE;
        msg.data    = { padIndex % 16, (int)contracts::DeviceControlContracts::BTN_PRESSED, 0, 0 };
        selectedProgram->handleCommand( msg );
        selectedPadLabel.setText( ae.getBank() + juce::String::formatted( "%02d", padIndex % 16 + 1 ), juce::dontSendNotification );
    }

    void ProgramEdit::releaseHeldPads()
    {
        for( int i = 0; i < 16; ++i )
            padNote( i, false, 0 );
    }

    // Hardware input. Only a real PAD message plays a note: Data Dial / Q-Link turns used to share this branch and were
    // read as pad-number / state / velocity (a turn to the right sounded note-on, to the left note-off).
    void ProgramEdit::handleCommand( CPktMgr::HWMsg_t& f_HwMsg )
    {
        if( selectedProgram )
        {
            if( contracts::DclContractsHelper::isPadButtonMessage( f_HwMsg ) )
            {
                const auto padNo    = contracts::DclContractsHelper::getPadNumber( f_HwMsg );
                const auto padState = contracts::DclContractsHelper::getPadState( f_HwMsg );
                const auto velocity = contracts::DclContractsHelper::getPadVelocity( f_HwMsg );
                if( padState == contracts::DeviceControlContracts::BTN_PRESSED ) padNote( padNo, true, velocity );
                else if( padState == contracts::DeviceControlContracts::BTN_RELEASED ) padNote( padNo, false, 0 );
            }
            else if( contracts::DclContractsHelper::isKnobMessage( f_HwMsg ) )
            {
                // Q-Link 1..4 drive the first four rotaries / numbers of the page that is showing (a Q-Link used to be read as a pad
                // hit here). Audio programs keep their own Q-Link use (nudge / edit start / end in the grid), the Data Dial goes on to
                // the page like before.
                const auto knob = contracts::DclContractsHelper::getKnobCode( f_HwMsg );
                if( knob >= contracts::DeviceControlContracts::EN_KNOB_1 && knob <= contracts::DeviceControlContracts::EN_KNOB_4 && selectedProgram->getType() != mt::MTProgramType::Audio )
                {
                    qLinkToPage( knob - contracts::DeviceControlContracts::EN_KNOB_1, contracts::DclContractsHelper::getKnobValue( f_HwMsg ) );
                    return;
                }
            }
            else if( contracts::DclContractsHelper::isButtonMessage( f_HwMsg ) && ( contracts::DclContractsHelper::getButtonState( f_HwMsg ) == contracts::DeviceControlContracts::BTN_PRESSED ) )
            {
                auto  buttonCode = contracts::DclContractsHelper::getButtonCode( f_HwMsg );
                auto& ae         = mt::getAudioInterface();
                auto& aeSel      = ae.getSelected();
                switch( buttonCode )
                {
                    case contracts::DeviceControlContracts::EN_PLAY: ae.setSequencePlay( aeSel.sequence, false, true ); return;
                    case contracts::DeviceControlContracts::EN_PLAYSTART: ae.setSequencePlay( aeSel.sequence, true, true ); return;
                    case contracts::DeviceControlContracts::EN_STOP: ae.setSequencePause(); return;
                }
            }
            selectedProgram->handleCommand( f_HwMsg );
        }
    }

    void ProgramEdit::resized()
    {
        auto b = getLocalBounds();
        header.setBounds( b.removeFromTop( ui::layout::header.getHeight() ) );
        audioheader.setBounds( header.getBounds() );
        //audioHdrTools.setBoundsRelative( .4567f, .0f, .489f, .1423f );
        infoLabel.setBoundsRelative( .1f, .45f, .8f, .2f );
        selectedPadLabel.setBounds( 600, 30, 140, 20 );
        padSelect.setBounds( 750, 8, 120, 44 );
        pluginOpen.setBoundsRelative( .35f, .68f, .3f, .1f );
        for( const auto& it : programs )
            it->setBounds( b );
    }

    void ProgramEdit::visibilityChanged()
    {
        auto children = getChildren();
        for( auto* it : children )
        {
            if( it )
                it->setVisible( isVisible() );
        }
        pluginOpen.setVisible( isVisible() && pluginMode_ );

        if( isVisible() )
        {
            // The one place a program is loaded on opening: the program of the selected track (else the selected program).
            auto& ae    = mt::getAudioInterface();
            auto  pProg = ae.getProgramDataForTrack( ae.getSelectedSequence(), ae.getSelectedTrack() );
            const auto name = pProg ? pProg->name.get() : ae.getProgramNameByIndex( ae.getSelectedProgram() );
            auto       type = pProg ? pProg->type.get() : ( selectedProgram ? selectedProgram->getType() : ae.getProgramType( name ) );
            hdrComp.programs.setValues( name, type, juce::sendNotification );
        }
        else
            releaseHeldPads();
    }

}
