#include "kick_XYFXGrid.h"
namespace ui
{
#define totalHorizontalCells 8
#define totalVerticalCells   8
#define spacing              2
    XYFXGrid::XYFXGrid()
        : selectedCells( 64 )
    {
    }

    void XYFXGrid::paint( juce::Graphics& g )
    {
        if( isEnabled() )
            paintCells( g );
        else
            g.fillAll( ui::Theme::XYFXCellColour() );
    }

    void XYFXGrid::paintCells( juce::Graphics& g )
    {
        for( unsigned int row = 0; row < totalHorizontalCells; row++ )
        {
            for( unsigned column = 0; column < totalVerticalCells; column++ )
            {
                unsigned int index = row * totalHorizontalCells + column;
                if( selectedCells[index] > 100 )
                {
                    g.setColour( ui::Theme::XYFXCellSelectColour( selectedCells[index] ) );
                }
                else
                {
                    g.setColour( ui::Theme::XYFXCellColour() );
                }

                const juce::Rectangle<float> cellRect = getCellRect( (int)row, (int)column );
                g.fillRect( cellRect );
            }
        }
        // The latched cell is the last one touched (currentIndex is -1 after the finger left: it drew column -1).
        if( isLatch && !isTimerRunning() && prevSel >= 0 )
        {
            const juce::Rectangle<float> cellRect = getCellRect( prevSel / 8, prevSel % 8 );
            g.setColour( ui::Theme::XYFXCellSelectColour( 255 ) );
            g.fillRect( cellRect );
        }

        float yAxisPos = cellHeight * 4.0f + spacing * 3.0f;
        mt::utils::fillRoundedRect( g, { 0, (int)yAxisPos - 1, getWidth(), 3 }, ui::Theme::XYFXAxisColour(), 0, false, false, false, false );
        float xAxisPos = cellWidth * 4.0f + spacing * 3.0f;
        mt::utils::fillRoundedRect( g, { (int)xAxisPos, 0, 3, getHeight() }, ui::Theme::XYFXAxisColour(), 0, false, false, false, false );

        float xpos = (float)cellWidth;
        for( int i = 0; i < 7; i++ ) //xaxis  markings
        {
            mt::utils::fillRoundedRect( g, { (int)xpos, (int)yAxisPos - 14, 2, 30 }, ui::Theme::XYFXAxisColour(), 0, false, false, false, false );
            xpos += cellWidth + 2;
        }

        float ypos = (float)cellHeight;
        for( int i = 0; i < 7; i++ ) //xaxis  markings
        {
            mt::utils::fillRoundedRect( g, { (int)xAxisPos - 14, (int)ypos, 30, 2 }, ui::Theme::XYFXAxisColour(), 0, false, false, false, false );
            ypos += cellHeight + 2;
        }

        g.setColour( ui::Theme::textColour( 1 ) );
        g.setFont( 15.0f );
        g.drawText( xLabel.getText(), 0, yAxisPos - 20, 100, 20, juce::Justification::left );
        g.drawText( xLeft, 0, yAxisPos, 100, 20, juce::Justification::centredLeft );
        g.drawText( xLabel.getText(), getWidth() - 100, yAxisPos - 20, 100, 20, juce::Justification::right );
        g.drawText( xRight, getWidth() - 100, yAxisPos, 100, 20, juce::Justification::centredRight );
        g.drawText( yLabel.getText(), xAxisPos + 10, getHeight() - 40, 100, 20, juce::Justification::left );
        g.drawText( yBottom, xAxisPos + 10, getHeight() - 20, 100, 20, juce::Justification::left );
        g.drawText( yLabel.getText(), xAxisPos + 10, 0, 100, 20, juce::Justification::left );
        g.drawText( yTop, xAxisPos + 10, 20, 100, 20, juce::Justification::left );
    }

    void XYFXGrid::resized()
    {
        cellWidth  = ( getWidth() - ( totalHorizontalCells - 1.0f ) * spacing ) / totalHorizontalCells;
        cellHeight = ( getHeight() - ( totalVerticalCells - 1.0f ) * spacing ) / totalVerticalCells;
    }

    juce::Rectangle<float> XYFXGrid::getCellRect( int row, int column )
    {
        float xStart = column * cellWidth + ( column * spacing );
        float yStart = row * cellHeight + ( row * spacing );
        return juce::Rectangle<float>( xStart, yStart, cellWidth, cellHeight );
    }

    void XYFXGrid::mouseDown( const juce::MouseEvent& event )
    {
        if( isEnabled() )
        {
            updateSelection( event );
            if( onTouch )
                onTouch( true, xCoordinate, yCoordinate );
        }
    }

