namespace ui
{
    static const juce::uint8 whiteNotes[] = { 0, 2, 4, 5, 7, 9, 11 };
    static const juce::uint8 blackNotes[] = { 1, 3, 6, 8, 10 };

    struct KeyboardComponent::UpDownButton : public juce::Button
    {
        UpDownButton( KeyboardComponent& c, int d )
            : Button( {} ), owner( c ), delta( d )
        {}

        void clicked() override
        {
            auto note = owner.getLowestVisibleKey();

            if( delta < 0 )
                note = ( note - 1 ) / 12;
            else
                note = note / 12 + 1;

            owner.setLowestVisibleKey( note * 12 );
        }

        using Button::clicked;

        void paintButton( juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown ) override
        {
            owner.drawUpDownButton( g, getWidth(), getHeight(),
                                    shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown,
                                    delta > 0 );
        }

    private:
        KeyboardComponent& owner;
        const int          delta;

        JUCE_DECLARE_NON_COPYABLE( UpDownButton )
    };

    KeyboardComponent::KeyboardComponent( juce::MidiKeyboardState& s, Orientation o )
        : state( s ), orientation( o )
    {
        // initialise with a default set of qwerty key-mappings..
        scrollDown.reset( new UpDownButton( *this, -1 ) );
        scrollUp.reset( new UpDownButton( *this, 1 ) );

        addChildComponent( scrollDown.get() );
        addChildComponent( scrollUp.get() );

        int note = 0;

        for( char c : "awsedftgyhujkolp;" )
            setKeyPressForNote( juce::KeyPress( c, 0, 0 ), note++ );

        mouseOverNotes.insertMultiple( 0, -1, 32 );
        mouseDownNotes.insertMultiple( 0, -1, 32 );

        // The bridge forwards every finger move to a widget with this property (no 24 px tap slop): glissando and bend
        // need the first pixels of the slide, the piano applies its own dead zone.
        getProperties().set( "kickTouchNoSlop", true );
        // Note: ui::TouchSlot (the bridge's finger slot) tells the fingers apart, not the JUCE source index.

        colourChanged();
        setWantsKeyboardFocus( true );

        //state.addListener(this);
        //setSize( 512, 500 );
        //startTimerHz(20);
    }

    void KeyboardComponent::paint( juce::Graphics& g )
    {
        g.fillAll( colourOr( keySeparatorLineColourId, juce::Colour( 0xff0a0a0a ) ) );

        auto lineColour = findColour( keySeparatorLineColourId );
        auto textColour = findColour( textLabelColourId );

        for( int octave = 0; octave < 128; octave += 12 )
        {
            for( int white = 0; white < 7; ++white )
            {
                auto noteNum = octave + whiteNotes[white];

                if( noteNum >= rangeStart && noteNum <= rangeEnd )
                    drawWhiteNote( noteNum, g, getRectangleForKey( noteNum ),
                                   styleOf( noteNum ).down, false, lineColour, textColour );
            }
        }

        float x1 = 0.0f, y1 = 0.0f, x2 = 0.0f, y2 = 0.0f;
        auto  width  = getWidth();
        auto  height = getHeight();

        if( orientation == verticalKeyboardFacingLeft )
        {
            x1 = (float)width - 1.0f;
            x2 = (float)width - 5.0f;
        }
        else if( orientation == verticalKeyboardFacingRight )
            x2 = 5.0f;
        else
            y2 = 5.0f;

        auto x         = getKeyPos( rangeEnd ).getEnd();
        auto shadowCol = findColour( shadowColourId );

        if( !shadowCol.isTransparent() )
        {
            g.setGradientFill( juce::ColourGradient( shadowCol, x1, y1, shadowCol.withAlpha( 0.0f ), x2, y2, false ) );

            switch( orientation )
            {
                case horizontalKeyboard: g.fillRect( 0.0f, 0.0f, x, 5.0f ); break;
                case verticalKeyboardFacingLeft: g.fillRect( (float)width - 5.0f, 0.0f, 5.0f, x ); break;
                case verticalKeyboardFacingRight: g.fillRect( 0.0f, 0.0f, 5.0f, x ); break;
                default: break;
            }
        }

        if( !lineColour.isTransparent() )
        {
            g.setColour( lineColour );

            switch( orientation )
            {
                case horizontalKeyboard: g.fillRect( 0.0f, (float)height - 1.0f, x, 1.0f ); break;
                case verticalKeyboardFacingLeft: g.fillRect( 0.0f, 0.0f, 1.0f, x ); break;
                case verticalKeyboardFacingRight: g.fillRect( (float)width - 1.0f, 0.0f, 1.0f, x ); break;
                default: break;
            }
        }

        auto blackNoteColour = findColour( blackNoteColourId );

        for( int octave = 0; octave < 128; octave += 12 )
        {
            for( int black = 0; black < 5; ++black )
            {
                auto noteNum = octave + blackNotes[black];

                if( noteNum >= rangeStart && noteNum <= rangeEnd )
                    drawBlackNote( noteNum, g, getRectangleForKey( noteNum ),
                                   styleOf( noteNum ).down, false, blackNoteColour );
            }
        }
    }

