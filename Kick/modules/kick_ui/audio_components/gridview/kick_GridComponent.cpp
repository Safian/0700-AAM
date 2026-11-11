
namespace kick_ui
{

    ///////////////////////////////////////////////////////////////////////
    // Grid Notes
    ///////////////////////////////////////////////////////////////////////
    GridNotes::GridNotes()
    {
        setName( "GridNote-Canvas" );
        setInterceptsMouseClicks( false, false );
    }

    void GridNotes::paint( juce::Graphics& g )
    {
        auto  h                = gridData.getNoteHeight();
        auto  noteList         = ae.getNoteList();
        auto& selectedNoteList = ae.getSelectedNoteList();

        for( auto it : noteList )
        {
            mt::MTNote           note( it );
            juce::Rectangle<int> rect;
            if( note.isValid() )
            {
                auto ys  = gridData.getYFromNote( note.number.get() );
                auto xs  = gridData.getXFromTime( note.start.get() );
                auto bar = gridData.getBarFromX( xs );
                auto w   = note.length.get() * gridData.getXPerTick( bar );

                rect.setBounds( xs, ys, w, h );
                // Fill...
                mt::utils::fillRoundedRect( g, rect, ui::Theme::noteColour( note.velocity.get() ) );
                // Outline...
                auto isSel = selectedNoteList.contains( it );
                if( rect.getWidth() <= 1 ) rect.setWidth( 1 );
                mt::utils::renderRoundedRect( g, isSel ? rect.getWidth() <= 1 ? rect : rect.reduced( 1 ) : rect, ui::Theme::outlineColour( isSel ? 1 : 4 ) );
            }
        }
    }

    ///////////////////////////////////////////////////////////////////////
    // Grid Component
    ///////////////////////////////////////////////////////////////////////
    GridComponent::GridComponent( juce::Viewport& ViewPort )
        : infoBar( this, 0 ), viewPort( ViewPort )
    {
        setName( "GridComponent" );
        setWantsKeyboardFocus( true );

        mt::utils::addChildComponents( this, notes );

        {
            auto p = new GridPencilTool( *this, gridData, viewPort );
            auto e = new GridEraseTool( *this, gridData, viewPort );
            auto s = new GridSelectTool( *this, gridData, viewPort );
            auto c = new GridCopyTool( *this, gridData, viewPort );
            auto m = new GridMagnifyTool( *this, gridData, viewPort );

            tools.insert( std::make_pair( p->getType(), p ) );
            tools.insert( std::make_pair( e->getType(), e ) );
            tools.insert( std::make_pair( s->getType(), s ) );
            tools.insert( std::make_pair( c->getType(), c ) );
            tools.insert( std::make_pair( m->getType(), m ) );
        }

        activeTool = tools.begin()->second;

        gridNeedsRepaint.connect( [&]() {
            triggerAsyncUpdate();
        } );
    }

    GridComponent::~GridComponent()
    {
        for( auto tool : tools )
        {
            delete tool.second;
        }
    }

    void GridComponent::noteSelectionChanged( int note )
    {
        noteSelected = note;
        repaint();
    }

    void GridComponent::setPosition( int bar, int beat, int tick )
    {
        currentBar  = bar;
        currentBeat = beat;
        currentTick = tick;
    }

