// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace ui
{
    class KickQrCode : public juce::Component
    {
    public:
        KickQrCode();
        ~KickQrCode() override = default;

        void setText( const juce::String& text );
        const juce::String& getText() const noexcept { return currentText; }

        void setCardBackground( juce::Colour col ) { cardBg = col; repaint(); }
        void setModuleColour( juce::Colour col ) { moduleCol = col; repaint(); }
        void setCornerRadius( float r ) { cornerRadius = r; repaint(); }

        void paint( juce::Graphics& g ) override;

    private:
        juce::String currentText;
        std::vector<std::vector<bool>> matrix;
        int qrSize{ 0 };
        juce::Colour cardBg{ juce::Colours::white };
        juce::Colour moduleCol{ juce::Colours::black };
        float cornerRadius{ 8.0f };

        void updateMatrix();
    };
}
