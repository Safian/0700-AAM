// SPDX-License-Identifier: AGPL-3.0-or-later
#include "kick_PianoView.h"

namespace kick_ui
{
    namespace
    {
        constexpr int kStateTimerHz = 20;
        const juce::Colour kPanel{ 0xff1d1d1d };
        const juce::Colour kAmber{ 0xffffb608 };

        const char* programTypeName( mt::MTProgramType t )
        {
            switch( t )
            {
                case mt::MTProgramType::Drum: return "Drum";
                case mt::MTProgramType::Keygroup: return "Keygroup";
                case mt::MTProgramType::Clip: return "Clip";
                case mt::MTProgramType::Audio: return "Audio";
                case mt::MTProgramType::Midi: return "MIDI";
                case mt::MTProgramType::Cv: return "CV";
                case mt::MTProgramType::Plugin: return "Plugin";
                default: return "";
            }
        }
    }

    //==============================================================================
    PianoOverview::PianoOverview()
    {
        getProperties().set( "kickTouchNoSlop", true );
    }

    void PianoOverview::setWindow( int firstKey, int numKeys )
    {
        if( firstKey == firstKey_ && numKeys == numKeys_ ) return;
        firstKey_ = firstKey;
        numKeys_  = numKeys;
        repaint();
    }

    void PianoOverview::paint( juce::Graphics& g )
    {
        auto  b = getLocalBounds().toFloat();
        g.setColour( juce::Colour( 0xff0a0a0a ) );
        g.fillRoundedRectangle( b, 3.0f );
        const float keyW = b.getWidth() / 128.0f;
        for( int n = 0; n < 128; ++n )
        {
            const bool black = juce::MidiMessage::isMidiNoteBlack( n );
            g.setColour( black ? juce::Colour( 0xff3a3a3a ) : juce::Colour( 0xff8d8a82 ) );
            g.fillRect( b.getX() + n * keyW, b.getY() + 3.0f, juce::jmax( 1.0f, keyW - 0.5f ), black ? b.getHeight() * 0.45f : b.getHeight() - 6.0f );
        }
        auto win = juce::Rectangle<float>( b.getX() + firstKey_ * keyW, b.getY() + 1.0f, numKeys_ * keyW, b.getHeight() - 2.0f );
        g.setColour( kAmber.withAlpha( 0.25f ) );
        g.fillRoundedRectangle( win, 3.0f );
        g.setColour( kAmber );
        g.drawRoundedRectangle( win, 3.0f, 2.0f );
    }

    void PianoOverview::scrollTo( const juce::MouseEvent& e )
    {
        const float keyW  = (float)getWidth() / 128.0f;
        const int   first = juce::roundToInt( e.position.x / keyW - numKeys_ * 0.5f );
        if( onScroll ) onScroll( juce::jlimit( 0, 127, first ) );
    }
    void PianoOverview::mouseDown( const juce::MouseEvent& e ) { scrollTo( e ); }
    void PianoOverview::mouseDrag( const juce::MouseEvent& e ) { scrollTo( e ); }