    void GridComponent::paint( juce::Graphics& g )
    {
        auto canvas = getLocalBounds().toFloat();

        auto qColour    = ui::Theme::gridColour( 1 ).withMultipliedBrightness( 1.2 );
        auto beatColour = ui::Theme::gridColour( 2 );
        auto barColour  = ui::Theme::gridColour( 3 );

        auto numBars = gridData.getNumBars();

        // NOTE overlay...
        g.fillCheckerBoard( canvas, canvas.getWidth(), (float)gridData.getNoteHeight(), juce::Colours::grey.withAlpha( 0.5f ), juce::Colours::black.withAlpha( 0.5f ) );

        if( mt::getAudioInterface().getNumSequences() > 0 )
        {
            ////Draw bar beat
            auto barWidth  = gridData.getBarWidth();
            auto drawWidth = barWidth * numBars;

            float x  = 0.f;
            float qx = 0.f;

            juce::Rectangle<float> barArea{ x, canvas.getY(), drawWidth, canvas.getHeight() };

            //Draw Bars and Beats
            for( int bar = 0; bar < numBars; ++bar )
            {
                auto qlWidth   = gridData.getQLWidth( bar );
                auto beatWidth = gridData.getBarWidth() / gridData.getBeatsPerBar( bar );
                //Draw Beat Background
                auto thisBarArea = barArea.removeFromLeft( barWidth );
                g.fillCheckerBoard( thisBarArea, beatWidth, thisBarArea.getHeight(), ui::Theme::backgroundColour( 1 ).withAlpha( 0.9f ), ui::Theme::backgroundColour( 3 ).withAlpha( 0.9f ) );
                //Draw Bar line
                g.setColour( barColour );
                g.drawLine( x, 0.f, x + 1.f, canvas.getHeight() );
                x += barWidth;

                //QuantizationLevels
                g.setColour( qColour );
                auto drawQl = gridData.getNumQlPerBar( bar );
                for( int q = 0; q < drawQl; ++q )
                {
                    qx += qlWidth;
                    g.drawLine( qx, 0.f, qx + 1.f, canvas.getHeight() );
                }
            }

            // Draw highlighted row.
            if( juce::isPositiveAndBelow( noteSelected, gridData.getNotesMax() ) )
            {
                g.setColour( barColour );
                juce::Rectangle<float> bounds = { 1.f, gridData.getYFromNote( noteSelected ), canvas.getWidth() - 1.f, gridData.getNoteHeight() };
                g.drawRoundedRectangle( bounds, 1.f, 1.f );
            }

            //paint active tools
            if( activeTool )
            {
                activeTool->paint( g );
            }
        }
        else
        {
            float x             = 0.f;
            auto  gridCompWidth = getWidth();

            // Draw the beats and ticks within the single bar
            juce::Rectangle<float> barArea{ x, (float)canvas.getY(), (float)gridCompWidth, (float)canvas.getHeight() };
            g.fillCheckerBoard( barArea, (float)gridCompWidth / 4, canvas.getHeight(), ui::Theme::backgroundColour( 1 ).withAlpha( 0.9f ), ui::Theme::backgroundColour( 3 ).withAlpha( 0.9f ) );

            qColour = ui::Theme::gridColour( 1 );
            for( int beat = 0; beat < 4; ++beat ) // Default 4 beats per bar
            {
                float beatX = beat * gridCompWidth / 4;
                for( int tick = 1; tick < 4; ++tick ) // Default 4 ticks per beat
                {
                    float tickX = beatX + ( tick * gridCompWidth / 16 );
                    g.drawLine( tickX, 0, tickX, canvas.getHeight(), 0.5f ); // Draw tick line
                }
            }
        }
        parameterViewNeedsRepaint();
        return;
    }

    void GridComponent::resized()
    {
        notes.setBounds( getLocalBounds() );
    }

    void GridComponent::insertTimeSig( int bar, int num, int den )
    {
        ai.insertTimeSig( aiSel, bar - 1, num, den );
    }

    void GridComponent::deleteTimeSig( int bar )
    {
        ai.deleteTimSig( aiSel, bar );
    }

    static int resolveGridTouchId( const juce::MouseEvent& event ) noexcept
    {
        return grid_logic::resolveTouchId( ui::TouchSlot::inBridge() ? ui::TouchSlot::state().slot : -1,
                                           event.source.getIndex() );
    }

    bool IGridTool::findNoteAtPosition( juce::Point<float> pos, float slopPx, grid_logic::NoteSpan& outNote ) const
    {
        const int noteNum  = gridData.getNoteFromY( pos.getY() );
        const int clampedX = static_cast<int>( std::lround( juce::jmax( 0.f, pos.getX() ) ) );
        const int tick     = gridData.getTimeFromX( clampedX, false );
        const int bar      = gridData.getBarFromX( clampedX );
        const int tpb      = static_cast<int>( gridData.getTicksPerBarAt( static_cast<unsigned int>( juce::jmax( 0, bar ) ) ) );
        const int slop     = grid_logic::ticksFromPixelSlop( slopPx, gridData.getBarWidth(), tpb > 0 ? tpb : 1920 );

        auto                              noteList = ae.getNoteList();
        std::vector<grid_logic::NoteSpan> spans;
        spans.reserve( static_cast<size_t>( noteList.getNumChildren() ) );

        for( auto it : noteList )
        {
            mt::MTNote n( it );
            if( n.isValid() )
                spans.push_back( { n.number.get(), n.start.get(), n.length.get() } );
        }

        const int hitIdx = grid_logic::findBestNoteMatch( spans.data(), static_cast<int>( spans.size() ), noteNum, tick, slop );
        if( hitIdx >= 0 )
        {
            outNote = spans[static_cast<size_t>( hitIdx )];
            return true;
        }
        return false;
    }

    bool IGridTool::eraseNoteAtPosition( juce::Point<float> pos, float slopPx, bool allowDeleteSelected )
    {
        grid_logic::NoteSpan hit;
        if( !findNoteAtPosition( pos, slopPx, hit ) )
            return false;

        const int queryLen = ( hit.length > 0 ) ? hit.length : -1;

        if( allowDeleteSelected && ae.isValidSelectedNote( hit.note, hit.start, queryLen ) )
        {
            ae.deleteSelectedNotes( aeSelected );
            return true;
        }
        return ae.eraseNote( hit.note, hit.start, queryLen ) == 0;
    }

    void GridComponent::mouseUp( const juce::MouseEvent& event )
    {
        activeTouches.erase( resolveGridTouchId( event ) );
        if( multiTouchGestureActive )
        {
            if( activeTouches.empty() )
                multiTouchGestureActive = false;
            return;
        }
        if( activeTool ) activeTool->mouseUp( event );
    }