    void KeyboardComponent::setKeyPressForNote( const juce::KeyPress& key, int midiNoteOffsetFromC )
    {
        removeKeyPressForNote( midiNoteOffsetFromC );

        keyPressNotes.add( midiNoteOffsetFromC );
        keyPresses.add( key );
    }

    void KeyboardComponent::removeKeyPressForNote( int midiNoteOffsetFromC )
    {
        for( int i = keyPressNotes.size(); --i >= 0; )
        {
            if( keyPressNotes.getUnchecked( i ) == midiNoteOffsetFromC )
            {
                keyPressNotes.remove( i );
                keyPresses.remove( i );
            }
        }
    }

    bool KeyboardComponent::mouseDownOnKey( int, const juce::MouseEvent& ) { return true; }
    bool KeyboardComponent::mouseDraggedToKey( int, const juce::MouseEvent& ) { return true; }
    void KeyboardComponent::mouseUpOnKey( int, const juce::MouseEvent& ) {}

    // One finger = one slot. Through the hardware bridge (MPC) every finger has the main mouse source (index 0), so the
    // slot comes from ui::TouchSlot; real touch input (desktop / iOS) has one JUCE source per finger (index = finger).
    static int fingerSlotOf( const juce::MouseEvent& e )
    {
        const int s = TouchSlot::inBridge() ? TouchSlot::state().slot : e.source.getIndex();
        return juce::jlimit( 0, piano::kSlots - 1, s );
    }

    void KeyboardComponent::mouseDown( const juce::MouseEvent& e )
    {
        const auto pos  = e.getEventRelativeTo( this ).position;
        const int  slot = fingerSlotOf( e );
        float      yFrac = 0.0f;
        const auto note  = xyToNote( pos, yFrac );

        piano::Events ev;
        if( note >= 0 && mouseDownOnKey( note, e ) )
            touchPiano.down( slot, note, yFrac, pos.x, pos.y, e.eventTime.toMilliseconds(), ev );
        else
            touchPiano.down( slot, -1, 0.0f, pos.x, pos.y, e.eventTime.toMilliseconds(), ev ); // ends a stale note of this slot
        expr.down( slot, pos.x );
        applyEvents( ev );
    }

    void KeyboardComponent::mouseDrag( const juce::MouseEvent& e )
    {
        const auto pos  = e.getEventRelativeTo( this ).position;
        const int  slot = fingerSlotOf( e );
        float      yFrac = 0.0f;
        const auto note  = xyToNote( pos, yFrac );
        const auto mode  = touchPiano.cfg.dragMode;

        if( mode == piano::DragMode::Bend || mode == piano::DragMode::Vibrato )
        {
            sendExpression( expr.move( slot, pos.x, mode ) );
            return;
        }
        piano::Events ev;
        if( note < 0 || mouseDraggedToKey( note, e ) )
            touchPiano.move( slot, note, yFrac, pos.x, pos.y, e.eventTime.toMilliseconds(), ev );
        applyEvents( ev );
    }

