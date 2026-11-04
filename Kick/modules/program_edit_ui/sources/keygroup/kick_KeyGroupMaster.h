#pragma once
namespace kick_ui
{
    class KeyGroupMaster : public ui::KickGrid
        , public KeyGroupInterface
    {
    public:
        KeyGroupMaster( std::vector<int>& Keys, mt::MTKeygroupProgram*& pKeyGroup );
        ~KeyGroupMaster() = default;

    private:
        ui::KickRotary levelSlider{ "LEVEL", juce::Slider::TextBoxLeft, true };
        ui::KickRotary panSlider{ "PAN", juce::Slider::TextBoxLeft, true };
        ui::KickRotary semiSlider{ "SEMI", juce::Slider::TextBoxLeft, true };
        ui::KickRotary fineSlider{ "FINE", juce::Slider::TextBoxLeft, true };

        ui::KickToggleButton insertsButton{ "INSERTS", "INSERTS", "INSERTS" };
        ui::KickListBox      outListBox{ "OUT" };

        ui::KickButton       keyTrack{ "KEY TRACK", "KEY TRACK" };
        ui::KickNumberT<int> muteGroup{ "Mute Group", MUTEGROUP::MIN, MUTEGROUP::MAX, MUTEGROUP::DEFAULT, true };
        ui::KickNumberT<int> kgPoly{ "KG POLY", POLYPHONY::MIN, POLYPHONY::MAX, POLYPHONY::DEFAULT, true };

        // Key zone of the selected key's sample (multi-sample keygroups: the key is played by the sample whose zone holds it; the root
        // is the sample's own key, the pad it is assigned to).
        ui::KickNumberT<int> zoneLow{ "ZONE LOW", 0, 127, 0 };
        ui::KickNumberT<int> zoneRoot{ "ROOT NOTE", 0, 127, 60 };
        ui::KickNumberT<int> zoneHigh{ "ZONE HIGH", 0, 127, 127 };

        ui::KickToggleOptions layerPlay{ "LAYER PLAY", progedit::layerPlayOptions() };
        ui::KickToggleOptions samplePlay{ "SAMPLE PLAY", progedit::samplePlayOptions() };

        std::vector<int>&                       keys;
        mt::MTKeygroupProgram*&                 pKeygroup;
        std::shared_ptr<mt::Kick::ScopeTracker> scope;

        void refresh() override;
        void handleCommand( CPktMgr::HWMsg_t& ) override;
        void visibilityChanged() override;
    };
}
