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

namespace kick_ui
{
    // One list for the Drum and the Keygroup editor, in the order of mt::SamplePlayType / mt::LayerPlayType (the Keygroup
    // page had "Note On" / "Note Off" swapped: choosing "Note On" started the sound on release).
    namespace progedit
    {
        // The sound of a sample for a waveform page. Cached: returned at once. Not cached: it is decoded on a background thread
        // (the page used to freeze the UI on every pad change onto a big file), nullptr is returned for now and `again` runs on the
        // message thread when the sound is loaded, as long as `owner` still exists.
        inline mt::ASound::Ptr soundOrLoadAsync( const juce::String& source, juce::Component* owner, std::function<void()> again )
        {
            if( source.isEmpty() ) return nullptr;
            if( auto p = mt::getAudioSoundCache().exists( source ) ) return p;
            juce::Component::SafePointer<juce::Component> safe( owner );
            juce::Thread::launch( [source, safe, again]() {
                mt::getAudioSoundCache().getOrLoad( source );
                juce::MessageManager::callAsync( [safe, again]() {
                    if( safe != nullptr && again ) again();
                } );
            } );
            return nullptr;
        }

        static_assert( (int)mt::SamplePlayType::ONE_SHOT == 0 && (int)mt::SamplePlayType::NOTE_OFF == 1 && (int)mt::SamplePlayType::NOTE_ON == 2, "samplePlayOptions() follows the enum order" );
        static_assert( (int)mt::LayerPlayType::CYCLE == 0 && (int)mt::LayerPlayType::VELOCITY == 1 && (int)mt::LayerPlayType::RANDOM == 2, "layerPlayOptions() follows the enum order" );

        inline const std::vector<juce::String>& samplePlayOptions()
        {
            static const std::vector<juce::String> v = { "One Shot", "Note Off", "Note On" };
            return v;
        }
        inline const std::vector<juce::String>& layerPlayOptions()
        {
            static const std::vector<juce::String> v = { "Cycle", "Velocity", "Random" };
            return v;
        }
    }

    class ProgEditInterface : public juce::Component
    {
    public:
        std::function<void()> onProgramChanged;

        explicit ProgEditInterface( mt::MTProgramType Type )
            : type( Type ) {}

        mt::MTProgramType getType() const { return type; }
        juce::String      getProgramName() { return pIProg ? pIProg->getName() : ""; }
        virtual void      setProgram( int Index )
        {
            idx    = Index;
            pIProg = ae.getProgram( idx );
            // The program's own property tree (the MIDI editor attaches its CC sliders to it); it stayed empty before.
            state = pIProg ? pIProg->getState() : juce::ValueTree{};

            if( onProgramChanged )
                onProgramChanged();

            refresh();
        }

        virtual void refresh()                                   = 0;
        virtual void setupHeader( kick_ui::KickModeHeader& Hdr ) = 0;
        virtual void handleCommand( CPktMgr::HWMsg_t& ) {}

    protected:
        mt::MTAudioInterface& ae{ mt::getAudioInterface() };
        const mt::MTSelected& aeSel{ ae.getSelected() };
        juce::ValueTree       state;
        mt::MTIProgram*       pIProg{ nullptr };

    private:
        int                     idx{ -1 };
        const mt::MTProgramType type;
    };

}