    void KeyboardComponent::mouseUp( const juce::MouseEvent& e )
    {
        const int slot = fingerSlotOf( e );
        const auto held = touchPiano.noteOfSlot( slot );
        piano::Events ev;
        touchPiano.up( slot, ev );
        applyEvents( ev );
        sendExpression( expr.up( slot ) );
        if( held >= 0 ) mouseUpOnKey( held, e );
    }

    void KeyboardComponent::mouseEnter( const juce::MouseEvent& )
    {
    }

    void KeyboardComponent::mouseExit( const juce::MouseEvent& )
    {
    }

    void KeyboardComponent::updateNoteUnderMouse( const juce::MouseEvent& e, bool isDown )
    {
        updateNoteUnderMouse( e.getEventRelativeTo( this ).position, isDown, fingerSlotOf( e ) );
    }

    // Kept for source compatibility: a position-based press / release of one finger.
    void KeyboardComponent::updateNoteUnderMouse( juce::Point<float> pos, bool isDown, int fingerNum )
    {
        float yFrac = 0.0f;
        const auto note = xyToNote( pos, yFrac );
        piano::Events ev;
        const auto now = juce::Time::currentTimeMillis();
        if( isDown ) touchPiano.down( fingerNum, note, yFrac, pos.x, pos.y, now, ev );
        else touchPiano.up( fingerNum, ev );
        applyEvents( ev );
    }

    void KeyboardComponent::applyEvents( const piano::Events& ev )
    {
        for( const auto& x : ev )
        {
            if( x.type == piano::Event::NoteOn ) lastVelocity[(size_t)x.note] = (uint8_t)x.velocity;
            repaintNote( x.note );
            if( x.type == piano::Event::NoteOn && noteSelectionChanged ) noteSelectionChanged( x.note );
            if( onKeyEvent ) onKeyEvent( x );
        }
    }

    void KeyboardComponent::sendExpression( const piano::ExpressionTracker::Out& o )
    {
        if( o.vibChanged && onVibrato ) onVibrato( o.vibrato );
        if( o.bendChanged && onPitchBend ) onPitchBend( o.bend );
    }

    void KeyboardComponent::setVisibleOctaves( float octaves )
    {
        visibleOctaves = juce::jlimit( 1.0f, 11.0f, octaves );
        resized();
    }

    void KeyboardComponent::setKeyMarks( const std::array<uint8_t, 128>& marks )
    {
        if( marks == keyMarks ) return;
        keyMarks = marks;
        repaint();
    }

    void KeyboardComponent::setExternalNotes( uint64_t lo, uint64_t hi )
    {
        std::bitset<128> nb;
        for( int i = 0; i < 64; ++i )
        {
            nb[(size_t)i]        = ( lo >> i ) & 1u;
            nb[(size_t)( i + 64 )] = ( hi >> i ) & 1u;
        }
        const auto diff = nb ^ externalNotes;
        if( diff.none() ) return;
        externalNotes = nb;
        for( int n = 0; n < 128; ++n )
            if( diff[(size_t)n] ) repaintNote( n );
    }

    void KeyboardComponent::setSustain( bool on )
    {
        piano::Events ev;
        touchPiano.sustain( on, ev );
        applyEvents( ev );
        repaint();
    }

    void KeyboardComponent::releaseAll()
    {
        piano::Events ev;
        touchPiano.releaseAll( ev );
        applyEvents( ev );
        sendExpression( expr.up( 0 ) );
        for( int s = 1; s < piano::kSlots; ++s ) sendExpression( expr.up( s ) );
        repaint();
    }

