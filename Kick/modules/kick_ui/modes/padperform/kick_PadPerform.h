#pragma once
#include "contracts/dcl/dcl_signals.h"
#include "contracts/dcl/dcl_contracts_helper.h"
#include "kick_pad_mapper.h"

namespace kick_ui
{
    // Pad Perform ("Pad Notes"): the 16 hardware pads and the on-screen pads play the SELECTED track as a pitch map.
    // What a pad plays comes from the JUCE-free kick_pad_mapper.h (layout, chords, held-note table, 16 LEVELS).
    class PadPerformContent : public juce::Component
        , private juce::Timer
    {
    public:
        PadPerformContent();
        ~PadPerformContent() override;

        void knobCommand( int knob, int value );

        // Hardware pad: padNo 0..15 inside the engine's current bank.
        void hardwarePad( int padNo, bool pressed, int velocity );
        // PP-22: Pad aftertouch for sounding notes
        bool isPadHeld( int padNo ) const;
        void padAftertouch( int padNo, int pressure );
        // Every note that is still on goes off (screen hidden, panic).
        void releaseAllNotes();

        void toggleSixteenLevels();
        bool isSixteenLevels() const { return level16_; }

    private:
        enum enumNoteTypes
        {
            EN_TYPE_OFF = 0,
            EN_TYPE_CHROMATIC,
            EN_TYPE_NOTES,
            EN_TYPE_CHORDS,
            EN_TYPE_PROGRESSION,
            EN_TYPE_CHROMATIC_CHORDS
        };

        mt::MTAudioInterface& ae{ mt::getAudioInterface() };

        int bankIndex{ 0 };
        int selectedType{ (int)EN_TYPE_CHROMATIC };

        std::unique_ptr<ui::KickPadButtonGroupComponent> pads;
        ui::KickLabel                                    scale_octave{ "SCALE AND OCTAVE" };
        ui::KickListBox                                  rootNote{ ui::KickListBox::BoxType::withTitleBar, "ROOT NOTE", mt::Note::rootNames(), 0, 3 };
        ui::KickListBox                                  scale{ ui::KickListBox::BoxType::withTitleBar, "SCALE", mt::ScaleAndNotes::scaleNames, 0, 3 };
        ui::KickListBox                                  chord{ ui::KickListBox::BoxType::withTitleBar, "CHORDS", mt::ScaleAndNotes::Chord, 0, 2 };
        ui::KickListBox                                  startOn{ ui::KickListBox::BoxType::withTitleBar, "BANKS", mt::Kick::PadPerformStartOn };
        ui::KickListBox                                  octave{ ui::KickListBox::BoxType::withTitleBar, "START ON OCTAVE", mt::ScaleAndNotes::octave, 0, 3 };

        ui::KickImageButton bankInc{ "Inc", "Plus", "Plus_Active" };
        ui::KickImageButton bankDec{ "Dec", "Minus", "Minus_Active" };
        ui::KickLabel       bank{ "A" };

        ui::KickToggleButton chromaticBtn{ "Chromatic", "Chromatic", "Chromatic" };
        ui::KickToggleButton notesBtn{ "Notes", "Notes", "Notes" };
        ui::KickToggleButton chordsBtn{ "Chords", "Chords", "Chords" };
        ui::KickToggleButton progressionBtn{ "Progressions", "Progressions", "Progressions" };

        // 16 LEVELS: the last pad you hit on all 16 pads, one parameter stepped (Velocity or Tune).
        ui::KickToggleButton levelBtn{ "Level16", "16 LEVELS", "16 LEVELS" };
        ui::KickToggleButton levelParamBtn{ "LevelParam", "Velocity", "Tune" };
        ui::KickLabel        heldLabel{ "" }; // note / chord name of the pads that are held

        // PP-25: Arpeggiator / Latch controls
        ui::KickToggleButton arpBtn{ "ARP", "ARP ON", "ARP OFF" };
        ui::KickToggleButton latchBtn{ "LATCH", "LATCH ON", "LATCH OFF" };
        ui::KickListBox      arpMode{ ui::KickListBox::BoxType::withTitleBar, "ARP MODE", { "Up", "Down", "UpDown", "Random" }, 0, 1 };
        ui::KickListBox      arpRate{ ui::KickListBox::BoxType::withTitleBar, "ARP RATE", { "1/4", "1/8", "1/16", "1/32", "1/4T", "1/8T", "1/16T", "1/32T" }, 2, 2 };
        ui::KickNumberT<int> arpOctaves{ "ARP OCT", 1, 4, 1 };
        ui::KickNumberT<int> arpGate{ "ARP GATE %", 5, 100, 50 };

        mt::MTIPadPerformanceData* pPadPerform{ nullptr };

        padmap::Params    cur_;
        padmap::Layout    layout_{};
        sigslot::connection bankConn_;
        padmap::HeldNotes held_;
        bool              level16_{ false };
        bool              levelParamTune_{ false };
        int               levelBase_{ -1 };
        int               lastBaseNote_{ -1 };

        std::map<int, mt::MTSelected> keyTarget_; // held key -> the track it started on (its note-off goes there)
        std::map<int, int>            keySlot_;   // held key -> pad slot (highlight)
        std::vector<int>              lastHwColours_;
        std::array<int, 128>          lastKgColours_{};
        bool                          kgColoursValid_{ false };

        padmap::Params params();
        void           setMode( int mode );
        void           stepMode( int direction );

        void padDown( int key, int slot, int velocity );
        void padUp( int key );
        void sendNote( const mt::MTSelected& target, bool on, int note, int velocity );
        void updateHeldDisplay();

        void requestRefresh();
        void timerCallback() override;
        void refresh();
        void refreshData();
        void pushToUI();
        void pushToHW();
        void pushToEngineColours();
        void updateLevelUi();
        static int hwColour( padmap::Mark mark );

        void paint( juce::Graphics& g ) override;
        void resized() override;
        void visibilityChanged() override;
    };

    class PadPerform : public kick_ui::IScreen
        , public juce::Component
    {
    public:
        PadPerform();
        ~PadPerform() override = default;

        bool canClose() override { return true; }
        bool requiresControlBar() override { return false; }
        bool requiresStatusBar() override { return true; }
        bool requiresHandleCommand() override { return true; }

        void handleCommand( CPktMgr::HWMsg_t& ) override;

        // 16 LEVEL button (DCL EN_16LEVELS): the hardware button toggles the mode, from any screen.
        void toggleSixteenLevels() { content.toggleSixteenLevels(); }

    private:
        mt::MTAudioInterface& ae{ mt::getAudioInterface() };
        KickModeHeader        header{ "Pad Notes", "Icon_PadPerform" };
        PadPerformContent     content;

        void resized() override;
        void visibilityChanged() override;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( PadPerform )
    };
}
