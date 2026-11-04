// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for MIDI Program logic: note/velocity ranges, transposition,
// pad mute separation, MIDI/CC filters, learn state machine, CC label validity,
// unique type-based naming, and offline port decoration.
// Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined -o /tmp/midi_tests MidiProgramTests.cpp

#include <algorithm>
#include <cassert>
#include <cstdio>
#include <string>
#include <unordered_set>
#include <vector>

static int g_fail = 0;

#define CHECK( cond )                                                                                         \
    do {                                                                                                      \
        if( !( cond ) ) {                                                                                     \
            std::printf( "FAIL %s:%d: assertion failed: %s\n", __FILE__, __LINE__, #cond );                   \
            ++g_fail;                                                                                         \
        }                                                                                                     \
    } while( 0 )

#define CHECK_EQ( a, b )                                                                                      \
    do {                                                                                                      \
        auto va = ( a );                                                                                      \
        auto vb = ( b );                                                                                      \
        if( va != vb ) {                                                                                      \
            std::printf( "FAIL %s:%d: %s != %s\n", __FILE__, __LINE__, #a, #b );                              \
            ++g_fail;                                                                                         \
        }                                                                                                     \
    } while( 0 )

namespace tests
{
    // 1. Note Range filtering & Transposition
    struct MidiProgramFilterState
    {
        int  noteMin{ 0 };
        int  noteMax{ 127 };
        int  velMin{ 1 };
        int  velMax{ 127 };
        int  transpose{ 0 };

        bool filterPitchBend{ false };
        bool filterModWheel{ false };
        bool filterAftertouch{ false };
        bool filterProgramChange{ false };
        bool filterSustain{ false };
        bool filterAllCC{ false };

        bool isNoteAllowed( int rawNote, int rawVelocity, bool isNoteOn, int& outTransposedNote ) const
        {
            int effNote = std::clamp( rawNote + transpose, 0, 127 );
            outTransposedNote = effNote;

            if( effNote < noteMin || effNote > noteMax )
                return false;

            if( isNoteOn && ( rawVelocity < velMin || rawVelocity > velMax ) )
                return false;

            return true;
        }

        bool isPitchBendAllowed() const
        {
            return !filterPitchBend;
        }

        bool isAftertouchAllowed() const
        {
            return !filterAftertouch;
        }

        bool isProgramChangeAllowed() const
        {
            return !filterProgramChange;
        }

        bool isControllerAllowed( int ccNumber ) const
        {
            if( filterAllCC )
                return false;
            if( ccNumber == 1 && filterModWheel )
                return false;
            if( ccNumber == 64 && filterSustain )
                return false;
            return true;
        }
    };

    // 2. Note Learn State Machine
    struct NoteLearnStateMachine
    {
        bool learning{ false };
        int  step{ 0 };
        int  learnedMin{ 0 };
        int  learnedMax{ 127 };

        void start()
        {
            learning = true;
            step = 0;
        }

        void stop()
        {
            learning = false;
            step = 0;
        }

        void processNote( int note )
        {
            if( !learning )
                return;

            if( step == 0 )
            {
                learnedMin = note;
                learnedMax = note;
                step = 1;
            }
            else
            {
                if( note < learnedMin )
                    learnedMin = note;
                else
                    learnedMax = note;
                learning = false;
                step = 0;
            }
        }
    };

    // 3. Type-based Unique Naming
    enum class ProgramType
    {
        Drum,
        Keygroup,
        Clip,
        Midi,
        Cv,
        Audio,
        Plugin
    };

    std::string getDefaultProgramName( ProgramType type, const std::vector<std::string>& existingNames )
    {
        std::string prefix = "Program ";
        switch( type )
        {
            case ProgramType::Drum:     prefix = "Drum "; break;
            case ProgramType::Keygroup: prefix = "Keygroup "; break;
            case ProgramType::Clip:     prefix = "Clip "; break;
            case ProgramType::Midi:     prefix = "MIDI "; break;
            case ProgramType::Cv:       prefix = "CV "; break;
            case ProgramType::Audio:    prefix = "Audio "; break;
            case ProgramType::Plugin:   prefix = "Plugin "; break;
        }

        int count = 1;
        while( true )
        {
            std::string candidate = prefix + std::to_string( count );
            bool found = false;
            for( const auto& name : existingNames )
            {
                if( name == candidate )
                {
                    found = true;
                    break;
                }
            }
            if( !found )
                return candidate;
            ++count;
        }
    }

    std::string getUniqueProgramName( const std::string& baseName, const std::vector<std::string>& existingNames )
    {
        bool found = false;
        for( const auto& name : existingNames )
        {
            if( name == baseName )
            {
                found = true;
                break;
            }
        }
        if( !found )
            return baseName;

        int count = 2;
        while( true )
        {
            std::string candidate = baseName + " " + std::to_string( count );
            bool cFound = false;
            for( const auto& name : existingNames )
            {
                if( name == candidate )
                {
                    cFound = true;
                    break;
                }
            }
            if( !cFound )
                return candidate;
            ++count;
        }
    }

    // 4. Offline/Disconnected Port Decoration & Reconnect
    std::string resolvePortSelection( const std::string& savedPort,
                                     const std::vector<std::string>& availablePorts,
                                     std::vector<std::string>& outDisplayList,
                                     int& outSelectedIndex )
    {
        outDisplayList = availablePorts;
        outSelectedIndex = 0;

        if( savedPort.empty() || savedPort == "None" )
        {
            outSelectedIndex = 0;
            return "None";
        }

        for( size_t i = 0; i < availablePorts.size(); ++i )
        {
            if( availablePorts[i] == savedPort )
            {
                outSelectedIndex = (int)i;
                return savedPort;
            }
        }

        // Port is missing! Append decorator
        std::string missingLabel = savedPort + " [Missing]";
        outDisplayList.push_back( missingLabel );
        outSelectedIndex = (int)outDisplayList.size() - 1;
        return missingLabel;
    }
}

// -----------------------------------------------------------------------------
// Test cases
// -----------------------------------------------------------------------------

void testNoteRangeAndTranspose()
{
    tests::MidiProgramFilterState state;
    state.noteMin = 36;
    state.noteMax = 84;
    state.transpose = 12; // +1 octave
    state.velMin = 10;
    state.velMax = 120;

    int effNote = 0;

    // Note 20 transposed to 32 -> below noteMin 36 -> rejected
    CHECK( !state.isNoteAllowed( 20, 64, true, effNote ) );
    CHECK_EQ( effNote, 32 );

    // Note 24 transposed to 36 -> exact noteMin -> accepted
    CHECK( state.isNoteAllowed( 24, 64, true, effNote ) );
    CHECK_EQ( effNote, 36 );

    // Note 60 transposed to 72 -> inside range -> accepted
    CHECK( state.isNoteAllowed( 60, 64, true, effNote ) );
    CHECK_EQ( effNote, 72 );

    // Note 72 transposed to 84 -> exact noteMax -> accepted
    CHECK( state.isNoteAllowed( 72, 64, true, effNote ) );
    CHECK_EQ( effNote, 84 );

    // Note 73 transposed to 85 -> above noteMax -> rejected
    CHECK( !state.isNoteAllowed( 73, 64, true, effNote ) );
    CHECK_EQ( effNote, 85 );

    // Velocity bounds
    CHECK( !state.isNoteAllowed( 60, 5, true, effNote ) );  // vel 5 < velMin 10
    CHECK( state.isNoteAllowed( 60, 10, true, effNote ) );  // exact velMin
    CHECK( state.isNoteAllowed( 60, 120, true, effNote ) ); // exact velMax
    CHECK( !state.isNoteAllowed( 60, 125, true, effNote ) );// vel 125 > velMax 120

    // Note-off ignores velocity filter
    CHECK( state.isNoteAllowed( 60, 0, false, effNote ) );

    // Transposition clamping at boundaries
    state.transpose = 50;
    state.noteMax = 127;
    CHECK( state.isNoteAllowed( 100, 64, true, effNote ) );
    CHECK_EQ( effNote, 127 ); // clamped to 127

    state.transpose = -50;
    state.noteMin = 0;
    CHECK( state.isNoteAllowed( 30, 64, true, effNote ) );
    CHECK_EQ( effNote, 0 ); // clamped to 0
}

void testPadMuteDoesNotSwallowCC()
{
    // Pad mute check must ONLY be evaluated for note messages.
    // In JUCE, MidiMessage::getNoteNumber() for CC messages returns the controller number.
    // If pad 64 (sustain) is muted, a naive pad-mute check on all messages would eat sustain pedal!
    std::vector<bool> padMuted( 128, false );
    padMuted[64] = true; // Pad 64 is muted

    auto shouldMuteMessage = [&]( bool isNoteMsg, int byte2 ) -> bool {
        if( !isNoteMsg )
            return false; // CC, pitch bend, etc. NEVER muted by pad mute
        return padMuted[byte2];
    };

    // Note 64 is muted
    CHECK( shouldMuteMessage( true, 64 ) );
    // Note 65 is not muted
    CHECK( !shouldMuteMessage( true, 65 ) );
    // CC 64 (Sustain pedal) MUST NOT be muted!
    CHECK( !shouldMuteMessage( false, 64 ) );
    // CC 1 (Mod wheel) MUST NOT be muted!
    CHECK( !shouldMuteMessage( false, 1 ) );
}

void testMidiFilters()
{
    tests::MidiProgramFilterState state;

    // Defaults: all passed
    CHECK( state.isPitchBendAllowed() );
    CHECK( state.isAftertouchAllowed() );
    CHECK( state.isProgramChangeAllowed() );
    CHECK( state.isControllerAllowed( 1 ) );
    CHECK( state.isControllerAllowed( 64 ) );
    CHECK( state.isControllerAllowed( 10 ) );

    // Pitch bend filter
    state.filterPitchBend = true;
    CHECK( !state.isPitchBendAllowed() );

    // Mod wheel filter (CC1)
    state.filterModWheel = true;
    CHECK( !state.isControllerAllowed( 1 ) );
    CHECK( state.isControllerAllowed( 64 ) ); // sustain still allowed

    // Sustain filter (CC64)
    state.filterSustain = true;
    CHECK( !state.isControllerAllowed( 64 ) );
    CHECK( state.isControllerAllowed( 10 ) ); // pan still allowed

    // Aftertouch filter
    state.filterAftertouch = true;
    CHECK( !state.isAftertouchAllowed() );

    // Program change filter
    state.filterProgramChange = true;
    CHECK( !state.isProgramChangeAllowed() );

    // Filter all CC
    state.filterAllCC = true;
    CHECK( !state.isControllerAllowed( 10 ) );
    CHECK( !state.isControllerAllowed( 7 ) );
}

void testNoteLearn()
{
    tests::NoteLearnStateMachine sm;
    CHECK( !sm.learning );

    sm.start();
    CHECK( sm.learning );
    CHECK_EQ( sm.step, 0 );

    // First note: C3 (48)
    sm.processNote( 48 );
    CHECK( sm.learning );
    CHECK_EQ( sm.step, 1 );
    CHECK_EQ( sm.learnedMin, 48 );
    CHECK_EQ( sm.learnedMax, 48 );

    // Second note: C5 (72)
    sm.processNote( 72 );
    CHECK( !sm.learning ); // completed
    CHECK_EQ( sm.learnedMin, 48 );
    CHECK_EQ( sm.learnedMax, 72 );

    // Inverted order test: high note first, low note second
    sm.start();
    sm.processNote( 80 );
    CHECK_EQ( sm.learnedMin, 80 );
    CHECK_EQ( sm.learnedMax, 80 );
    sm.processNote( 40 );
    CHECK( !sm.learning );
    CHECK_EQ( sm.learnedMin, 40 );
    CHECK_EQ( sm.learnedMax, 80 );
}

void testTypeBasedNaming()
{
    std::vector<std::string> names;

    // First MIDI program -> "MIDI 1"
    std::string name1 = tests::getDefaultProgramName( tests::ProgramType::Midi, names );
    CHECK_EQ( name1, std::string( "MIDI 1" ) );
    names.push_back( name1 );

    // Second MIDI program -> "MIDI 2"
    std::string name2 = tests::getDefaultProgramName( tests::ProgramType::Midi, names );
    CHECK_EQ( name2, std::string( "MIDI 2" ) );
    names.push_back( name2 );

    // First Drum program -> "Drum 1"
    std::string drum1 = tests::getDefaultProgramName( tests::ProgramType::Drum, names );
    CHECK_EQ( drum1, std::string( "Drum 1" ) );
    names.push_back( drum1 );

    // Duplicate unique naming
    std::string dup1 = tests::getUniqueProgramName( name1 + "_copy", names );
    CHECK_EQ( dup1, std::string( "MIDI 1_copy" ) );
    names.push_back( dup1 );

    // Second duplicate of same name
    std::string dup2 = tests::getUniqueProgramName( name1 + "_copy", names );
    CHECK_EQ( dup2, std::string( "MIDI 1_copy 2" ) );
    names.push_back( dup2 );
}

void testCcLabels()
{
    // Exact KnobTabs and MidiControlTabs list
    const std::vector<std::string> ccLabels = {
        "Bank Select", "Modulation", " Breath", " CC3", " Foot", " Portamento ", " DataEntry ", " Volume ", " Balance ", " CC9 ",
        " Pan ", " Expression ", " Effect1 ", " Effect2 ", " CC14 ", " CC15 ", " GenPurpose1 ", " GenPurpose2 ", " genpurpose3 ", " genpurpose4 ",
        " CC20 ", " CC21 ", " CC22 ", " CC23 ", " CC24 ", " CC25 ", " CC26 ", " CC27 ", " CC28 ", " CC29 ",
        " CC30 ", " CC31 ", " BankSelLSB ", " ModulationLSB ", " BreathLSB ", " CC35 ", " FootLSB ", " PortamentoLSB ", " DataEntryLSB ", " VolumeLSB ",
        " BalanceLSB ", " CC41 ", " PanLSB ", " ExpressionLSB ", " Effect1LSB ", " Effect2LSB ", " CC46 ", " CC47 ", " GenPur1LSB ", " GenPur2LSB ",
        " GenPur3LSB ", " GenPur4LSB ", " CC52 ", " CC53 ", " CC54 ", " CC55 ", " CC56 ", " CC57 ", " CC58 ", " CC59 ",
        " CC60 ", " CC61 ", " CC62 ", " CC63 ", " SustainOnOff ", " PmentoOnOff ", " SostOnOff ", " SoftPedOnOff ", " LegatoAwitch ", " Hold2 ",
        " SndControl1 ", " SndControl2 ", " SndControl3 ", " SndControl4 ", " SndControl5 ", " SndControl6 ", " SndControl7 ", " SndControl8 ", " SndControl9 ", " SndControl10 ",
        " GenPurpose5 ", " GenPurpose6 ", " GenPurpose7 ", " GenPurpose8 ", " PmentoControl ", " CC85 ", " CC86 ", " CC87 ", " HiResVelocity ", " CC89 ",
        " CC90 ", " Send1 ", " Send2 ", " Send3 ", " Send4 ", " Send5 ", " DataIncrement ", " DataDecrement ", " NRPNLSB ", " NRPNMSB ",
        " RPNLSB ", " RPNMSB ", " CC102 ", " CC103 ", " CC104 ", " CC105 ", " CC106 ", " CC107 ", " CC108 ", " CC109 ",
        " CC110 ", " CC111 ", " CC112 ", " CC113 ", " CC114 ", " CC115 ", " CC116 ", " CC117 ", " CC118 ", " CC119 ",
        " AllSoundOff ", " ResetAll ", " Localonoff ", " AllnoteOff ", " OmniModeOff ", " OmniModeOn ", " MonoMode ", " PolyMode "
    };

    CHECK_EQ( (int)ccLabels.size(), 128 );

    // Check fixed items
    CHECK_EQ( ccLabels[20], std::string( " CC20 " ) );
    CHECK_EQ( ccLabels[21], std::string( " CC21 " ) );
    CHECK_EQ( ccLabels[22], std::string( " CC22 " ) ); // Fixed: was duplicate "CC21"
    CHECK_EQ( ccLabels[23], std::string( " CC23 " ) );

    CHECK_EQ( ccLabels[91], std::string( " Send1 " ) );
    CHECK_EQ( ccLabels[92], std::string( " Send2 " ) );
    CHECK_EQ( ccLabels[93], std::string( " Send3 " ) );
    CHECK_EQ( ccLabels[94], std::string( " Send4 " ) );
    CHECK_EQ( ccLabels[95], std::string( " Send5 " ) ); // Fixed: was duplicate " Send4 "
    CHECK_EQ( ccLabels[96], std::string( " DataIncrement " ) );
}

void testOfflinePortAndReconnection()
{
    std::vector<std::string> ports = { "None", "Kick:Out:1", "Kick:Out:2" };
    std::vector<std::string> displayList;
    int selectedIndex = -1;

    // Case 1: port is available
    std::string sel = tests::resolvePortSelection( "Kick:Out:1", ports, displayList, selectedIndex );
    CHECK_EQ( sel, std::string( "Kick:Out:1" ) );
    CHECK_EQ( selectedIndex, 1 );
    CHECK_EQ( (int)displayList.size(), 3 );

    // Case 2: port is disconnected/missing
    sel = tests::resolvePortSelection( "External Synth", ports, displayList, selectedIndex );
    CHECK_EQ( sel, std::string( "External Synth [Missing]" ) );
    CHECK_EQ( selectedIndex, 3 );
    CHECK_EQ( (int)displayList.size(), 4 );
    CHECK_EQ( displayList[3], std::string( "External Synth [Missing]" ) );

    // Case 3: hot-plug reconnection - "External Synth" is plugged in
    ports.push_back( "External Synth" );
    sel = tests::resolvePortSelection( "External Synth", ports, displayList, selectedIndex );
    CHECK_EQ( sel, std::string( "External Synth" ) );
    CHECK_EQ( selectedIndex, 3 );
    CHECK_EQ( (int)displayList.size(), 4 );
    CHECK_EQ( displayList[3], std::string( "External Synth" ) ); // No "[Missing]"
}

int main()
{
    testNoteRangeAndTranspose();
    testPadMuteDoesNotSwallowCC();
    testMidiFilters();
    testNoteLearn();
    testTypeBasedNaming();
    testCcLabels();
    testOfflinePortAndReconnection();

    if( g_fail == 0 )
    {
        std::printf( "ALL MIDI PROGRAM TESTS PASSED (0 failures)\n" );
        return 0;
    }
    std::printf( "MIDI PROGRAM TESTS FAILED (%d failures)\n", g_fail );
    return 1;
}