    bool KeyboardComponent::isAnyKeyDown() const
    {
        for( int n = 0; n < 128; ++n )
            if( touchPiano.isSounding( n ) ) return true;
        return false;
    }

    KeyboardComponent::KeyStyle KeyboardComponent::styleOf( int note ) const
    {
        KeyStyle k;
        k.down      = touchPiano.fingersOn( note ) > 0;
        k.sustained = touchPiano.isSustained( note );
        k.external  = note >= 0 && note < 128 && externalNotes[(size_t)note];
        k.mark      = note >= 0 && note < 128 ? (piano::KeyMark)keyMarks[(size_t)note] : piano::KeyMark::None;
        k.velocity  = note >= 0 && note < 128 ? lastVelocity[(size_t)note] : 0;
        return k;
    }

    void KeyboardComponent::handleNoteOn( juce::MidiKeyboardState*, int /*midiChannel*/, int /*midiNoteNumber*/, float /*velocity*/ )
    {
        shouldCheckState = true; // (probably being called from the audio thread, so avoid blocking in here)
    }

    void KeyboardComponent::handleNoteOff( juce::MidiKeyboardState*, int /*midiChannel*/, int /*midiNoteNumber*/, float /*velocity*/ )
    {
        shouldCheckState = true; // (probably being called from the audio thread, so avoid blocking in here)
    }

    void KeyboardComponent::drawUpDownButton( juce::Graphics& g, int w, int h,
                                              bool mouseOver,
                                              bool buttonDown,
                                              bool movesOctavesUp )
    {
        g.fillAll( findColour( upDownButtonBackgroundColourId ) );

        float angle = 0;

        switch( orientation )
        {
            case horizontalKeyboard: angle = movesOctavesUp ? 0.0f : 0.5f; break;
            case verticalKeyboardFacingLeft: angle = movesOctavesUp ? 0.25f : 0.75f; break;
            case verticalKeyboardFacingRight: angle = movesOctavesUp ? 0.75f : 0.25f; break;
            default: jassertfalse; break;
        }

        juce::Path path;
        path.addTriangle( 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.5f );
        path.applyTransform( juce::AffineTransform::rotation( juce::MathConstants<float>::twoPi * angle, 0.5f, 0.5f ) );

        g.setColour( findColour( upDownButtonArrowColourId )
                         .withAlpha( buttonDown ? 1.0f : ( mouseOver ? 0.6f : 0.4f ) ) );

        g.fillPath( path, path.getTransformToScaleToFit( 1.0f, 1.0f, (float)w - 2.0f, (float)h - 2.0f, true ) );
    }

    // Colours of the piano-ui mock-up (docs/plans/assets/piano-ui, dark theme): graphite gap, ivory / ebony keys,
    // velocity bands blue / turquoise / yellow / orange / red (the Kick pad LED palette), amber = held by the pedal,
    // cyan = sounds from elsewhere. A colour set on the component (or the look and feel) wins.
    juce::Colour KeyboardComponent::colourOr( int id, juce::Colour fallback ) const
    {
        return isColourSpecified( id ) || getLookAndFeel().isColourSpecified( id ) ? findColour( id ) : fallback;
    }

    static juce::Colour velocityBandColour( int velocity )
    {
        static const juce::uint32 band[5] = { 0xff3daeff, 0xff1fd3a0, 0xffffd02a, 0xffff8a1a, 0xffff3b30 };
        return juce::Colour( band[juce::jlimit( 0, 4, velocity * 5 / 128 )] );
    }