    //==============================================================================
    PianoView::PianoView()
        : kick_ui::IScreen( kick_ui::Screen::MenuPiano )
    {
        addAndMakeVisible( header );
        addAndMakeVisible( keys );
        addAndMakeVisible( overview );
        for( auto* c : std::initializer_list<juce::Component*>{ &velTitle, &dragTitle, &bendTitle, &fixedTitle, &transposeTitle, &viewTitle, &fixedValue, &transposeValue, &targetInfo,
                                                                 &velFixed, &velPos, &velSpeed, &dragGlide, &dragLegato, &dragBend, &dragVibrato, &range2, &range4, &range7, &range12,
                                                                 &sustain, &fixedDown, &fixedUp, &octDown, &octUp, &semiDown, &semiUp, &resetTranspose, &viewLeft, &viewRight, &zoomOut, &zoomIn } )
            addAndMakeVisible( c );

        header.onBackButtonPressed = [this]( bool ) {
            stopEverything();
            closeScreen();
        };
        ui::themeChanged.connect( [&]() { header.setIcon( "Icon_PadPerform" ); } );

        for( auto* l : { &velTitle, &dragTitle, &bendTitle, &fixedTitle, &transposeTitle, &viewTitle } )
        {
            l->setInterceptsMouseClicks( false, false );
            l->setValueColour( ui::Theme::textColour( 1 ) );
            l->setValueFontHeight( 14 );
            l->setValueJustification( juce::Justification::centredLeft );
        }
        for( auto* l : { &fixedValue, &transposeValue, &targetInfo } )
        {
            l->setInterceptsMouseClicks( false, false );
            l->setValueColour( ui::Theme::textColour( 1 ) );
            l->setValueBackgroundColour( ui::Theme::backgroundColour( 3 ) );
            l->setValueFontHeight( 16 );
            l->setValueJustification( juce::Justification::centred );
        }
        targetInfo.setValueJustification( juce::Justification::centredLeft );
        targetInfo.setValueFontHeight( 15 );

        mt::utils::setValueFontHeight( 16, velFixed, velPos, velSpeed, dragGlide, dragLegato, dragBend, dragVibrato, range2, range4, range7, range12, sustain,
                                       fixedDown, fixedUp, octDown, octUp, semiDown, semiUp, resetTranspose, viewLeft, viewRight, zoomOut, zoomIn );
        mt::utils::setValueColour( ui::Theme::textColour( 1 ), velFixed, velPos, velSpeed, dragGlide, dragLegato, dragBend, dragVibrato, range2, range4, range7, range12, sustain,
                                   fixedDown, fixedUp, octDown, octUp, semiDown, semiUp, resetTranspose, viewLeft, viewRight, zoomOut, zoomIn );
        mt::utils::setRadioGroupID( 9301, velFixed, velPos, velSpeed );
        mt::utils::setRadioGroupID( 9302, dragGlide, dragLegato, dragBend, dragVibrato );
        mt::utils::setRadioGroupID( 9303, range2, range4, range7, range12 );

        // Keyboard look: piano-ui mock-up (ivory / ebony keys on a graphite gap, amber accent).
        keys.setScrollButtonsVisible( false );
        keys.setColour( ui::KeyboardComponent::keySeparatorLineColourId, juce::Colour( 0xff0a0a0a ) );
        keys.setColour( ui::KeyboardComponent::blackNoteColourId, juce::Colour( 0xff1b1b1b ) );
        keys.setColour( ui::KeyboardComponent::textLabelColourId, juce::Colour( 0xff6c6a64 ) );
        keys.setColour( ui::KeyboardComponent::shadowColourId, juce::Colours::transparentBlack );
        keys.setLowestVisibleKey( 48 );
        keys.setVisibleOctaves( 3.0f );
        keys.onKeyEvent   = [this]( const ui::piano::Event& e ) { keyEvent( e ); };
        keys.onPitchBend  = [this]( int v ) { sendBend( v ); };
        keys.onVibrato    = [this]( int v ) { sendVibrato( v ); };

        overview.onScroll = [this]( int first ) { keys.setLowestVisibleKey( ( first / 12 ) * 12 ); };

        velFixed.onClick = [this]() { applyVelocityMode( ui::piano::VelocityMode::Fixed ); };
        velPos.onClick   = [this]() { applyVelocityMode( ui::piano::VelocityMode::Position ); };
        velSpeed.onClick = [this]() { applyVelocityMode( ui::piano::VelocityMode::Speed ); };
        dragGlide.onClick   = [this]() { applyDragMode( ui::piano::DragMode::Glide ); };
        dragLegato.onClick  = [this]() { applyDragMode( ui::piano::DragMode::Legato ); };
        dragBend.onClick    = [this]() { applyDragMode( ui::piano::DragMode::Bend ); };
        dragVibrato.onClick = [this]() { applyDragMode( ui::piano::DragMode::Vibrato ); };
        range2.onClick  = [] { mt::pitchbend::setRangeSemitones( 2.f ); };
        range4.onClick  = [] { mt::pitchbend::setRangeSemitones( 4.f ); };
        range7.onClick  = [] { mt::pitchbend::setRangeSemitones( 7.f ); };
        range12.onClick = [] { mt::pitchbend::setRangeSemitones( 12.f ); };

        auto setFixed = [this]( int delta ) {
            auto& cfg = keys.touch().cfg;
            cfg.fixedVelocity = ui::piano::clampVelocity( cfg.fixedVelocity + delta );
            fixedValue.setText( juce::String( cfg.fixedVelocity ), juce::dontSendNotification );
        };
        fixedDown.onClick = [setFixed]() { setFixed( -5 ); };
        fixedUp.onClick   = [setFixed]() { setFixed( 5 ); };

        sustain.onClick = [this]() { setSustain( sustain.getToggleState() ); };

        octDown.onClick        = [this]() { hub.requestStep( false, false ); };
        octUp.onClick          = [this]() { hub.requestStep( true, false ); };
        semiDown.onClick       = [this]() { hub.requestStep( false, true ); };
        semiUp.onClick         = [this]() { hub.requestStep( true, true ); };
        resetTranspose.onClick = [this]() { hub.requestReset(); };

        viewLeft.onClick  = [this]() { keys.setLowestVisibleKey( ( ( keys.getLowestVisibleKey() - 1 ) / 12 ) * 12 ); };
        viewRight.onClick = [this]() { keys.setLowestVisibleKey( ( keys.getLowestVisibleKey() / 12 + 1 ) * 12 ); };
        zoomOut.onClick   = [this]() { keys.setVisibleOctaves( keys.getVisibleOctaves() + 1.0f ); };
        zoomIn.onClick    = [this]() { keys.setVisibleOctaves( keys.getVisibleOctaves() - 1.0f ); };

        // Defaults: position velocity, glissando with slide, +-2.
        velPos.setToggleState( true, juce::dontSendNotification );
        dragGlide.setToggleState( true, juce::dontSendNotification );
        range2.setToggleState( true, juce::dontSendNotification );
        applyVelocityMode( ui::piano::VelocityMode::Position );
        applyDragMode( ui::piano::DragMode::Glide );
        mt::pitchbend::setRangeSemitones( 2.f );
        fixedValue.setText( juce::String( keys.touch().cfg.fixedVelocity ), juce::dontSendNotification );
    }

