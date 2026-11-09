/*
 * Copyright (c) 2024 MUSIC Tribe
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

#include "../../hardware/source/kick_PlatformServices.h"
#include "../../hardware/source/kick_I2c.h"
#include "kick_recent_list.h"

namespace mt
{
    KickSettings& getSettings()
    {
        static auto* settingsInstance = new KickSettings;
        return *settingsInstance;
    }

    /// ///////////////////////////////////
    ///    KickSettings
    /// ///////////////////////////////////
    KickSettings::KickSettings()
        : KickXML( IDs::PREFERENCES, __settingsFileSuffix, __settingsFileVersion )
    {
        // Disable auto-save during constructor activity.
        setAutoSave( false );
        setAutoSaveTime( 1500 );

        // Load settings file...
        onLoad = [this]() {
            // Setup sub-trees.
            projects.setup( getState().getOrCreateChildWithName( IDs::PROJECTDEFAULTS, nullptr ), nullptr );
            midiControl.setup( getState().getOrCreateChildWithName( IDs::PREFMIDICONTROL, nullptr ), nullptr );
            eth.setup( getState().getOrCreateChildWithName( IDs::ETHERNET, nullptr ), nullptr );
            cv.setup( getState().getOrCreateChildWithName( IDs::PREFCV, nullptr ), nullptr );
            hardware.setup( getState().getOrCreateChildWithName( IDs::PREFHARDWARE, nullptr ), nullptr );
            browser.setup( getState().getOrCreateChildWithName( IDs::BROWSERFAVOURITE, nullptr ), nullptr );
            cvclib.setup( getState().getOrCreateChildWithName( IDs::PREFCVCLIB, nullptr ), nullptr );
            metronome.setup( getState().getOrCreateChildWithName( IDs::METRONOME, nullptr ), nullptr );
            wifi.setup( getState().getOrCreateChildWithName( IDs::WIFI, nullptr ), nullptr );
            cloudSources.setup( getState().getOrCreateChildWithName( IDs::CloudSources, nullptr ), nullptr );
            padColours.setup( getState().getOrCreateChildWithName( IDs::PADCOLOURDEFAULTS, nullptr ), nullptr );
            knobControl.setup( getState().getOrCreateChildWithName( IDs::KNOBCONTROL, nullptr ), nullptr );
            timingCorrect.setup( getState().getOrCreateChildWithName( IDs::TIMINGCORRECT, nullptr ), nullptr );
        };

        auto settingsFile = getAppDirs( MTPrefs ).getChildFile( __settingsFileName );
        loadOrCreate( settingsFile );

        // Apply CV calibration and start notes at boot from loaded preferences
        std::array<uint16_t, 6> calib = { 0 };
        for( int i = 0; i < 4; ++i )
            calib[i + 1] = static_cast<uint16_t>( cvclib.getOffset( i ) );
        calib[5] = 4095;
        CV::getCVInterface().applyCalibration( calib );

        for( int i = 0; i < CV::MaxCVPorts; ++i )
            CV::getCVInterface().setChannelNoteMap( i, cv.getNoteStart( i ) );

        // Apply hardware settings to platform services
        auto& ps = PlatformServices::getInstance();
        kick_log( "info", "[INFO] [BACKLIGHT] %d%% from the preferences at start-up\n", (int)hardware.brightness() );
        ps.setBrightness( hardware.brightness() );

        juce::String currentTz = hardware.timezone();
        bool userExplicit = hardware.timeZoneUserSet();
        if( !userExplicit || currentTz.isEmpty() || currentTz == "Asia/Kolkata" )
        {
            juce::String detected = ps.detectSystemTimezone();
            if( detected.isNotEmpty() )
            {
                hardware.timezone = detected;
                currentTz = detected;
            }
        }
        ps.setTimeZone( currentTz );

        // If not explicitly set by user, asynchronously check network/GeoIP
        if( !userExplicit )
        {
            ps.detectTimezoneAsync( [this]( const juce::String& netTz ) {
                if( !hardware.timeZoneUserSet() && netTz.isNotEmpty() )
                {
                    hardware.timezone = netTz;
                    PlatformServices::getInstance().setTimeZone( netTz );
                }
            } );
        }

        // Enable auto-save for settings file.
        setAutoSave( true );
    }

    void KickSettings::makeDefaults()
    {
        projects.makeDefaults();
        hardware.makeDefaults();
        eth.makeDefaults();
        cv.makeDefaults();
        midiControl.makeDefaults();
        wifi.makeDefaults();
        cloudSources.makeDefaults();
        browser.makeDefaults();
        cvclib.makeDefaults();
        metronome.makeDefaults();
        padColours.makeDefaults();
        knobControl.makeDefaults();
        timingCorrect.makeDefaults();

        // Ensure autoSave stays true on defaults
        projects.isAutoSave = true;

        // Re-apply defaults to running hardware
        auto& ps = PlatformServices::getInstance();
        kick_log( "info", "[INFO] [BACKLIGHT] %d%% from the default preferences (reset)\n", (int)hardware.brightness() );
        ps.setBrightness( hardware.brightness() );
        juce::String tz = hardware.timezone();
        ps.setTimeZone( tz );

        std::array<uint16_t, 6> calib = { 0, 0, 0, 0, 0, 4095 };
        CV::getCVInterface().applyCalibration( calib );

        for( int i = 0; i < CV::MaxCVPorts; ++i )
            CV::getCVInterface().setChannelNoteMap( i, 0 );
    }

    /// ///////////////////////////////////
    ///    Pad colour defaults.
    /// ///////////////////////////////////
    void         KickPadColourDefaults::setup( juce::ValueTree VT, juce::UndoManager* Undo ) { setupBase( VT, Undo ); }
    bool         KickPadColourDefaults::isValid() { return vTree.hasType( IDs::PADCOLOURDEFAULTS ); }
    juce::String KickPadColourDefaults::get( const juce::String& Key ) { return vTree.isValid() && vTree.hasProperty( Key ) ? vTree.getProperty( Key ).toString() : juce::String(); }
    void         KickPadColourDefaults::set( const juce::String& Key, const juce::String& Value ) { vTree.setProperty( Key, Value, nullptr ); }

    /// ///////////////////////////////////
    ///    Browser Favourite.
    /// ///////////////////////////////////
    void KickBrowserFavourite::setup( juce::ValueTree VT, juce::UndoManager* Undo ) { setupBase( VT, Undo ); }
    bool KickBrowserFavourite::isValid() { return vTree.hasType( IDs::BROWSERFAVOURITE ); }

    // Getters...
    juce::String KickBrowserFavourite::getPathAssignedToButton( juce::String buttonName ) { return vTree.getPropertyAsValue( buttonName, nullptr ).toString(); }
    juce::var    KickBrowserFavourite::getSelectedLeftRow( juce::String buttonName ) { return vTree.getPropertyAsValue( buttonName, nullptr ); }
    int          KickBrowserFavourite::getNumProperties() { return vTree.getNumProperties(); }

    // Setters...
    juce::var KickBrowserFavourite::getOption( const juce::String& name ) { return vTree.hasProperty( name ) ? vTree.getProperty( name ) : juce::var(); }
    void      KickBrowserFavourite::setOption( const juce::String& name, const juce::var& value ) { vTree.setProperty( name, value, nullptr ); }
    void KickBrowserFavourite::setPathToButton( juce::String buttonName, juce::String path ) { vTree.setProperty( buttonName, path, nullptr ); }
    void KickBrowserFavourite::setSelectedLeftRow( juce::String buttonName, int row ) { vTree.setProperty( buttonName, row, nullptr ); }

    /// ///////////////////////////////////
    ///    Defaults Projects.
    /// ///////////////////////////////////
    void KickProjectsDefault::setup( juce::ValueTree VT, juce::UndoManager* Undo )
    {
        setupBase( VT, Undo );

        isAutoLoad.attachTo( vTree, IDs::Autoload, Undo, false );
        isAutoSave.attachTo( vTree, IDs::Autosave, Undo, true );

        tempo.attachTo( vTree, IDs::TimeTempo, Undo, BPM::DEFAULT );
        bars.attachTo( vTree, IDs::TimeBars, Undo, BAR::DEFAULT );
        rootNote.attachTo( vTree, IDs::rootNote, Undo, 0 );
        scale.attachTo( vTree, IDs::scale, Undo, 0 );
        numSeq.attachTo( vTree, IDs::ProjectSequance, Undo, 1 );
        numTrackPerSeq.attachTo( vTree, IDs::ProjectTrackSeq, Undo, 1 );
        maxRecentProjects.attachTo( vTree, IDs::MaxNumOfRecentProject, Undo, 5 );
        defProjectState.attachTo( vTree, IDs::DefaultProjectState, Undo, 5 );

        onLaunch.attachTo( vTree, IDs::Onlaunch, Undo, "Selector" );
        launchTemplate.attachTo( vTree, IDs::LaunchTemplate, Undo, "" );
        bitDepth.attachTo( vTree, IDs::recordBitDeapth, Undo, "16" );
        saveAfter.attachTo( vTree, IDs::SaveAfter, Undo, "30 Sec" );
        numerator.attachTo( vTree, IDs::TimeNumerator, Undo, "4" );
        denominator.attachTo( vTree, IDs::TimeDenominator, Undo, "4" );

        namingStyle.attachTo( vTree, IDs::DefaultProjectStateName, Undo, "DISABLE" );
        namePrefix.attachTo( vTree, IDs::DefaultProjectName, Undo, "" );

        currentProject.attachTo( vTree, IDs::CurrentProject, Undo, "" );
    }
    bool KickProjectsDefault::isValid() { return vTree.hasType( IDs::PROJECTDEFAULTS ); }

    // Getters...

    int          KickProjectsDefault::getNumChildren() { return vTree.getNumChildren(); }
    juce::String KickProjectsDefault::getProject( int Index )
    {
        auto idx = ( 0 > Index ) ? vTree.getNumChildren() - 1 : juce::jlimit( 0, ( vTree.getNumChildren() - 1 ), Index );
        return vTree.getChild( idx ).getProperty( IDs::FULLPATH, "" );
    }

    // Setters...
    void KickProjectsDefault::setProject( int Index, const juce::String& Path ) { vTree.getChild( Index ).setProperty( IDs::FULLPATH, Path, undo ); }
    void KickProjectsDefault::addProject( const juce::String& Path )
    {
        juce::ValueTree newEntry( "Entry" );
        newEntry.setProperty( IDs::FULLPATH, Path, undo );
        vTree.appendChild( newEntry, undo );
    }

    void KickProjectsDefault::removeProject( int Index ) { vTree.removeChild( Index, undo ); }

    // The stored list as plain strings (oldest first), for the pure list logic (kick_recent_list.h).
    static std::vector<std::string> recentList( juce::ValueTree& tree )
    {
        std::vector<std::string> out;
        for( int i = 0; i < tree.getNumChildren(); ++i )
            out.push_back( tree.getChild( i ).getProperty( IDs::FULLPATH, "" ).toString().toStdString() );
        return out;
    }

    void KickProjectsDefault::removeProject( juce::File& ProjectFile )
    {
        juce::File project = ProjectFile.exists() ? ProjectFile.isDirectory() ? ProjectFile : ProjectFile.getParentDirectory() : ProjectFile;

        // Highest index first: removing an entry must not shift the ones still to be removed.
        for( int i : kick_recent::indicesOf( recentList( vTree ), project.getFullPathName().toStdString() ) )
            removeProject( i );
    }

    void KickProjectsDefault::renameProject( juce::File& ProjectFile, juce::File& newFile )
    {
        juce::File project = ProjectFile.exists() ? ProjectFile.isDirectory() ? ProjectFile : ProjectFile.getParentDirectory() : ProjectFile;
        const auto list    = recentList( vTree );
        const auto after   = kick_recent::renamed( list, project.getFullPathName().toStdString(), newFile.getFullPathName().toStdString() );
        if( after == list )
            return;
        if( after.size() == list.size() )
        {
            for( size_t i = 0; i < after.size(); ++i )
                if( after[i] != list[i] )
                    setProject( (int)i, juce::String( after[i] ) );
            return;
        }
        // The new path was already in the list: one project, one entry.
        while( vTree.getNumChildren() > 0 )
            removeProject( vTree.getNumChildren() - 1 );
        for( const auto& e : after )
            addProject( juce::String( e ) );
    }

    void KickProjectsDefault::createProject( const juce::File& file )
    {
        juce::File project = file.getChildFile( "Samples" );
        project.createDirectory();
        project = file.getChildFile( "Export" );
        project.createDirectory();
    }

    void KickProjectsDefault::rearrangeProject( juce::File& ProjectFile )
    {
        bool isLoad = checkAlreadyLoadOrNot( ProjectFile );
        if( isLoad )
        {
            addProject( ProjectFile.getFullPathName() );
            currentProject = ( ProjectFile.getFullPathName() );
        }
    }

    juce::File KickProjectsDefault::getProjectPath( juce::File& ProjectFile )
    {
        if( ProjectFile.exists() )
        {
            juce::File dir     = ProjectFile.exists() ? ProjectFile.isDirectory() ? ProjectFile : ProjectFile.getParentDirectory() : ProjectFile;
            auto       n       = dir.getFileName() + __projectFileSuffix;
            juce::File project = dir.getChildFile( n );
            return project;
        }
        return juce::File();
    }

    bool KickProjectsDefault::checkAlreadyLoadOrNot( juce::File& ProjectFile )
    {
        // The project is opened again: its old entry goes, the oldest ones make room (the list never exceeds its maximum), the
        // caller adds it as the newest. A project without a project file is not added.
        juce::File project = getProjectPath( ProjectFile );
        const auto plan    = kick_recent::planOpen( recentList( vTree ), ProjectFile.getFullPathName().toStdString(), project.exists(), maxRecentProjects() );
        for( int i : plan.remove )
            removeProject( i );
        return plan.add ? ADDPROJECT : DONT_ADDPROJECT;
    }

    void KickProjectsDefault::refreshMaxNumOfRecentProj()
    {
        for( int i = kick_recent::excess( (size_t)getNumChildren(), maxRecentProjects() ); i > 0; --i )
            removeProject( 0 );
    }

    /// ///////////////////////////////////
    ///    Hardware
    /// ///////////////////////////////////
    void KickHardware::setup( juce::ValueTree VT, juce::UndoManager* Undo )
    {
        setupBase( VT, Undo );

        tempoLEDStatus.attachTo( vTree, IDs::tapTempoLed, Undo, false );
        velocity.attachTo( vTree, IDs::padVelocity, Undo, MIDI::MAX );
        aftertouch.attachTo( vTree, IDs::afterTouch, Undo, MIDI::MAX );
        brightness.attachTo( vTree, IDs::screenBrightness, Undo, MIDI::LEVEL_DEFAULT );
        screenDimMinutes.attachTo( vTree, IDs::screenDimMinutes, Undo, 5 );
        screenDimLevel.attachTo( vTree, IDs::screenDimLevel, Undo, 20 );
        threshold.attachTo( vTree, IDs::padThreshold, Undo, "4" );
        sensitivity.attachTo( vTree, IDs::padSensitivity, Undo, "4" );
        curve.attachTo( vTree, IDs::padCurve, Undo, "4" );
        numTaps.attachTo( vTree, IDs::noOftaps, Undo, "2" );
        timezone.attachTo( vTree, IDs::timeZone, Undo, "Europe/Budapest" );
        timeZoneUserSet.attachTo( vTree, IDs::timeZoneUserSet, Undo, false );
    }
    bool KickHardware::isValid() { return vTree.hasType( IDs::PREFHARDWARE ); }

    /// ///////////////////////////////////
    ///    MIDI Control .
    /// ///////////////////////////////////
    void KickMidiControl::setup( juce::ValueTree VT, juce::UndoManager* Undo )
    {
        setupBase( VT, Undo );

        ipTrackState.attachTo( vTree, IDs::midiInputTrackState, Undo, false );
        opTrackState.attachTo( vTree, IDs::midiOutputTrackState, Undo, false );

        ipControlState.attachTo( vTree, IDs::midiInputControlState, Undo, false );
        opControlState.attachTo( vTree, IDs::midiOutputControlState, Undo, false );

        ipPortName.attachTo( vTree, IDs::MidiInputName, Undo, "" );
        opPortName.attachTo( vTree, IDs::MidiOutputName, Undo, "" );
        midiControlModeOutputPort.attachTo( vTree, IDs::MidiControlModeOutputPort, Undo, "" );
    }
    bool KickMidiControl::isValid() { return vTree.hasType( IDs::PREFMIDICONTROL ) || vTree.hasType( IDs::MIDICONTROL ); }

    /// ///////////////////////////////////
    ///    KNOB Control (QL-13)
    /// ///////////////////////////////////
    void KickKnobControl::setup( juce::ValueTree VT, juce::UndoManager* Undo )
    {
        setupBase( VT, Undo );

        defaultQLinkMode.attachTo( vTree, IDs::defaultQLinkMode, Undo, 1 ); // 1 = Screen mode default
        qlinkPlayheadIncrement.attachTo( vTree, IDs::qlinkPlayheadIncrement, Undo, 1 ); // 1 bar
        swingControlAppliesTcOnRelease.attachTo( vTree, IDs::swingControlAppliesTcOnRelease, Undo, false );
        showQLinkStatusWhenTouched.attachTo( vTree, IDs::showQLinkStatusWhenTouched, Undo, true );
        automationLaneFollowsQLinks.attachTo( vTree, IDs::automationLaneFollowsQLinks, Undo, false );
    }
    bool KickKnobControl::isValid() { return vTree.hasType( IDs::KNOBCONTROL ); }

    /// ///////////////////////////////////
    ///    Timing Correct (HDR-24)
    /// ///////////////////////////////////
    void KickTimingCorrect::setup( juce::ValueTree VT, juce::UndoManager* Undo )
    {
        setupBase( VT, Undo );

        enabled.attachTo( vTree, IDs::tcEnabled, Undo, false );
        type.attachTo( vTree, IDs::tcType, Undo, 0 );
        events.attachTo( vTree, IDs::tcEvents, Undo, 0 );
        gridTicks.attachTo( vTree, IDs::tcGridTicks, Undo, 24 );
        swingPct.attachTo( vTree, IDs::tcSwingPct, Undo, 50 );
        shiftTicks.attachTo( vTree, IDs::tcShiftTicks, Undo, 0 );
        windowPct.attachTo( vTree, IDs::tcWindowPct, Undo, 100 );
        strengthPct.attachTo( vTree, IDs::tcStrengthPct, Undo, 100 );
        startBar.attachTo( vTree, IDs::tcStartBar, Undo, 1 );
        endBar.attachTo( vTree, IDs::tcEndBar, Undo, 2 );
    }
    bool KickTimingCorrect::isValid() { return vTree.hasType( IDs::TIMINGCORRECT ); }

    /// ///////////////////////////////////
    ///    Ethernet .
    /// ///////////////////////////////////
    void KickEthernet::setup( juce::ValueTree VT, juce::UndoManager* Undo )
    {
        setupBase( VT, Undo );

        enabled.attachTo( vTree, IDs::status, Undo, true );
        mode.attachTo( vTree, IDs::mode, Undo, "DHCP" );
        ipAddress.attachTo( vTree, IDs::ipAdd, Undo, "0.0.0.0" );
        subnetMask.attachTo( vTree, IDs::subnetMask, Undo, "255.255.255.0" );
        gateway.attachTo( vTree, IDs::gateway, Undo, "0.0.0.0" );
        dns.attachTo( vTree, IDs::dns, Undo, "0.0.0.0" );
    }
    bool KickEthernet::isValid() { return vTree.hasType( IDs::ETHERNET ); }

    /// ///////////////////////////////////
    ///    CV .
    /// ///////////////////////////////////
    void KickCV::setup( juce::ValueTree VT, juce::UndoManager* Undo ) { setupBase( VT, Undo ); }
    bool KickCV::isValid() { return vTree.hasType( IDs::PREFCV ); }
    // Getters...
    int KickCV::getNoteStart( int CVPort )
    {
        if( vTree.isValid() )
        {
            juce::Identifier id( "CV" + juce::String( CVPort ) );
            return vTree.getProperty( id, 0 );
        }
        return 0;
    }
    // Setters...
    void KickCV::setNoteStart( int CVPort, int Note )
    {
        if( vTree.isValid() )
        {
            juce::Identifier id( "CV" + juce::String( CVPort ) );
            vTree.setProperty( id, Note, undo );
        }
    }

    /// ///////////////////////////////////
    ///    KickCVClib .
    /// ///////////////////////////////////
    void KickCVClib::setup( juce::ValueTree VT, juce::UndoManager* Undo ) { setupBase( VT, Undo ); }
    bool KickCVClib::isValid() { return vTree.hasType( IDs::PREFCV ); }
    // Getters...
    int KickCVClib::getOffset( int index )
    {
        if( vTree.isValid() )
        {
            juce::Identifier id( "CLIB" + juce::String( index ) );
            return vTree.getProperty( id, 0 );
        }
        return 0;
    }
    // Setters...
    void KickCVClib::setOffset( int index, int offset )
    {
        if( vTree.isValid() )
        {
            juce::Identifier id( "CLIB" + juce::String( index ) );
            vTree.setProperty( id, offset, undo );
        }
    }

    /// ///////////////////////////////////
    ///    KickMetronome .
    /// ///////////////////////////////////
    static const juce::String MStyle0{ "met-1-bar.wav" };
    static const juce::String MStyle1{ "Perc_Glass_hi.wav" };
    static const juce::String MStyle2{ "Perc_Clap_hi.wav" };
    static const juce::String MStyle3{ "Perc_Metal_hi.wav" };
    static const juce::String MStyle4{ "Perc_MouthPop_hi.wav" };
    static const juce::String MStyle5{ "Perc_MetronomeQuartz_hi.wav" };
    static const juce::String MStyle6{ "Perc_Stick_hi.wav" };
    static const juce::String MStyle7{ "Synth_Bell_A_hi.wav" };

    KickMetronome::KickMetronome()
    {
        style.onValueChanged = [&]() {
            juce::String styleSound;
            switch( style.load() )
            {
                default:
                case MTMetronomeStyle::Style1: styleSound = MStyle1; break;
                case MTMetronomeStyle::Style2: styleSound = MStyle2; break;
                case MTMetronomeStyle::Style3: styleSound = MStyle3; break;
                case MTMetronomeStyle::Style4: styleSound = MStyle4; break;
                case MTMetronomeStyle::Style5: styleSound = MStyle5; break;
                case MTMetronomeStyle::Style6: styleSound = MStyle6; break;
                case MTMetronomeStyle::Style7: styleSound = MStyle7; break;
            }
            auto file = getAppDirs( MTAssets ).getChildFile( styleSound ).getFullPathName();
            source    = file;
        };
    }

    void KickMetronome::setup( juce::ValueTree VT, juce::UndoManager* Undo )
    {
        setupBase( VT, Undo );

        enabled.attachTo( vTree, IDs::enable, Undo, false );
        gain.attachTo( vTree, IDs::gain, Undo, GAIN::ROOT_2 );
        pan.attachTo( vTree, IDs::pan, Undo, PAN::DEFAULT );
        rate.attachTo( vTree, IDs::rate, Undo, 2 );
        style.attachTo( vTree, IDs::type, Undo, 0 );
        mode.attachTo( vTree, IDs::mode, Undo, 1 );
        source.attachTo( vTree, IDs::source, Undo, MStyle0 );
    }

    bool KickMetronome::isValid() { return vTree.hasType( IDs::METRONOME ); }

    /// ///////////////////////////////////
    ///    KickWifi .
    /// ///////////////////////////////////
    void KickWifi::setup( juce::ValueTree VT, juce::UndoManager* Undo )
    {
        setupBase( VT, Undo );

        enabled.attachTo( vTree, IDs::wifienabled, Undo, false );
        ssid.attachTo( vTree, IDs::wifissid, Undo, "" );
        pwd.attachTo( vTree, IDs::wifipasswd, Undo, "" );
    }

    bool KickWifi::isValid() { return vTree.hasType( IDs::WIFI ); }

    /// ///////////////////////////////////
    ///    KickCloudSources .
    /// ///////////////////////////////////
    void KickCloudSources::setup( juce::ValueTree VT, juce::UndoManager* Undo )
    {
        setupBase( VT, Undo );

        enableSplice.attachTo( vTree, IDs::enableSplice, Undo, true );
        enableGoogleDrive.attachTo( vTree, IDs::enableGoogleDrive, Undo, true );
        enableWebDav.attachTo( vTree, IDs::enableWebDav, Undo, true );
        splicePath.attachTo( vTree, IDs::splicePath, Undo, "" );
        googleDrivePath.attachTo( vTree, IDs::googleDrivePath, Undo, "" );
    }

    void KickCloudSources::makeDefaults()
    {
        enableSplice = true;
        enableGoogleDrive = true;
        enableWebDav = true;
        splicePath = "";
        googleDrivePath = "";
    }

    bool KickCloudSources::isValid() { return vTree.hasType( IDs::CloudSources ); }

    /// ///////////////////////////////////
    ///    KickPreview .
    /// ///////////////////////////////////
    KickPreview::KickPreview()
    {
        auto gainCalculator = [&]() { gain = !mute.get() ? CommonUtils::midiToGain( level.get() ) : 0.0; };

        level.onValueChange.connect( gainCalculator );
        mute.onValueChange.connect( gainCalculator );

        level.referTo( state, IDs::level, nullptr, MIDI::LEVEL_DEFAULT );
        mute.referTo( state, IDs::mute, nullptr, false );
        isOn.referTo( state, IDs::IsPreviewOn, nullptr, false );
        isAuto.referTo( state, IDs::IsPreviewAuto, nullptr, false );
    }

    void KickPreview::reset()
    {
        mute  = false;
        level = MIDI::MAX;
    }
}