    void XYFXGrid::mouseUp( const juce::MouseEvent& )
    {
        if( isEnabled() )
        {
            currentIndex = -1;
            if( onTouch )
                onTouch( false, xCoordinate, yCoordinate );
        }
    }

    void XYFXGrid::clearHighlight()
    {
        stopTimer();
        std::fill( selectedCells.begin(), selectedCells.end(), 0 );
        currentIndex = prevSel = -1;
        repaint();
    }

    void XYFXGrid::mouseDrag( const juce::MouseEvent& event )
    {
        if( isEnabled() )
            updateSelection( event );
    }

    void XYFXGrid::timerCallback()
    {
        bool valueChanged = false;
        for( unsigned int i = 0; i < 64; i++ )
        {
            if( selectedCells[i] > 100 )
            {
                selectedCells[i] = selectedCells[i] - 5;
                valueChanged     = true;
            }
        }
        if( !valueChanged )
        {
            stopTimer();
            if( isLatch && prevSel >= 0 )
            {
                selectedCells[(size_t)prevSel] = 255;
                repaint();
            }
        }
        else
            repaint();
    }

    void XYFXGrid::updateSelection( const juce::MouseEvent& event )
    {
        // The same geometry as the drawing (cells with `spacing` between them). The old hit test ignored the gaps, so a
        // touch near the right edge hit a different cell than the one drawn there.
        const auto cell = kick_ui::xyfx::cellAt( event.position.getX(), event.position.getY(), (float)getWidth(), (float)getHeight(), (float)spacing );
        if( cell.column < 0 )
            return;
        const int index = cell.row * totalHorizontalCells + cell.column;

        xCoord = event.position.getX();
        yCoord = event.position.getY();
        setXYCoordinateValue( xCoord, yCoord );

        if( index != currentIndex )
        {
            currentIndex = index;
            if( prevSel >= 0 && prevSel != index && isLatch )
                selectedCells[(size_t)prevSel] = 0; // only the latest cell stays lit
            prevSel                            = index;
            selectedCells[(unsigned int)index] = 255;
            if( !isTimerRunning() )
            {
                startTimer( 30 );
            }
            repaint();
        }
    }

    void XYFXGrid::setXYCoordinates( std::vector<juce::String> data )
    {
        xLabel.setText( data[0], juce::dontSendNotification );
        yLabel.setText( data[1], juce::dontSendNotification );
        xIsBeat = ( data[0] == kick_ui::xyfx::axisName( kick_ui::xyfx::Axis::BeatDivision ) );
        if( xIsBeat )
        {
            xLeft  = "2 Bar";
            xRight = "1/64";
        }
        else
        {
            xLeft  = "0.000";
            xRight = "1.000";
        }
        yTop    = "1.000";
        yBottom = "0.000";

        setXYCoordinateValue( xCoord, yCoord );
        repaint();
    }

    void XYFXGrid::setAxes( kick_ui::xyfx::Axis X, kick_ui::xyfx::Axis Y )
    {
        using namespace kick_ui::xyfx;
        xLabel.setText( axisName( X ), juce::dontSendNotification );
        yLabel.setText( axisName( Y ), juce::dontSendNotification );
        xIsBeat = ( X == Axis::BeatDivision );
        xLeft   = axisLow( X );
        xRight  = axisHigh( X );
        yBottom = axisLow( Y );
        yTop    = axisHigh( Y );
        setXYCoordinateValue( xCoord, yCoord );
        repaint();
    }

    void XYFXGrid::setXYCoordinateValue( float XCoordinate, float YCoordinate )
    {
        // Before the first layout the grid has no size: normalising would divide by 0 and leave a NaN until the first touch.
        if( getWidth() <= 0 || getHeight() <= 0 )
            return;

        /// X Axis Coordinates
        if( xIsBeat )
        {
            const auto cell = kick_ui::xyfx::cellAt( XCoordinate, 0.f, (float)getWidth(), (float)getHeight(), (float)spacing );
            xCoordinate     = (float)kick_ui::xyfx::divisionOfColumn( cell.column < 0 ? ( XCoordinate <= 0.f ? 0 : totalHorizontalCells - 1 ) : cell.column );
        }
        else
            xCoordinate = kick_ui::xyfx::normalised( XCoordinate, (float)getWidth() );

        /// Y Axis Coordinates
        yCoordinate = 1.0f - kick_ui::xyfx::normalised( YCoordinate, (float)getHeight() );

        if( onValueChanged )
            onValueChanged( xCoordinate, yCoordinate );
    }
}