    PianoView::~PianoView()
    {
        stopTimer();
    }

    void PianoView::resized()
    {
        auto b = getLocalBounds();
        header.setBounds( b.removeFromTop( ui::layout::header.getHeight() ) );
        b.reduce( 8, 4 );

        const int rowH = 36, gap = 4;
        auto      row  = [&]() {
            auto r = b.removeFromTop( rowH );
            b.removeFromTop( gap );
            return r;
        };
        auto place = [&]( juce::Rectangle<int>& r, juce::Component& c, int w ) {
            c.setBounds( r.removeFromLeft( w ).reduced( 1, 0 ) );
        };

        auto r1 = row();
        place( r1, velTitle, 92 ); place( r1, velFixed, 78 ); place( r1, velPos, 96 ); place( r1, velSpeed, 82 );
        r1.removeFromLeft( 24 );
        place( r1, fixedTitle, 62 ); place( r1, fixedDown, 40 ); place( r1, fixedValue, 56 ); place( r1, fixedUp, 40 );

        auto r2 = row();
        place( r2, dragTitle, 92 ); place( r2, dragGlide, 78 ); place( r2, dragLegato, 82 ); place( r2, dragBend, 72 ); place( r2, dragVibrato, 92 );
        r2.removeFromLeft( 24 );
        place( r2, bendTitle, 100 ); place( r2, range2, 56 ); place( r2, range4, 56 ); place( r2, range7, 56 ); place( r2, range12, 64 );

        auto r3 = row();
        place( r3, sustain, 110 );
        r3.removeFromLeft( 12 );
        place( r3, transposeTitle, 94 ); place( r3, octDown, 58 ); place( r3, transposeValue, 128 ); place( r3, octUp, 58 );
        place( r3, semiDown, 50 ); place( r3, semiUp, 50 ); place( r3, resetTranspose, 40 );
        r3.removeFromLeft( 12 );
        place( r3, viewTitle, 52 ); place( r3, viewLeft, 40 ); place( r3, viewRight, 40 ); place( r3, zoomOut, 36 ); place( r3, zoomIn, 36 );

        targetInfo.setBounds( b.removeFromTop( 26 ) );
        b.removeFromTop( gap );
        overview.setBounds( b.removeFromTop( 24 ) );
        b.removeFromTop( gap );
        keys.setBounds( b );
    }

