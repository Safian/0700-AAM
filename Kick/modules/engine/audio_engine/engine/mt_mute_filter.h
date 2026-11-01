/*
 * Kick - Behringer KICK drum machine software.
 * Licensed under AGPL-3.0-or-later; see the repository LICENSE.
 */
#pragma once
// JUCE-free: which live MIDI messages a muted track still forwards to its program (G5). A muted track must not start
// anything new, but it must let go of what it already started: note-off, pitch wheel (back to centre), sustain CC64
// (pedal release) and all-notes-off CC123 - otherwise a mute while a key/pedal is held leaves hanging notes.
namespace mt::mutefilter
{
    // status = raw MIDI status byte (0x80..0xFF, channel bits ignored), data1 = first data byte.
    constexpr bool passesWhileMuted( int status, int data1 )
    {
        const int type = status & 0xF0;
        if( type == 0x80 ) return true;                        // note-off
        if( type == 0xE0 ) return true;                        // pitch wheel
        if( type == 0xB0 ) return data1 == 64 || data1 == 123; // sustain, all notes off
        return false;                                          // note-on, aftertouch, other CC, program change...
    }
} // namespace mt::mutefilter
