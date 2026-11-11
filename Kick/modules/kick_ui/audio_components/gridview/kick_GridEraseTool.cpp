
namespace kick_ui
{
    GridEraseTool::GridEraseTool( GridComponent& gridComponent, GridData& gData, juce::Viewport& ViewPort )
        : IGridTool( gridComponent, gData, GridToolType::GridEraseTool, ViewPort )
    {}

    void GridEraseTool::paint( juce::Graphics& )
    {}

    void GridEraseTool::mouseDown( const juce::MouseEvent& event )
    {
        lastDragPos      = event.position;
        hasLastDragPos   = true;
        gestureCancelled = false;
    }

    bool GridEraseTool::eraseAlongSegment( juce::Point<float> from, juce::Point<float> to )
    {
        const float dist  = from.getDistanceFrom( to );
        const int   steps = juce::jlimit( 1, 64, static_cast<int>( std::ceil( dist / 4.0f ) ) );
        bool        erasedAny = false;

        for( int i = 0; i <= steps; ++i )
        {
            const float        t   = static_cast<float>( i ) / static_cast<float>( steps );
            const juce::Point<float> pt( from.x + ( to.x - from.x ) * t,
                                         from.y + ( to.y - from.y ) * t );
            if( eraseNoteAtPosition( pt, 0.0f, false ) )
                erasedAny = true;
        }
        return erasedAny;
    }

    void GridEraseTool::mouseUp( const juce::MouseEvent& event )
    {
        if( gestureCancelled )
        {
            gestureCancelled = false;
            hasLastDragPos   = false;
            return;
        }

        if( !event.mouseWasDraggedSinceMouseDown() )
        {
            constexpr float kTapSlopPx = 6.0f;
            if( !eraseNoteAtPosition( event.mouseDownPosition, kTapSlopPx, true ) )
                eraseNoteAtPosition( event.position, kTapSlopPx, true );
            gridNeedsRepaint();
        }
        else
        {
            const auto from = hasLastDragPos ? lastDragPos : event.mouseDownPosition;
            if( eraseAlongSegment( from, event.position ) )
                gridNeedsRepaint();
        }

        hasLastDragPos = false;
    }

    void GridEraseTool::mouseDrag( const juce::MouseEvent& event )
    {
        if( gestureCancelled )
            return;

        if( event.mouseWasDraggedSinceMouseDown() )
        {
            const auto from = hasLastDragPos ? lastDragPos : event.mouseDownPosition;
            if( eraseAlongSegment( from, event.position ) )
                gridNeedsRepaint();
            lastDragPos    = event.position;
            hasLastDragPos = true;
        }
    }

    void GridEraseTool::cancelGesture()
    {
        gestureCancelled = true;
        hasLastDragPos   = false;
    }

} //namespace mt