    void PianoView::paint( juce::Graphics& g )
    {
        g.fillAll( kPanel.darker( 0.6f ) );
    }

    void PianoView::visibilityChanged()
    {
        const bool visible = isVisible();
        for( auto it : getChildren() )
            it->setVisible( visible );

        if( visible || isShowing() )
        {
            lastTransposeVersion_ = -1;
            lastSelTrack_         = -1;
            startTimerHz( kStateTimerHz );
            refreshTargetInfo();
            refreshTransposeLabel();
        }
        else
        {
            stopTimer();
            stopEverything();
        }
    }

    //==============================================================================
    // Touch -> engine. The key number is the KEY (like the physical keybed); the sounding pitch adds the shared transposition.
    void PianoView::keyEvent( const ui::piano::Event& e )
    {
        if( e.note < 0 || e.note > 127 ) return;
        const bool trace = ui::LatencyTrace::enabled();

        if( e.type == ui::piano::Event::NoteOn )
        {
            Sounding& s = sounding_[e.note];
            if( s.on ) ae.stopLiveMidiNote( s.sel, s.pitch ); // never leave an older voice of this key behind
            s.on    = true;
            s.pitch = ui::piano::soundingPitch( e.note, hub.octave(), hub.semitone() );
            s.sel   = ae.getSelected(); // the note-off goes to the track that got the note-on

            const int64_t t0 = trace ? ui::TouchSlot::nowUs() : 0;
            ae.addLiveMidiNote( s.sel, s.pitch, e.velocity );
            if( trace )
            {
                const int64_t t1 = ui::TouchSlot::nowUs(), ev = ui::TouchSlot::eventUs();
                kick_log( "bridge", "[LAT] note-on key=%d sounding=%d vel=%d slot=%d addLiveMidiNote called %lldus after the touch was read (call took %lldus)%s\n",
                          e.note, s.pitch, e.velocity, e.slot, ev > 0 ? (long long)( t0 - ev ) : -1LL, (long long)( t1 - t0 ), ev > 0 ? "" : " [no touch time: not from the bridge]" );
            }
        }
        else
        {
            Sounding& s = sounding_[e.note];
            if( ! s.on ) return;
            s.on = false;
            const int64_t t0 = trace ? ui::TouchSlot::nowUs() : 0;
            ae.stopLiveMidiNote( s.sel, s.pitch );
            if( trace )
            {
                const int64_t ev = ui::TouchSlot::eventUs();
                kick_log( "bridge", "[LAT] note-off key=%d sounding=%d slot=%d stopLiveMidiNote called %lldus after the touch was read\n",
                          e.note, s.pitch, e.slot, ev > 0 ? (long long)( t0 - ev ) : -1LL );
            }
        }
    }

