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
        auto touchIndex                    = event.source.getIndex();
        activeTouches[touchIndex]          = event.position;
        previousTouchPositions[touchIndex] = event.position;
        initialTouchPositions[touchIndex]  = event.position;
        if( activeTouches.size() == 2 )
        {
            initialDistance   = calculateDistance();
            initialZoomFactor = zoomFactor;
            auto it = activeTouches.begin();
            auto p1 = it->second;
            ++it;
            auto p2     = it->second;
            initialTick = gridData.getTimeFromX( ( p1.x + p2.x ) / 2 );
            initialNote = gridData.getNoteFromY( ( p1.y + p2.y ) / 2 );
        }
    }

    void GridMagnifyTool::mouseUp( const juce::MouseEvent& event )
    {
        auto touchIndex = event.source.getIndex();
        activeTouches.erase( touchIndex );
        previousTouchPositions.erase( touchIndex );
        initialTouchPositions.erase( touchIndex );
        if( activeTouches.empty() )
        {
            initialTick = 0;
            initialNote = 0;
        }
    }

    void GridMagnifyTool::mouseDrag( const juce::MouseEvent& event )
    {
        auto touchIndex           = event.source.getIndex();
        auto currentPosition      = event.position;
        activeTouches[touchIndex] = currentPosition;
        if( activeTouches.size() == 2 ) //Zoom in - Zoom out
        {
            auto  prevZoomFactor  = zoomFactor;
            float currentDistance = calculateDistance();
            if( initialDistance > 0.0f )
                zoomFactor = initialZoomFactor * ( currentDistance / initialDistance );

            auto it   = activeTouches.begin();
            auto idx1 = it->first;
            auto p1   = it->second;
            ++it;
            auto idx2 = it->first;
            auto p2   = it->second;

            auto viewWidth  = viewPort.getViewWidth();
            auto viewHeight = viewPort.getViewHeight();

            auto viewPos = viewPort.getViewPosition();

            float initialXDistance = std::abs( initialTouchPositions[idx1].x - initialTouchPositions[idx2].x );
            float initialYDistance = std::abs( initialTouchPositions[idx1].y - initialTouchPositions[idx2].y );
            float currentXDistance = std::abs( p1.x - p2.x );
            float currentYDistance = std::abs( p1.y - p2.y );

            bool isHorizontalZoom = std::abs( currentXDistance - initialXDistance ) > 10.0f;
            bool isVerticalZoom   = std::abs( currentYDistance - initialYDistance ) > 10.0f;

            if( currentDistance > initialDistance )
                gridData.setZoomFactor( isHorizontalZoom && isVerticalZoom ? 20 : isHorizontalZoom ? 20 :
                                                                                                     0,
                                        isHorizontalZoom && isVerticalZoom ? 1 : isHorizontalZoom ? 0 :
                                                                                                    1 );
            else
                gridData.setZoomFactor( isHorizontalZoom && isVerticalZoom ? -20 : isHorizontalZoom ? -20 :
                                                                                                      0,
                                        isHorizontalZoom && isVerticalZoom ? -1 : isHorizontalZoom ? 0 :
                                                                                                     -1 );

            if( gridComp.getWidth() <= 864 ) //GridViewPort Minimum Width
                zoomFactor = prevZoomFactor;

            auto xPos = gridData.getXFromTime( initialTick );
            auto yPos = gridData.getYFromNote( initialNote );
            viewPort.setViewPosition( isHorizontalZoom ? xPos : viewPos.x,
                                      isVerticalZoom ? yPos : viewPos.y );
        }
        else if( activeTouches.size() == 1 ) // For Scroll
        {
            auto previousPosition = previousTouchPositions[touchIndex];
            auto delta            = currentPosition - previousPosition;

            auto viewPos = viewPort.getViewPosition();
            viewPort.setViewPosition( viewPos + juce::Point<int>( -delta.x, -delta.y ) );

            previousTouchPositions[touchIndex] = currentPosition;
        }
    }

}