    // Marks shared by white and black keys: scale root (amber ring), other scale notes (small dot), chord tones (amber disc).
    void KeyboardComponent::drawMark( juce::Graphics& g, piano::KeyMark mark, juce::Rectangle<float> keyArea, float yFromBottom, bool onDarkKey )
    {
        if( mark == piano::KeyMark::None ) return;
        const float d  = juce::jlimit( 5.0f, 9.0f, keyArea.getWidth() * 0.45f );
        auto        dot = juce::Rectangle<float>( d, d ).withCentre( { keyArea.getCentreX(), keyArea.getBottom() - yFromBottom } );
        const auto  amber = colourOr( rootMarkColourId, juce::Colour( 0xffffb608 ) );
        switch( mark )
        {
            case piano::KeyMark::Root:
                g.setColour( amber );
                g.drawEllipse( dot, 2.0f );
                break;
            case piano::KeyMark::Chord:
                g.setColour( colourOr( chordMarkColourId, amber ) );
                g.fillEllipse( dot );
                break;
            case piano::KeyMark::InScale:
                g.setColour( colourOr( scaleMarkColourId, onDarkKey ? juce::Colour( 0xff8d8d8d ) : juce::Colour( 0xff9a968c ) ) );
                g.fillEllipse( dot.reduced( d * 0.22f ) );
                break;
            default: break;
        }
    }

    void KeyboardComponent::drawWhiteNote( int midiNoteNumber, juce::Graphics& g, juce::Rectangle<float> area,
                                           bool isDown, bool /*isOver*/, juce::Colour lineColour, juce::Colour textColour )
    {
        const auto st   = styleOf( midiNoteNumber );
        auto       key  = area.withTrimmedRight( 1.0f ).withTrimmedLeft( 0.0f );
        const bool down = isDown || st.down;

        if( down )
        {
            const auto c = velocityBandColour( st.velocity > 0 ? st.velocity : 100 );
            g.setGradientFill( juce::ColourGradient( c, 0.0f, key.getY(), c.interpolatedWith( juce::Colours::black, 0.3f ), 0.0f, key.getBottom(), false ) );
            g.fillRoundedRectangle( key.translated( 0.0f, 1.0f ), 3.0f );
        }
        else
        {
            g.setGradientFill( juce::ColourGradient( juce::Colour( 0xfff2f0ea ), 0.0f, key.getY(), juce::Colour( 0xffc9c5bb ), 0.0f, key.getBottom(), false ) );
            g.fillRoundedRectangle( key, 3.0f );
        }

        if( st.sustained )
        {
            const auto amber = colourOr( sustainedNoteColourId, juce::Colour( 0xffffb608 ) );
            g.setColour( amber );
            g.drawRoundedRectangle( key.reduced( 1.0f ), 3.0f, 2.0f );
            g.fillRect( key.withTrimmedLeft( key.getWidth() * 0.25f ).withTrimmedRight( key.getWidth() * 0.25f ).removeFromBottom( 3.0f ) );
        }
        if( st.external )
        {
            g.setColour( colourOr( externalNoteColourId, juce::Colour( 0xff00aaee ) ) );
            g.drawRoundedRectangle( key.reduced( 1.0f ), 3.0f, 2.0f );
        }

        drawMark( g, st.mark, key, juce::jmax( 18.0f, key.getHeight() * 0.16f ), false );

        auto text = getWhiteNoteText( midiNoteNumber );
        if( text.isNotEmpty() )
        {
            auto fontHeight = juce::jmin( 12.0f, juce::jmax( 9.0f, key.getWidth() * 0.55f ) );

            g.setColour( down ? juce::Colours::black.withAlpha( 0.65f ) : textColour );
            g.setFont( juce::Font( fontHeight ).withHorizontalScale( 0.8f ) );
            g.drawText( text, key.withTrimmedLeft( 1.0f ).withTrimmedBottom( 3.0f ), juce::Justification::centredBottom, false );
        }
        juce::ignoreUnused( lineColour );
    }