    // Pitch wheel: the track that got the first off-centre value gets the spring back as well (like the touch strip).
    void PianoView::sendBend( int value )
    {
        const bool centre = ui::piano::kBendCentre == value;
        if( ! bendActive_ && ! centre )
        {
            bendActive_ = true;
            bendTarget_ = ae.getSelected();
        }
        const auto target = bendActive_ ? bendTarget_ : ae.getSelected();
        ae.addLiveMidiMsg( target, juce::MidiMessage::pitchWheel( 1, value ) );
        if( centre ) bendActive_ = false;
        if( ui::LatencyTrace::enabled() ) kick_log( "bridge", "[LAT] bend %d\n", value );
    }

    void PianoView::sendVibrato( int value )
    {
        ae.addLiveMidiMsg( ae.getSelected(), juce::MidiMessage::controllerEvent( 1, 1, value ) );
    }

    // CC64 first (so an engine / external synth releases with the pedal), then the held-note logic: pressing it only starts
    // deferring note-offs, releasing it ends the pedal-held notes (the note-offs follow the CC64=0).
    void PianoView::setSustain( bool on )
    {
        if( hub.sustainViaBridge() ) hub.setSustain( on );
        else ae.addLiveMidiMsg( ae.getSelected(), juce::MidiMessage::controllerEvent( 1, 64, on ? 127 : 0 ) );
        keys.setSustain( on );
        if( sustain.getToggleState() != on ) sustain.setToggleState( on, juce::dontSendNotification );
    }

    void PianoView::stopEverything()
    {
        if( keys.touch().sustainOn() ) setSustain( false );
        keys.releaseAll();
        for( int n = 0; n < 128; ++n )
            if( sounding_[n].on ) // (should be empty after releaseAll; a safety net)
            {
                sounding_[n].on = false;
                ae.stopLiveMidiNote( sounding_[n].sel, sounding_[n].pitch );
            }
        if( bendActive_ ) sendBend( ui::piano::kBendCentre );
        sendVibrato( 0 );
    }

    void PianoView::applyVelocityMode( ui::piano::VelocityMode m ) { keys.touch().cfg.velocityMode = m; }
    void PianoView::applyDragMode( ui::piano::DragMode m )
    {
        // A bend / vibrato in flight must end cleanly when the mode changes.
        keys.touch().cfg.dragMode = m;
    }

    //==============================================================================
    void PianoView::timerCallback()
    {
        if( hub.version() != lastTransposeVersion_ )
        {
            lastTransposeVersion_ = hub.version();
            refreshTransposeLabel();
        }
        const auto sel = ae.getSelected();
        if( sel.track != lastSelTrack_ || sel.sequence != lastSelSeq_ )
        {
            lastSelTrack_ = sel.track;
            lastSelSeq_   = sel.sequence;
            refreshTargetInfo();
        }
        refreshMarksAndExternal();
    }

    void PianoView::refreshTransposeLabel()
    {
        transposeValue.setText( "OCT " + juce::String( hub.octave() ) + "  ST " + juce::String( hub.semitone() ), juce::dontSendNotification );
        overview.setWindow( keys.getLowestVisibleKey(), (int)std::lround( keys.getVisibleOctaves() * 12.0f ) );
    }

    // The target of the keys: the selected track and its program (for a MIDI program: port / channel / bank / program).
    void PianoView::refreshTargetInfo()
    {
        const auto sel = ae.getSelected();
        juce::String text = "TRACK " + juce::String( sel.track + 1 );
        if( auto* p = ae.getProgramForTrack( sel.sequence, sel.track ) )
        {
            text << "  -  " << p->getName() << " (" << programTypeName( p->getType() ) << ")";
            if( auto* m = dynamic_cast<mt::MTMidiProgram*>( p ) )
                text << "  ->  " << m->getPort() << "  ch " << juce::String( m->getMidiChannel() ) << "  bank " << juce::String( m->getBankMSB() ) << "/" << juce::String( m->getBankLSB() );
        }
        else
            text << "  -  no program";
        targetInfo.setText( text, juce::dontSendNotification );
    }

