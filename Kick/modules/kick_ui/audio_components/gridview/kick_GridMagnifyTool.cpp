namespace kick_ui
{
    GridMagnifyTool::GridMagnifyTool( GridComponent& gridComponent, GridData& data, juce::Viewport& ViewPort )
        : IGridTool( gridComponent, data, GridToolType::GridMagnifyTool, ViewPort )
    {
    }

    void GridMagnifyTool::paint( juce::Graphics& g )
    {
    }

    void GridMagnifyTool::mouseDown( const juce::MouseEvent& event )
    {
        const auto vpEv  = event.getEventRelativeTo( &viewPort );
        previousVpPos    = { vpEv.position.getX(), vpEv.position.getY() };
        hasActivePan     = true;
        smoothViewX      = static_cast<float>( viewPort.getViewPositionX() );
        smoothViewY      = static_cast<float>( viewPort.getViewPositionY() );
        lastSetViewX     = viewPort.getViewPositionX();
        lastSetViewY     = viewPort.getViewPositionY();
    }

    void GridMagnifyTool::mouseUp( const juce::MouseEvent& )
    {
        hasActivePan = false;
    }

    void GridMagnifyTool::cancelGesture()
    {
        hasActivePan = false;
    }

    void GridMagnifyTool::mouseDrag( const juce::MouseEvent& event )
    {
        const auto                     vpEv  = event.getEventRelativeTo( &viewPort );
        const grid_logic::TouchPoint curVp = { vpEv.position.getX(), vpEv.position.getY() };

        if( !hasActivePan )
        {
            previousVpPos = curVp;
            hasActivePan  = true;
            smoothViewX   = static_cast<float>( viewPort.getViewPositionX() );
            smoothViewY   = static_cast<float>( viewPort.getViewPositionY() );
            lastSetViewX  = viewPort.getViewPositionX();
            lastSetViewY  = viewPort.getViewPositionY();
            return;
        }

        if( lastSetViewX != viewPort.getViewPositionX() || lastSetViewY != viewPort.getViewPositionY() )
        {
            smoothViewX = static_cast<float>( viewPort.getViewPositionX() );
            smoothViewY = static_cast<float>( viewPort.getViewPositionY() );
        }

        grid_logic::GridViewportState st;
        st.viewX      = smoothViewX;
        st.viewY      = smoothViewY;
        st.viewWidth  = static_cast<float>( viewPort.getWidth() );
        st.viewHeight = static_cast<float>( viewPort.getHeight() );
        st.barWidth   = gridData.getBarWidth();
        st.noteHeight = gridData.getNoteHeight();
        st.numBars    = gridData.getNumBars();
        st.maxNotes   = gridData.getNotesMax();

        const auto res = grid_logic::computeSingleFingerPan( st, previousVpPos, curVp );
        previousVpPos  = curVp;
        smoothViewX    = res.viewX;
        smoothViewY    = res.viewY;

        viewPort.setViewPosition( static_cast<int>( std::lround( smoothViewX ) ),
                                  static_cast<int>( std::lround( smoothViewY ) ) );
        lastSetViewX = viewPort.getViewPositionX();
        lastSetViewY = viewPort.getViewPositionY();
    }

}