    void KeyboardComponent::drawBlackNote( int midiNoteNumber, juce::Graphics& g, juce::Rectangle<float> area,
                                           bool isDown, bool /*isOver*/, juce::Colour noteFillColour )
    {
        const auto st   = styleOf( midiNoteNumber );
        const bool down = isDown || st.down;

        if( down )
        {
            const auto c = velocityBandColour( st.velocity > 0 ? st.velocity : 100 );
            g.setGradientFill( juce::ColourGradient( c, 0.0f, area.getY(), c.interpolatedWith( juce::Colours::black, 0.3f ), 0.0f, area.getBottom(), false ) );
        }
        else
        {
            g.setGradientFill( juce::ColourGradient( noteFillColour.brighter( 0.35f ), 0.0f, area.getY(), noteFillColour.darker( 0.4f ), 0.0f, area.getBottom(), false ) );
        }
        g.fillRoundedRectangle( area, 2.0f );

        if( st.sustained )
        {
            g.setColour( colourOr( sustainedNoteColourId, juce::Colour( 0xffffb608 ) ) );
            g.drawRoundedRectangle( area.reduced( 1.0f ), 2.0f, 2.0f );
        }
        if( st.external )
        {
            g.setColour( colourOr( externalNoteColourId, juce::Colour( 0xff00aaee ) ) );
            g.drawRoundedRectangle( area.reduced( 1.0f ), 2.0f, 2.0f );
        }
        drawMark( g, st.mark, area, juce::jmax( 10.0f, area.getHeight() * 0.14f ), true );
    }

    void KeyboardComponent::setOctaveForMiddleC( int octaveNum )
    {
        octaveNumForMiddleC = octaveNum;
        repaint();
    }

    juce::String KeyboardComponent::getWhiteNoteText( int midiNoteNumber )
    {
        if( midiNoteNumber % 12 == 0 )
            return juce::MidiMessage::getMidiNoteName( midiNoteNumber, true, true, octaveNumForMiddleC );

        return {};
    }

    juce::Range<float> KeyboardComponent::getKeyPosition( int midiNoteNumber, float targetKeyWidth ) const
    {
        jassert( midiNoteNumber >= 0 && midiNoteNumber < 128 );

        static const float notePos[] = { 0.0f, 1,
                                         1.5f, 3,
                                         3.5f,
                                         5.0f, 6,
                                         6.5f, 8,
                                         8.5f, 10,
                                         10.5f };

        auto octave = midiNoteNumber / 12;
        auto note   = midiNoteNumber % 12;

        auto start = octave * 12.0f * targetKeyWidth + notePos[note] * targetKeyWidth;
        auto width = juce::MidiMessage::isMidiNoteBlack( note ) ? blackNoteWidthRatio * targetKeyWidth : targetKeyWidth;

        return { start, start + width };
    }

    KeyboardComponent::~KeyboardComponent()
    {}

    void KeyboardComponent::resized()
    {
        auto w = getWidth();
        auto h = getHeight();

        if( w > 0 && h > 0 )
        {
            if( orientation != horizontalKeyboard )
                std::swap( w, h );

            setKeyWidth( (float)w / ( 12.0f * visibleOctaves ) );

            auto kx2 = getKeyPos( rangeEnd ).getEnd();

            if( (int)firstKey != rangeStart )
            {
                auto kx1 = getKeyPos( rangeStart ).getStart();

                if( kx2 - kx1 <= (float)w )
                {
                    firstKey = (float)rangeStart;
                    sendChangeMessage();
                    repaint();
                }
            }

            xOffset = 0;

            if( canScroll )
            {
                auto scrollButtonW = juce::jmin( scrollButtonWidth, w / 2 );
                auto r             = getLocalBounds();

                if( orientation == horizontalKeyboard )
                {
                    scrollDown->setBounds( r.removeFromLeft( scrollButtonW ) );
                    scrollUp->setBounds( r.removeFromRight( scrollButtonW ) );
                }
                else if( orientation == verticalKeyboardFacingLeft )
                {
                    scrollDown->setBounds( r.removeFromTop( scrollButtonW ) );
                    scrollUp->setBounds( r.removeFromBottom( scrollButtonW ) );
                }
                else
                {
                    scrollDown->setBounds( r.removeFromBottom( scrollButtonW ) );
                    scrollUp->setBounds( r.removeFromTop( scrollButtonW ) );
                }

                auto endOfLastKey = getKeyPos( rangeEnd ).getEnd();

                float mousePositionVelocity;
                auto  spaceAvailable = w;
                auto  lastStartKey   = remappedXYToNote( { endOfLastKey - (float)spaceAvailable, 0 }, mousePositionVelocity ) + 1;

                if( lastStartKey >= 0 && ( (int)firstKey ) > lastStartKey )
                {
                    firstKey = (float)juce::jlimit( rangeStart, rangeEnd, lastStartKey );
                    sendChangeMessage();
                }

                xOffset = getKeyPos( (int)firstKey ).getStart();
            }
            else
            {
                firstKey = (float)rangeStart;
            }
            scrollDown->setVisible( canScroll && firstKey > (float)rangeStart );
            scrollUp->setVisible( canScroll && getKeyPos( rangeEnd ).getStart() > (float)w );

            repaint();
        }
    }

