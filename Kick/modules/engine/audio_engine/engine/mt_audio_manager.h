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

#include "mt_audio_metronome.h"
#include "mt_audio_recorder.h"
#include "mt_audio_manager_program.h"
#include "mt_audio_manager_sequence.h"
#include "mt_audio_manager_transport.h"

namespace mt
{
    class MTSequence;
    class MTTrack;
    class AudioDevice;

    //
    class MTAudioManager
        : public projectManager
        , public MTTempoMgr
        , public MTMidiDeviceManager
        , public MTAudioRecorderManager
        , public MTAudioAssetManager
        , public MTSongManager
        , public MTAudioProgramManager
        , public MTAudioTransportManager
        , public MTIPadPerformanceData
    {
    public:
        MTAudioManager( const char* PrjName, const char* PrjManufacturer, const char* PrjVersion );
        virtual ~MTAudioManager();

        void addListener( MTAudioInterfaceListener* l );
        void removeListener( MTAudioInterfaceListener* l );

        // Project API's..
        bool isProjectLoaded() const noexcept { return projectLoaded; };
        bool loadProject( juce::File& ProjectFile );
        bool closeProject();
        bool saveProject();

        // State
        inline juce::UndoManager& getUndoManager() noexcept { return undoManager; }
        inline juce::ValueTree&   getValueTree() noexcept { return vTree; }

        // Selection API's...
        inline const MTSelected&   getSelected() { return selected; };
        inline const juce::String& getSelectedStr() { return selectedStr; };
        void                       setSelected( MTSelected Selected );

        // Creators
        MTSampleEditorInterface* createSampleEditor();

        // Utilities...
        void triggerTrackNotesChanged( int Sequence, int Track );
        void triggerTrackProgramChanged( int Track, MTIProgram* Program );

        // Device manager...
        juce::AudioDeviceManager& getAudioDeviceManager();

        // Bus Manager
        MTBusManager& getBusManager() { return busMgr; }

        //Song Utilities
        void playSong( const juce::String& SongName, int SequenceIndexToPlay );
        void resumeSong();
        void stopSong();

        // Preview Utilities...
        void        createPreview( const juce::File& F, float Gain );
        void        stopPreview();
        void        loopPreview( bool Loop );
        void        setPreviewMute( bool Mute );
        bool        isPreviewRunning();
        bool        isPreviewLooping();
        bool        isPreviewMuted();
        void        setPreviewGain( float Gain );
        MTMeterData readPreviewMeter();

        void                setBank( const juce::String& Bank );
        inline juce::String getBank() const { return bank; };

        void performUndo();
        void performRedo();

        // Automation mode (automation::Mode: 0 Off, 1 Read, 2 Write), global. Readable from any thread (tick thread
        // included); the setter runs on the message thread and re-arms every track.
        inline int getAutomationMode() const noexcept { return automationMode.load( std::memory_order_relaxed ); }
        void       setAutomationMode( int Mode );
        // Clears the level lane of the tracks in Scope (0 = selected track, 1 = selected sequence, 2 = all). One undo step.
        void clearAutomation( int Scope );
        // PAN lane arbitration (message thread). True when Track is the lowest-ordered (sequence index, track index)
        // track that is playing Prog: the only one that records a pan take for it in Write mode. Called from a
        // non-message thread it answers true (no scan: the sequence list is not safe to walk there); the Read rule
        // below still keeps several lanes from fighting.
        bool isPanLaneOwner( const MTTrack* Track, const MTIProgram* Prog );

    private:
        void interTimerCallbackHandler();
        // Read mode, message thread (25 Hz timer): applies the pan values the tick threads published (MTTrack::
        // takePanTarget) to the program's pan property. Per program the lowest (sequence, track) publisher wins this pass.
        void applyAutomatedPans();
        // Leaving Read: puts back the pan the programs had before playback overwrote it.
        void restoreAutomatedPans();
        void restoreAutomatedPan( const juce::String& ProgramId ); // one program (lane cleared / reloaded empty)
        std::map<juce::String, double> panBases; // program id -> stored pan before Read touched it (message thread)

    private:
        AudIO::DawEngine engine{ AUDIO_NUM_INPUTS, AUDIO_NUM_OUTPUTS, AUDIO_NUM_SEND_RETURNS, AUDIO_NUM_SUBMIX, AUDIO_NUM_AUX };

        juce::ValueTree   vTree{ IDs::ROOT };
        juce::String      selectedStr{ "" };
        juce::String      bank{ "A" };
        juce::UndoManager undoManager;

        juce::ListenerList<MTAudioInterfaceListener> listeners;

        std::unique_ptr<AudioDevice> dm;
        std::unique_ptr<MTMetronome> _metronome;

        MTBusManager busMgr{ *engine.getBusManager() };
        MTSelected   selected{ -1, -1, -1 };

        AudioPreview*    _previewer{ nullptr };
        KickPrivateTimer internalTimer;

        bool projectLoaded{ false };
        std::atomic<int> automationMode{ 0 };

#if defined( USE_VALUE_TREE_DEBUGGER ) && ( USE_VALUE_TREE_DEBUGGER > 0 )
        jcf::ValueTreeDebugger debugger;
#endif

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( MTAudioManager )
    };
} // namespace MT
