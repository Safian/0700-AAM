/*
 * Copyright (c) 2021 MUSIC Tribe
 *
 * This file is part of Kick.
 *
 * Kick is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Kick is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Kick. If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once
#include "kick_assign_sample_logic.h"
#include <kick_ui_selectors/source/kick_headerBarComponents.h>
#define NOT_USED 0
namespace kick_ui
{
    enum PadAssignedSampleStatus
    {
        Pad_Assigned,
        Pad_NotAssigned,
    };

    class AssignSampleContent : public juce::Component
    {
    public:
        AssignSampleContent();
        ~AssignSampleContent() = default;
        void sendAssignedSamplesAtStartup( void );
        void handleCommand( CPktMgr::HWMsg_t& f_HwMsg );

        std::unique_ptr<ui::KickPadButtonGroupComponent> pads;
        std::vector<mt::MTISample*>                      samples;
        void                                             assignSampleIntoPad( int Index );
        void                                             confirmClearEntirePad( int padIndex );
        void                                             confirmDeleteSample();
        void                                             refreshLEDs();

    private:
        mt::MTAudioInterface&     ae;
        ui::KickList              fileList{ "Samples", {}, 0, 1 };
        std::vector<juce::String> files;
        ui::KickLabel             sampleLabel{ "Samples" };
        void                      audition( juce::File f );
        void                      scheduleAudition( int listIndex );
        void                      auditionListIndex( int listIndex );
        static constexpr int      kAuditionDelayMs{ 150 };
        int                       currentLayer{ 0 };
        assign_logic::AuditionDebounce auditionDebounce;
        ui::KickToggleOptions     layerSelector{ "Layer" };
        ui::KickButton            assignButton{ "Assign" };
        ui::KickButton            clearPadButton{ "Clear Pad" };
        ui::KickToggleButton      auditionToggle{ "Audition", "Audition", "Audition" };
        ui::KickSlider            auditionGain{ "Gain", juce::Slider::LinearBar, juce::Slider::TextBoxRight };
        ui::KickButton            deleteSampleButton{ "Delete Sample" };

        void                      updateLabelText();
        void                      resized() override;
        void                      paint( juce::Graphics& ) override;
        void                      visibilityChanged() override;
        void                      refresh();
        void                      mouseDown( const juce::MouseEvent& e ) override;
        void                      mouseDrag( const juce::MouseEvent& e ) override;
        void                      mouseUp( const juce::MouseEvent& e ) override;
    };
#if NOT_USED
    //Nowhere it is using as of now
    class DrumAssignSampleContent : public juce::Component
    {
    public:
        DrumAssignSampleContent();
        void sendAssignedSamplesAtStartup( void );

    private:
        mt::MTAudioInterface&                            ae;
        std::unique_ptr<ui::KickPadButtonGroupComponent> pads;
        ui::KickListBox                                  padListBox{ ui::KickListBox::BoxType::withoutTitleBarWithNavigationButtons, "Pad1", { "A01" } };
        ui::KickList                                     fileList{ "Samples" };
        std::vector<juce::String>                        files;
        std::vector<mt::MTSampleData>                    samples;

        void resized() override;
        void visibilityChanged();
        void refresh();
        void refreshLEDs();

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( DrumAssignSampleContent )
    };

    class SamplesList : public juce::Component
    {
    public:
        ui::KickTableDataProviders     provider;
        std::unique_ptr<ui::KickTable> table;

        SamplesList();

    private:
        void paint( juce::Graphics& g ) override;
        void resized() override;
    };

    class KeysPanel : public juce::Component
    {
    public:
        ui::KickTableDataProviders     provider;
        std::unique_ptr<ui::KickTable> table;
        KeysPanel();

    private:
        void paint( juce::Graphics& g ) override;
        void resized() override;
    };

    class KeysBottomControls : public juce::Component
    {
    public:
        ui::KickButton audition{ "Audition" }, clear{ "Clear" }, load{ "Load" };
        KeysBottomControls();

    private:
        void paint( juce::Graphics& g ) override;
        void resized() override;
        void visibilityChanged() override;
    };

    //Nowhere it is using as of now
    class KeysAssignSampleContent : public juce::Component
    {
    public:
        KeysAssignSampleContent();

        void setAsAudioProgram();
        void setAsKeysProgram();

    private:
        bool                  isKeys{ true };
        mt::MTAudioInterface& ae;
        SamplesList           left;
        KeysPanel             right;
        KeysBottomControls    bottom;

        juce::String                  selectedAsset;
        std::vector<mt::MTSampleData> samples;
        int                           numSamples{ 0 }, selectedRow{ -1 };

        void paint( juce::Graphics& g ) override;
        void resized() override;
        void visibilityChanged() override;

        void refreshSampleList();

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( KeysAssignSampleContent )
    };
#endif
    class AppAssignSample : public juce::Component
        , public kick_ui::IScreen
    {
    public:
        AppAssignSample();

        bool requiresControlBar() override { return false; }
        bool requiresStatusBar() override { return true; }
        bool requiresHandleCommand() override { return true; }

        void handleCommand( CPktMgr::HWMsg_t& ) override;

        void startup( void ) { drum.sendAssignedSamplesAtStartup(); }

    private:
        kick_ui::KickModeHeader header{ "Sample Assign", "sEdit_Icon" };
        HeaderBarTrackSelector  trackSelector;
        AssignSampleContent     drum;

        ui::KickImageButton fileBrowser{ "File Browser", "Browser_Recent" };
        ui::KickImageButton leftNav{ "Left Nav", "Icon_minus" };
        ui::KickImageButton rightNav{ "Right Nav", "Icon_plus" };
        ui::KickLabel       bankLabel{ "" };

        void resized() override;
        void visibilityChanged() override;
        void setBank( int, juce::String );

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( AppAssignSample )
    };
}
