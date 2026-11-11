// SPDX-License-Identifier: AGPL-3.0-or-later
#include "kick_qrcode.h"
#include "../../3rdParty/qrcodegen/qrcodegen.hpp"
#include "../../3rdParty/qrcodegen/qrcodegen.cpp"

namespace ui
{
    KickQrCode::KickQrCode()
    {
        setOpaque( false );
    }

    void KickQrCode::setText( const juce::String& text )
    {
        if( currentText == text && qrSize > 0 )
            return;
        currentText = text;
        updateMatrix();
        repaint();
    }

    void KickQrCode::updateMatrix()
    {
        matrix.clear();
        qrSize = 0;
        if( currentText.isEmpty() )
            return;

        try
        {
            auto qr = qrcodegen::QrCode::encodeText( currentText.toRawUTF8(), qrcodegen::QrCode::Ecc::MEDIUM );
            qrSize = qr.getSize();
            matrix.resize( static_cast<size_t>( qrSize ) );
            for( int y = 0; y < qrSize; ++y )
            {
                matrix[static_cast<size_t>( y )].resize( static_cast<size_t>( qrSize ) );
                for( int x = 0; x < qrSize; ++x )
                    matrix[static_cast<size_t>( y )][static_cast<size_t>( x )] = qr.getModule( x, y );
            }
        }
        catch( const std::exception& )
        {
            matrix.clear();
            qrSize = 0;
        }
    }

    void KickQrCode::paint( juce::Graphics& g )
    {
        auto bounds = getLocalBounds().toFloat();
        if( bounds.isEmpty() )
            return;

        // Card background
        g.setColour( cardBg );
        g.fillRoundedRectangle( bounds, cornerRadius );

        if( qrSize <= 0 || matrix.empty() )
        {
            g.setColour( juce::Colours::grey );
            g.setFont( 14.0f );
            g.drawFittedText( "Generating QR Code...", getLocalBounds(), juce::Justification::centred, 1 );
            return;
        }

        // Draw QR modules with quiet zone margin
        const float margin = 10.0f;
        const float availWidth = bounds.getWidth() - margin * 2.0f;
        const float availHeight = bounds.getHeight() - margin * 2.0f;
        const float avail = juce::jmin( availWidth, availHeight );
        const float cellSize = avail / static_cast<float>( qrSize );
        const float startX = bounds.getX() + ( bounds.getWidth() - cellSize * static_cast<float>( qrSize ) ) * 0.5f;
        const float startY = bounds.getY() + ( bounds.getHeight() - cellSize * static_cast<float>( qrSize ) ) * 0.5f;

        g.setColour( moduleCol );
        for( int y = 0; y < qrSize; ++y )
        {
            for( int x = 0; x < qrSize; ++x )
            {
                if( matrix[static_cast<size_t>( y )][static_cast<size_t>( x )] )
                {
                    g.fillRect( startX + static_cast<float>( x ) * cellSize,
                                startY + static_cast<float>( y ) * cellSize,
                                cellSize + 0.4f,
                                cellSize + 0.4f );
                }
            }
        }
    }
}
