/*
 * Copyright (c) 2024 MUSIC Tribe / Safian
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

// Startup state: one thread-safe source of truth for the boot sequence (audio engine, MIDI, display, project). The
// startup screen, the problem dialogs and the log read it; the engine, the hardware bridge and the project loader write
// it. JUCE-free and header only, so the state machine and the messages are host-testable (tests/platform/StartupTests.cpp).

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace kick_startup
{
    enum class Step : int
    {
        AudioEngine = 0,
        Midi,
        Display,
        Project,
        Count
    };

    enum class StepState : int
    {
        Pending = 0, // nobody reported yet
        Running,
        Ok,
        Failed,
        Skipped // not applicable on this platform / switched off
    };

    // What went wrong. Every kind maps to a cause and a "what to do" text (describeProblem).
    enum class Fault : int
    {
        AudioDevice,
        AudioDeviceMissing,
        MidiPort,
        Touch,
        Framebuffer,
        ProjectLoad,
        SafeMode
    };

    struct StepInfo
    {
        StepState   state{ StepState::Pending };
        std::string detail;
    };

    struct Problem
    {
        Fault       fault{ Fault::AudioDevice };
        std::string title;  // dialog title
        std::string cause;  // what happened (includes the raw detail)
        std::string action; // what the user can do
        std::string text() const { return cause + "\n" + action; }
    };

    inline const char* stepLabel( Step s )
    {
        switch( s )
        {
            case Step::AudioEngine: return "Audio engine";
            case Step::Midi: return "MIDI";
            case Step::Display: return "Display";
            case Step::Project: return "Project";
            default: return "?";
        }
    }

    // "Audio engine... OK (ALSA)", "MIDI... FAILED: no port", "Project... loading Demo 01", "Display... skipped"
    inline std::string stepLine( Step s, const StepInfo& i )
    {
        std::string line = std::string( stepLabel( s ) ) + "...";
        const std::string d = i.detail.empty() ? std::string() : i.detail;
        switch( i.state )
        {
            case StepState::Pending: break;
            case StepState::Running: line += d.empty() ? " working" : " " + d; break;
            case StepState::Ok: line += d.empty() ? " OK" : " OK (" + d + ")"; break;
            case StepState::Failed: line += d.empty() ? " FAILED" : " FAILED: " + d; break;
            case StepState::Skipped: line += d.empty() ? " skipped" : " skipped (" + d + ")"; break;
        }
        return line;
    }

    inline Step stepOfFault( Fault f )
    {
        switch( f )
        {
            case Fault::AudioDevice:
            case Fault::AudioDeviceMissing: return Step::AudioEngine;
            case Fault::MidiPort: return Step::Midi;
            case Fault::Touch:
            case Fault::Framebuffer: return Step::Display;
            case Fault::ProjectLoad:
            case Fault::SafeMode: return Step::Project;
        }
        return Step::Project;
    }

    inline Problem describeProblem( Fault f, const std::string& detail )
    {
        Problem p;
        p.fault               = f;
        const std::string raw = detail.empty() ? std::string() : " (" + detail + ")";
        switch( f )
        {
            case Fault::AudioDevice:
                p.title  = "AUDIO DEVICE";
                p.cause  = "The audio device could not be opened" + raw + ". There will be no sound.";
                p.action = "Check the audio output and restart Kick.";
                break;
            case Fault::AudioDeviceMissing:
                p.title  = "AUDIO DEVICE";
                p.cause  = "No audio output device was found" + raw + ". There will be no sound.";
                p.action = "Connect or enable an audio output and restart Kick.";
                break;
            case Fault::MidiPort:
                p.title  = "MIDI";
                p.cause  = "The MIDI control surface could not be opened" + raw + ". Buttons, pads and keys will not respond.";
                p.action = "Check the USB/MIDI connection and restart Kick. The touch screen still works.";
                break;
            case Fault::Touch:
                p.title  = "TOUCH SCREEN";
                p.cause  = "The touch screen could not be opened" + raw + ". Touch input will not work.";
                p.action = "Use the hardware buttons and the data dial, then restart Kick.";
                break;
            case Fault::Framebuffer:
                p.title  = "DISPLAY";
                p.cause  = "The display could not be opened" + raw + ". The screen may stay black.";
                p.action = "Restart the device. If it persists, reinstall Kick.";
                break;
            case Fault::ProjectLoad:
                p.title  = "PROJECT";
                p.cause  = "The project could not be loaded" + raw + ".";
                p.action = "Pick another project from the Projects list. The file was not changed.";
                break;
            case Fault::SafeMode:
                p.title  = "SAFE MODE";
                p.cause  = "Kick crashed on the last two starts" + raw + ". It started without a project.";
                p.action = "Open a project from the Projects list. Send the crash file to support.";
                break;
        }
        return p;
    }

    // Build identity shown on the startup screen. Source of truth: the "Kick.version" file that the pendrive installer
    // copies from the package's VERSION file (KEY=VALUE lines VERSION, GIT_COMMIT, BUILD_DATE); otherwise what was compiled in.
    struct BuildInfo
    {
        std::string version;
        std::string commit;
        std::string date;
        bool        fromInstallFile{ false };
        std::string text() const
        {
            std::string t = version.empty() ? std::string( "dev" ) : version;
            if( !commit.empty() ) t += " (" + commit + ")";
            if( !date.empty() ) t += " " + date;
            return t;
        }
    };

    inline std::string trimmed( const std::string& s )
    {
        const char* ws = " \t\r\n\"";
        const auto  b  = s.find_first_not_of( ws );
        if( b == std::string::npos ) return {};
        const auto e = s.find_last_not_of( ws );
        return s.substr( b, e - b + 1 );
    }

    inline BuildInfo parseVersionFile( const std::string& text )
    {
        BuildInfo b;
        size_t    pos = 0;
        while( pos <= text.size() )
        {
            auto nl = text.find( '\n', pos );
            if( nl == std::string::npos ) nl = text.size();
            const auto line = text.substr( pos, nl - pos );
            pos             = nl + 1;
            const auto eq   = line.find( '=' );
            if( eq == std::string::npos ) continue;
            const auto key = trimmed( line.substr( 0, eq ) );
            const auto val = trimmed( line.substr( eq + 1 ) );
            if( key == "VERSION" ) b.version = val;
            else if( key == "GIT_COMMIT" ) b.commit = val.substr( 0, 12 );
            else if( key == "BUILD_DATE" ) b.date = val;
        }
        b.fromInstallFile = !b.version.empty();
        return b;
    }

    inline BuildInfo resolveBuildInfo( const std::string& installFileText, const std::string& compiledVersion, const std::string& compiledCommit, const std::string& compiledDate )
    {
        auto b = parseVersionFile( installFileText );
        if( b.fromInstallFile ) return b;
        b.version = compiledVersion;
        b.commit  = compiledCommit;
        b.date    = compiledDate;
        return b;
    }

    struct Snapshot
    {
        StepInfo             steps[(int)Step::Count];
        std::vector<Problem> problems; // all, in order of arrival
        bool                 safeMode{ false };
        std::string          crashPath;
        uint32_t             revision{ 0 }; // grows on every change (cheap "did it change" for the UI)
    };

    // Thread-safe state. Writers: any thread. The UI polls snapshot() on the message thread.
    class StartupState
    {
    public:
        void set( Step s, StepState st, const std::string& detail = {} )
        {
            std::lock_guard<std::mutex> lk( m );
            snap.steps[(int)s] = { st, detail };
            ++snap.revision;
        }

        // Marks the step failed and records the problem (text for the dialog). Returns it so the caller can log it.
        Problem fail( Fault f, const std::string& detail )
        {
            auto p = describeProblem( f, detail );
            std::lock_guard<std::mutex> lk( m );
            snap.steps[(int)stepOfFault( f )] = { StepState::Failed, detail };
            snap.problems.push_back( p );
            ++snap.revision;
            return p;
        }

        void setSafeMode( bool on, const std::string& crashPath )
        {
            std::lock_guard<std::mutex> lk( m );
            snap.safeMode  = on;
            snap.crashPath = crashPath;
            ++snap.revision;
        }

        // Dialog queue: each problem is handed out once.
        bool takeUnshownProblem( Problem& out )
        {
            std::lock_guard<std::mutex> lk( m );
            if( shown >= snap.problems.size() ) return false;
            out = snap.problems[shown++];
            return true;
        }

        Snapshot snapshot() const
        {
            std::lock_guard<std::mutex> lk( m );
            return snap;
        }

        StepInfo info( Step s ) const
        {
            std::lock_guard<std::mutex> lk( m );
            return snap.steps[(int)s];
        }

        bool safeMode() const
        {
            std::lock_guard<std::mutex> lk( m );
            return snap.safeMode;
        }

        // Done = every step reported a final state. The startup screen leaves when this is true.
        bool isComplete() const
        {
            std::lock_guard<std::mutex> lk( m );
            for( const auto& i : snap.steps )
                if( i.state == StepState::Pending || i.state == StepState::Running ) return false;
            return true;
        }

        void reset()
        {
            std::lock_guard<std::mutex> lk( m );
            snap  = Snapshot();
            shown = 0;
        }

    private:
        mutable std::mutex m;
        Snapshot           snap;
        size_t             shown{ 0 };
    };

    inline StartupState& state()
    {
        static StartupState s;
        return s;
    }
}
