/*
 * Kick - Behringer KICK drum machine software.
 * Licensed under AGPL-3.0-or-later; see the repository LICENSE.
 */
#pragma once

// JUCE-free: the track Monitor setting (Off / In / Auto) and which live MIDI input a track hands to its program.
// "Input" is the external MIDI input (keyboard, controller); the on-screen/hardware pads of the app always sound.
//  - In:   the input always reaches the program.
//  - Off:  the input is not heard (it is still captured for RECALL / REC take, the track records what you play).
//  - Auto: the input is heard while the track is rec-armed, or while the track is not playing back (stopped transport);
//          a playing, un-armed track does not take input.
// A blocked track still lets go of what it started (note-off, all-notes-off, pedal release, bend back to centre),
// otherwise switching the monitor while a key is held leaves hanging notes.
namespace mt::trackmonitor
{
    enum Mode : int
    {
        Off     = 0,
        In      = 1,
        Auto    = 2,
        NumModes = 3,
        Default = In // the behaviour before the setting existed: input always reaches the program
    };

    constexpr int clamp( int Value ) { return ( Value < 0 || Value >= NumModes ) ? static_cast<int>( Default ) : Value; }

    // Button cycle: Off -> In -> Auto -> Off.
    constexpr int next( int Value ) { return ( clamp( Value ) + 1 ) % NumModes; }

    // The messages a blocked track still forwards: only what ends or resets something already started.
    // status = raw MIDI status byte (channel bits ignored), data1/data2 = data bytes (0 if absent).
    constexpr bool isRelease( int status, int data1, int data2 )
    {
        const int type = status & 0xF0;
        if( type == 0x80 ) return true;                              // note-off
        if( type == 0x90 ) return data2 == 0;                        // note-on, velocity 0 = note-off
        if( type == 0xE0 ) return data1 == 0 && data2 == 64;         // pitch wheel back to centre (8192)
        if( type == 0xB0 ) return ( data1 == 64 && data2 < 64 ) || data1 == 123; // pedal release, all notes off
        return false;
    }

    constexpr bool isHeard( int Mode, bool RecArmed, bool TrackPlaying )
    {
        switch( clamp( Mode ) )
        {
            case Off: return false;
            case Auto: return RecArmed || !TrackPlaying;
            default: return true;
        }
    }

    // True when the message must reach the track's program.
    constexpr bool passes( int Mode, bool RecArmed, bool TrackPlaying, int status, int data1, int data2 )
    {
        return isHeard( Mode, RecArmed, TrackPlaying ) || isRelease( status, data1, data2 );
    }
} // namespace mt::trackmonitor