    // Marks from the shared Pad Perform source (root, scale, chord of the project's performance state), and the notes that
    // sound from elsewhere (engine note activity of the selected track: physical keys, sequence, arpeggiator).
    void PianoView::refreshMarksAndExternal()
    {
        using ui::piano::KeyMark;
        const int octave = hub.octave(), semitone = hub.semitone();

        std::array<uint8_t, 128> marks{};
        if( auto* pp = ae.getPadPerformanceData() )
        {
            const int rootPc = juce::jmax( 0, mt::Note::getRootNumber( pp->performRootNote.get() ) );
            const auto type  = mt::ScaleAndNotes::getScaleType( pp->performScale.get() );
            std::vector<bool> inScale;
            auto  f = mt::ScaleAndNotes::scaleFormulaMap.find( type );
            if( f != mt::ScaleAndNotes::scaleFormulaMap.end() )
                inScale = ui::piano::scalePitchClasses( rootPc, f->second );

            // Chord tones of the key that was pressed last (only when the pads would play chords: mode CHORDS).
            int              chordRootKey = -1;
            std::vector<int> chordSteps;
            if( pp->performMode.get() == mt::PerformanceMode::CHORDS )
            {
                for( int n = 127; n >= 0; --n )
                    if( keys.touch().fingersOn( n ) > 0 ) { chordRootKey = n; break; }
                auto c = mt::ScaleAndNotes::chordFormulaMap.find( pp->performChords.get() );
                if( c != mt::ScaleAndNotes::chordFormulaMap.end() ) chordSteps = c->second;
            }
            for( int k = 0; k < 128; ++k )
            {
                const int snd = ui::piano::soundingPitch( k, octave, semitone ); // marks are about what the key SOUNDS
                marks[(size_t)k] = (uint8_t)ui::piano::markFor( snd, rootPc, inScale, chordRootKey >= 0 ? ui::piano::soundingPitch( chordRootKey, octave, semitone ) : -1, chordSteps );
            }
        }
        keys.setKeyMarks( marks );

        uint64_t lo = 0, hi = 0;
        ae.getActiveNotes( ae.getSelected(), lo, hi );
        uint64_t klo = 0, khi = 0;
        for( int n = 0; n < 128; ++n )
        {
            const bool on = n < 64 ? ( ( lo >> n ) & 1u ) : ( ( hi >> ( n - 64 ) ) & 1u );
            if( ! on ) continue;
            const int key = ui::piano::keyForSoundingPitch( n, octave, semitone );
            if( key < 0 || keys.touch().isSounding( key ) ) continue; // our own fingers are drawn as presses
            if( key < 64 ) klo |= uint64_t( 1 ) << key;
            else khi |= uint64_t( 1 ) << ( key - 64 );
        }
        keys.setExternalNotes( klo, khi );

        const int first = keys.getLowestVisibleKey();
        overview.setWindow( first, (int)std::lround( keys.getVisibleOctaves() * 12.0f ) );
    }

    void PianoView::handleCommand( CPktMgr::HWMsg_t& f_HwMsg )
    {
        if( contracts::DclContractsHelper::isButtonMessage( f_HwMsg ) && ( contracts::DclContractsHelper::getButtonState( f_HwMsg ) == contracts::DeviceControlContracts::BTN_PRESSED ) )
        {
            auto  buttonCode = contracts::DclContractsHelper::getButtonCode( f_HwMsg );
            auto& aeSel      = ae.getSelected();
            switch( buttonCode )
            {
                case contracts::DeviceControlContracts::EN_PLAY: ae.setSequencePlay( aeSel.sequence, false, true ); break;
                case contracts::DeviceControlContracts::EN_PLAYSTART: ae.setSequencePlay( aeSel.sequence, true, true ); break;
                case contracts::DeviceControlContracts::EN_STOP: ae.setSequencePause(); break;
            }
        }
    }
}
