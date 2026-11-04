// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "contracts/dcl/dcl_signals.h"
#include "contracts/dcl/dcl_contracts_helper.h"

namespace kick_ui
{
    // Overview strip above the keyboard: all 128 keys in miniature, the visible window highlighted; a drag scrolls.
    class PianoOverview : public juce::Component
    {
    public:
        PianoOverview();
        std::function<void( int firstKey )> onScroll; // lowest visible key requested by the finger
        void                                setWindow( int firstKey, int numKeys );

    private:
        void paint( juce::Graphics& g ) override;
        void mouseDown( const juce::MouseEvent& e ) override;
        void mouseDrag( const juce::MouseEvent& e ) override;
        void scrollTo( const juce::MouseEvent& e );

        int firstKey_{ 48 }, numKeys_{ 36 };
    };

    // "Keyboard": the touch piano. Built on ui::KeyboardComponent (multi-finger, glissando, velocity modes, bend / vibrato)
    // and the pure logic in kick_piano_logic.h. Plays the SELECTED track like the Key 61 keybed, with the same shared
    // transposition (ui::PianoHub <-> the bridge's KeyTranspose) and the root / scale / chord of Pad Perform as marks.
    class PianoView : public kick_ui::IScreen
        , public juce::Component
        , private juce::Timer
    {
    public:
        PianoView();
        ~PianoView() override;

        bool canClose() override { return true; }
        bool requiresControlBar() override { return false; }
        bool requiresStatusBar() override { return true; }
        bool requiresHandleCommand() override { return true; }

        void handleCommand( CPktMgr::HWMsg_t& ) override;

    private:
        struct Sounding
        {
            bool           on{ false };
            int            pitch{ 0 };
            mt::MTSelected sel;
        };

        void resized() override;
        void visibilityChanged() override;
        void paint( juce::Graphics& g ) override;
        void timerCallback() override;

        void keyEvent( const ui::piano::Event& e );
        void sendBend( int value );
        void sendVibrato( int value );
        void setSustain( bool on );
        void stopEverything();
        void applyVelocityMode( ui::piano::VelocityMode m );
        void applyDragMode( ui::piano::DragMode m );
        void refreshTargetInfo();
        void refreshTransposeLabel();
        void refreshMarksAndExternal();

        mt::MTAudioInterface& ae{ mt::getAudioInterface() };
        ui::PianoHub&         hub{ ui::PianoHub::instance() };
        KickModeHeader        header{ "Keyboard", "Icon_PadPerform" };

        juce::MidiKeyboardState keyboardState;
        ui::KeyboardComponent   keys{ keyboardState, ui::KeyboardComponent::horizontalKeyboard };
        PianoOverview           overview;

        ui::KickLabel velTitle{ "VELOCITY" }, dragTitle{ "DRAG" }, bendTitle{ "BEND RANGE" }, fixedTitle{ "FIXED" }, transposeTitle{ "TRANSPOSE" }, viewTitle{ "VIEW" };
        ui::KickLabel fixedValue{ "100" }, transposeValue{ "OCT 0  ST 0" }, targetInfo{ "" };

        ui::KickToggleButton velFixed{ "Fixed", "Fixed", "Fixed" }, velPos{ "Position", "Position", "Position" }, velSpeed{ "Speed", "Speed", "Speed" };
        ui::KickToggleButton dragGlide{ "Glide", "Glide", "Glide" }, dragLegato{ "Legato", "Legato", "Legato" }, dragBend{ "Bend", "Bend", "Bend" }, dragVibrato{ "Vibrato", "Vibrato", "Vibrato" };
        ui::KickToggleButton range2{ "+-2", "+-2", "+-2" }, range4{ "+-4", "+-4", "+-4" }, range7{ "+-7", "+-7", "+-7" }, range12{ "+-12", "+-12", "+-12" };
        ui::KickToggleButton sustain{ "Sustain", "SUSTAIN", "SUSTAIN" };
        ui::KickButton       fixedDown{ "FixedDown", "-" }, fixedUp{ "FixedUp", "+" };
        ui::KickButton       octDown{ "OctDown", "OCT-" }, octUp{ "OctUp", "OCT+" }, semiDown{ "SemiDown", "ST-" }, semiUp{ "SemiUp", "ST+" }, resetTranspose{ "ResetTr", "0" };
        ui::KickButton       viewLeft{ "ViewLeft", "<" }, viewRight{ "ViewRight", ">" }, zoomOut{ "ZoomOut", "-" }, zoomIn{ "ZoomIn", "+" };

        Sounding sounding_[128];
        bool     bendActive_{ false };
        mt::MTSelected bendTarget_;
        int      lastTransposeVersion_{ -1 };
        int      lastSelTrack_{ -1 }, lastSelSeq_{ -1 };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( PianoView )
    };
}