    juce::Rectangle<float> KeyboardComponent::getRectangleForKey( int note ) const
    {
        jassert( note >= rangeStart && note <= rangeEnd );

        auto pos = getKeyPos( note );
        auto x   = pos.getStart();
        auto w   = pos.getLength();

        if( juce::MidiMessage::isMidiNoteBlack( note ) )
        {
            auto blackNoteLength = getBlackNoteLength();

            switch( orientation )
            {
                case horizontalKeyboard: return { x, 0, w, blackNoteLength };
                case verticalKeyboardFacingLeft: return { getWidth() - blackNoteLength, x, blackNoteLength, w };
                case verticalKeyboardFacingRight: return { 0, getHeight() - x - w, blackNoteLength, w };
                default: jassertfalse; break;
            }
        }
        else
        {
            w *= getKeyWidthModifier( note );
            switch( orientation )
            {
                case horizontalKeyboard: return { x, 0, w, (float)getHeight() };
                case verticalKeyboardFacingLeft: return { 0, x, (float)getWidth(), w };
                case verticalKeyboardFacingRight: return { 0, getHeight() - x - w, (float)getWidth(), w };
                default: jassertfalse; break;
            }
        }

        return {};
    }

    float KeyboardComponent::getKeyWidthModifier( int midiNoteNumber ) const noexcept
    {
        auto name = juce::MidiMessage::getMidiNoteName( midiNoteNumber, true, false, 3 );

        if( name == "C" || name == "E" || name == "F" || name == "B" )
            return 1.5f;
        if( name == "D" || name == "G" || name == "A" )
            return 2;

        return 1;
    }

    void KeyboardComponent::setBlackNoteWidthProportion( float ratio ) noexcept
    {
        jassert( ratio >= 0.0f && ratio <= 1.0f );

        if( blackNoteWidthRatio != ratio )
        {
            blackNoteWidthRatio = ratio;
            resized();
        }
    }

    float KeyboardComponent::getBlackNoteLength() const noexcept
    {
        auto whiteNoteLength = orientation == horizontalKeyboard ? getHeight() : getWidth();
        return (float)whiteNoteLength * blackNoteLengthRatio;
    }

    juce::Range<float> KeyboardComponent::getKeyPos( int midiNoteNumber ) const
    {
        return getKeyPosition( midiNoteNumber, keyWidth )
               - xOffset
               - getKeyPosition( rangeStart, keyWidth ).getStart();
    }

    void KeyboardComponent::setLowestVisibleKey( int noteNumber )
    {
        setLowestVisibleKeyFloat( (float)noteNumber );
    }

    void KeyboardComponent::setLowestVisibleKeyFloat( float noteNumber )
    {
        noteNumber = juce::jlimit( (float)rangeStart, (float)rangeEnd, noteNumber );

        if( noteNumber != firstKey )
        {
            bool hasMoved = ( ( (int)firstKey ) != (int)noteNumber );
            firstKey      = noteNumber;

            if( hasMoved )
                sendChangeMessage();

            resized();
        }
    }

