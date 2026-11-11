#pragma once

namespace kick_ui
{
    class GridMagnifyTool : public IGridTool
    {
    public:
        GridMagnifyTool( GridComponent& gridComponent, GridData& data, juce::Viewport& ViewPort );

        void paint( juce::Graphics& g ) override;

        void mouseDrag( const juce::MouseEvent& event ) override;
        void mouseUp( const juce::MouseEvent& event ) override;
        void mouseDown( const juce::MouseEvent& event ) override;
        void cancelGesture() override;

    private:
        grid_logic::TouchPoint previousVpPos;
        bool                   hasActivePan{ false };
        float                  smoothViewX{ 0.0f };
        float                  smoothViewY{ 0.0f };
        int                    lastSetViewX{ -1 };
        int                    lastSetViewY{ -1 };
    };
}