    void GridComponent::mouseMove( const juce::MouseEvent& event )
    {
        if( activeTool ) activeTool->mouseMove( event );
    }

    void GridComponent::mouseDrag( const juce::MouseEvent& event )
    {
        const int                      tid   = resolveGridTouchId( event );
        const auto                     vpEv  = event.getEventRelativeTo( &viewPort );
        const grid_logic::TouchPoint curVp = { vpEv.position.getX(), vpEv.position.getY() };

        if( activeTouches.find( tid ) == activeTouches.end() )
            activeTouches[tid] = curVp;

        if( activeTouches.size() >= 2 )
        {
            multiTouchGestureActive = true;

            auto it1  = activeTouches.begin();
            auto it2  = std::next( it1 );
            const grid_logic::TouchPoint prevP1 = it1->second;
            const grid_logic::TouchPoint prevP2 = it2->second;

            activeTouches[tid] = curVp;
            const grid_logic::TouchPoint curP1 = it1->second;
            const grid_logic::TouchPoint curP2 = it2->second;

            if( lastSetViewX != viewPort.getViewPositionX() || lastSetViewY != viewPort.getViewPositionY() )
            {
                smoothViewX = static_cast<float>( viewPort.getViewPositionX() );
                smoothViewY = static_cast<float>( viewPort.getViewPositionY() );
            }

            grid_logic::GridViewportState st;
            st.viewX      = smoothViewX;
            st.viewY      = smoothViewY;
            st.viewWidth  = static_cast<float>( viewPort.getViewWidth() );
            st.viewHeight = static_cast<float>( viewPort.getViewHeight() );
            st.barWidth   = gridData.getBarWidth();
            st.noteHeight = gridData.getNoteHeight();
            st.numBars    = gridData.getNumBars();
            st.maxNotes   = gridData.getNotesMax();

            const auto res = grid_logic::computePinchPan( st, gridData.getZoomBounds(), prevP1, prevP2, curP1, curP2 );
            const float dW = res.barWidth - gridData.getBarWidth();
            const float dH = res.noteHeight - gridData.getNoteHeight();
            if( std::abs( dW ) > 1e-4f || std::abs( dH ) > 1e-4f )
                gridData.setZoomFactor( dW, dH );

            smoothViewX = res.viewX;
            smoothViewY = res.viewY;
            viewPort.setViewPosition( static_cast<int>( std::lround( smoothViewX ) ),
                                      static_cast<int>( std::lround( smoothViewY ) ) );
            lastSetViewX = viewPort.getViewPositionX();
            lastSetViewY = viewPort.getViewPositionY();
            return;
        }

        activeTouches[tid] = curVp;
        if( multiTouchGestureActive )
            return;

        if( activeTool ) activeTool->mouseDrag( event );
    }

    void GridComponent::mouseDown( const juce::MouseEvent& event )
    {
        const int                      tid   = resolveGridTouchId( event );
        const auto                     vpEv  = event.getEventRelativeTo( &viewPort );
        const grid_logic::TouchPoint curVp = { vpEv.position.getX(), vpEv.position.getY() };

        activeTouches[tid] = curVp;
        if( activeTouches.size() == 1 || lastSetViewX != viewPort.getViewPositionX() || lastSetViewY != viewPort.getViewPositionY() )
        {
            smoothViewX  = static_cast<float>( viewPort.getViewPositionX() );
            smoothViewY  = static_cast<float>( viewPort.getViewPositionY() );
            lastSetViewX = viewPort.getViewPositionX();
            lastSetViewY = viewPort.getViewPositionY();
        }

        if( activeTouches.size() >= 2 )
        {
            if( !multiTouchGestureActive )
            {
                multiTouchGestureActive = true;
                if( activeTool ) activeTool->cancelGesture();
            }
            return;
        }

        if( multiTouchGestureActive )
            return;

        if( activeTool ) activeTool->mouseDown( event );
    }

    void GridComponent::mouseDoubleClick( const juce::MouseEvent& event )
    {
        if( multiTouchGestureActive || activeTouches.size() > 1 )
            return;
        if( activeTool ) activeTool->mouseDoubleClick( event );
    }

    void GridComponent::toolTypeChanged( GridToolType toolType )
    {
        if( activeTool ) activeTool->cancelGesture();
        activeTouches.clear();
        multiTouchGestureActive = false;
        auto it                 = tools.find( toolType );
        activeTool              = ( it != tools.end() ) ? it->second : nullptr;
    }

    void GridComponent::visibilityChanged()
    {
        if( activeTool ) activeTool->cancelGesture();
        activeTouches.clear();
        multiTouchGestureActive = false;
        for( auto it : getChildren() )
            it->setVisible( isVisible() );
    }

    void GridComponent::handleAsyncUpdate()
    {
        notes.repaint();
        repaint();
    }

    int GridComponent::getMouseDownNote()
    {
        if( activeTool )
            return activeTool->getMouseDownNoteNumber();
        return -1;
    }

} // namespace kick_ui