    void KeyboardComponent::setScrollButtonsVisible( bool newCanScroll )
    {
        if( canScroll != newCanScroll )
        {
            canScroll = newCanScroll;
            resized();
        }
    }

    void KeyboardComponent::colourChanged()
    {
        setOpaque( findColour( whiteNoteColourId ).isOpaque() );
        repaint();
    }

    float KeyboardComponent::getKeyStartPosition( int midiNoteNumber ) const
    {
        return getKeyPos( midiNoteNumber ).getStart();
    }

    float KeyboardComponent::getTotalKeyboardWidth() const noexcept
    {
        return getKeyPos( rangeEnd ).getEnd();
    }

    int KeyboardComponent::getNoteAtPosition( juce::Point<float> p )
    {
        float v;
        return xyToNote( p, v );
    }

    int KeyboardComponent::xyToNote( juce::Point<float> pos, float& mousePositionVelocity )
    {
        // Own bounds test, NOT reallyContains() / contains(): both walk up to the top-level window (reallyContains() through
        // its getComponentAt(), which is nullptr for an invisible window; contains() through the window's peer). That is the
        // MPC (hidden window, touches delivered straight to mouseDown/Drag/Up): every key lookup failed and no note ever
        // sounded. The event was already delivered to this component, so its local bounds are all that is needed.
        if( !getLocalBounds().toFloat().contains( pos ) )
            return -1;

        auto p = pos;

        if( orientation != horizontalKeyboard )
        {
            p = { p.y, p.x };

            if( orientation == verticalKeyboardFacingLeft )
                p = { p.x, (float)getWidth() - p.y };
            else
                p = { (float)getHeight() - p.x, p.y };
        }

        return remappedXYToNote( p + juce::Point<float>( xOffset, 0 ), mousePositionVelocity );
    }

    int KeyboardComponent::remappedXYToNote( juce::Point<float> pos, float& mousePositionVelocity ) const
    {
        auto blackNoteLength = getBlackNoteLength();

        if( pos.getY() < blackNoteLength )
        {
            for( int octaveStart = 12 * ( rangeStart / 12 ); octaveStart <= rangeEnd; octaveStart += 12 )
            {
                for( int i = 0; i < 5; ++i )
                {
                    auto note = octaveStart + blackNotes[i];

                    if( note >= rangeStart && note <= rangeEnd )
                    {
                        if( getKeyPos( note ).contains( pos.x - xOffset ) )
                        {
                            mousePositionVelocity = juce::jmax( 0.0f, pos.y / blackNoteLength );
                            return note;
                        }
                    }
                }
            }
        }

        for( int octaveStart = 12 * ( rangeStart / 12 ); octaveStart <= rangeEnd; octaveStart += 12 )
        {
            for( int i = 0; i < 7; ++i )
            {
                auto note = octaveStart + whiteNotes[i];

                if( note >= rangeStart && note <= rangeEnd )
                {
                    // The whole drawn key, not just its 1-unit start cell (see piano::whiteKeyUnits).
                    float us = 0, ue = 0;
                    piano::whiteKeyUnits( whiteNotes[i], us, ue );
                    const auto cell = getKeyPos( note );
                    const juce::Range<float> full( cell.getStart(), cell.getStart() + ( ue - us ) * keyWidth );
                    if( full.contains( pos.x - xOffset ) )
                    {
                        auto whiteNoteLength  = ( orientation == horizontalKeyboard ) ? getHeight() : getWidth();
                        mousePositionVelocity = juce::jmax( 0.0f, pos.y / (float)whiteNoteLength );
                        return note;
                    }
                }
            }
        }

        mousePositionVelocity = 0;
        return -1;
    }

    void KeyboardComponent::repaintNote( int noteNum )
    {
        if( noteNum >= rangeStart && noteNum <= rangeEnd )
            repaint( getRectangleForKey( noteNum ).getSmallestIntegerContainer() );
    }
}
