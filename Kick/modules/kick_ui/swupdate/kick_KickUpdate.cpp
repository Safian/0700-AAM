// SPDX-License-Identifier: AGPL-3.0-or-later
#include "kick_KickUpdate.h"

namespace kick_ui
{
    namespace
    {
        // Packaged layout written by make_pendrive_package.sh: <drive>/tkgl_bootstrap_[ProjectData]/{VERSION,bin/Kick}.
        constexpr const char* kPackageDir = "tkgl_bootstrap_[ProjectData]";

        std::string readText( const juce::File& f )
        {
            return f.existsAsFile() ? f.loadFileAsString().toStdString() : std::string();
        }

        juce::File dataDir()
        {
            return juce::File( juce::SystemStats::getEnvironmentVariable( "KICK_DATA_ROOT", "/data" ) );
        }

        // What this process was built as, for the installed line when /data/Kick.version does not exist (desktop, dev builds).
        kick_update::PackageInfo compiledInfo()
        {
#ifndef KICK_APP_VERSION
    #define KICK_APP_VERSION ""
#endif
#ifndef KICK_GIT_HASH
    #define KICK_GIT_HASH ""
#endif
            kick_update::PackageInfo i;
            i.version = KICK_APP_VERSION;
            i.commit  = std::string( KICK_GIT_HASH ).substr( 0, 12 );
            return i;
        }
    }

    /// Looks at the installed Kick and at every USB drive on a worker thread (a slow stick must not freeze the UI; the SHA-256
    /// of the package binary is read in full). Result is delivered once on the message thread.
    class KickUpdateScreen::Scanner : public juce::Thread
    {
    public:
        explicit Scanner( juce::Component::SafePointer<KickUpdateScreen> o )
            : juce::Thread( "Kick update scan" ), owner( o ) {}
        ~Scanner() override { stopThread( 5000 ); }

        void run() override
        {
            Scan scan;
            const auto data = dataDir();
            const auto installedText = readText( data.getChildFile( "Kick.version" ) );
            scan.installed = kick_update::parseVersionText( installedText );
            if( !scan.installed.valid() )
                scan.installed = compiledInfo(); // not installed from a package (dev / desktop): the running build
            const auto prevText = readText( data.getChildFile( "Kick.version.prev" ) );
            scan.hasPrevious     = data.getChildFile( "Kick.prev" ).existsAsFile();
            scan.previousVersion = kick_update::parseVersionText( prevText ).version;

            kick_update::Inputs in;
            in.installedText = installedText;

            // Drives from the mount table; the first one that carries a package wins, otherwise the first drive.
            std::vector<std::string> drives;
#if JUCE_LINUX
            {
                std::ifstream f( "/proc/mounts" );
                std::stringstream ss;
                ss << f.rdbuf();
                drives = kick_sysinfo::findVolumeMounts( kick_sysinfo::parseMounts( ss.str() ), kick_sysinfo::Volume::Usb,
                                                         []( const std::string& p ) { return juce::File( juce::String( p ) ).isDirectory(); } );
            }
#endif
            // KICK_UPDATE_DRIVE: a directory that stands in for a USB drive (desktop test / smoke run).
            const auto override = juce::SystemStats::getEnvironmentVariable( "KICK_UPDATE_DRIVE", {} );
            if( override.isNotEmpty() )
                drives.insert( drives.begin(), override.toStdString() );

            in.driveFound = !drives.empty();
            juce::File root;
            for( const auto& d : drives )
            {
                if( threadShouldExit() )
                    return;
                const auto pkg = juce::File( juce::String( d ) ).getChildFile( kPackageDir );
                if( pkg.getChildFile( "VERSION" ).existsAsFile() )
                {
                    root             = pkg;
                    scan.drive       = d;
                    in.versionFileFound = true;
                    break;
                }
            }
            if( scan.drive.empty() && !drives.empty() )
                scan.drive = drives.front();

            if( in.versionFileFound )
            {
                in.packageText  = readText( root.getChildFile( "VERSION" ) );
                scan.package    = kick_update::parseVersionText( in.packageText );
                const auto bin  = root.getChildFile( "bin" ).getChildFile( "Kick" );
                in.binaryFound  = bin.existsAsFile() && bin.getSize() > 0;
                if( in.binaryFound && !scan.package.sha256.empty() )
                {
                    juce::FileInputStream stream( bin );
                    if( stream.openedOk() )
                    {
                        juce::MemoryBlock block( 1 << 20 );
                        kick_update::Sha256 sha;
                        while( !stream.isExhausted() )
                        {
                            if( threadShouldExit() )
                                return;
                            const auto n = stream.read( block.getData(), (int)block.getSize() );
                            if( n <= 0 )
                                break;
                            sha.update( block.getData(), (size_t)n );
                        }
                        in.shaChecked = true;
                        in.shaMatches = juce::String( sha.hex() ).equalsIgnoreCase( juce::String( scan.package.sha256 ) );
                    }
                }
            }
            scan.state = kick_update::classify( in, &scan.reason );

            auto owner_ = owner;
            juce::MessageManager::callAsync( [owner_, scan] {
                if( auto* o = owner_.getComponent() )
                    o->showScan( scan );
            } );
        }

    private:
        juce::Component::SafePointer<KickUpdateScreen> owner;
    };

    KickUpdateScreen::KickUpdateScreen()
        : IScreen( Screen::KickUpdate )
    {
        mt::utils::addAndMakeVisible( this, header, installedLabel, driveLabel, stateLabel, howtoLabel, refresh, back );
        header.showOptions( false, false );
        header.onBackButtonPressed = [this]( bool ) { closeScreen(); };
        back.onClick               = [this]() { closeScreen(); };
        refresh.onClick            = [this]() { startScan(); };

        for( auto* l : { &installedLabel, &driveLabel, &stateLabel, &howtoLabel } )
        {
            l->setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
            l->setValueColour( ui::Theme::textColour( 2 ) );
            l->setValueJustification( juce::Justification::centredLeft );
            l->setPadding( 20 );
        }
        installedLabel.setFontHeight( 22 );
        driveLabel.setFontHeight( 22 );
        stateLabel.setFontHeight( 26 );
        howtoLabel.setFontHeight( 18 );
        howtoLabel.setValueJustification( juce::Justification::centredLeft );
        // The Kick package is installed by the pendrive bootstrap when the MPC starts with the drive inserted: the screen says
        // exactly that (no button that would only look as if it installed something).
        howtoLabel.setText( "To install: save your project, power the MPC off and on again with the USB drive inserted.\n"
                            "Your projects and settings are kept; the previous Kick is saved on the MPC as Kick.prev.",
                            juce::dontSendNotification );
        refresh.setValueColour( ui::Theme::textColour( 1 ) );
        back.setValueColour( ui::Theme::textColour( 1 ) );
        mt::utils::setValueFontHeight( 20, refresh, back );
    }

    KickUpdateScreen::~KickUpdateScreen()
    {
        scanner.reset();
    }

    void KickUpdateScreen::visibilityChanged()
    {
        if( isVisible() )
            startScan();
        else
            scanner.reset(); // stops a scan that is still running
    }

    void KickUpdateScreen::startScan()
    {
        scanner.reset();
        stateLabel.setText( "Looking for a USB drive ...", juce::dontSendNotification );
        scanner = std::make_unique<Scanner>( this );
        scanner->startThread();
    }

    void KickUpdateScreen::showScan( const Scan& scan )
    {
        auto line = []( const kick_update::PackageInfo& p ) {
            juce::String t = p.version.empty() ? juce::String( "unknown" ) : juce::String( p.version );
            if( !p.commit.empty() ) t << " (" << juce::String( p.commit ).substring( 0, 12 ) << ")";
            if( !p.buildDate.empty() ) t << "  " << juce::String( p.buildDate );
            return t;
        };
        juce::String inst = "Installed:  " + line( scan.installed );
        if( scan.hasPrevious )
            inst << "      Previous (Kick.prev):  " << ( scan.previousVersion.empty() ? juce::String( "kept" ) : juce::String( scan.previousVersion ) );
        installedLabel.setText( inst, juce::dontSendNotification );

        juce::String drive;
        switch( scan.state )
        {
            case kick_update::State::NoDrive: drive = "USB drive:  none"; break;
            case kick_update::State::NoPackage: drive = "USB drive:  " + juce::String( scan.drive ) + "  (no Kick package)"; break;
            default: drive = "USB drive:  " + juce::String( scan.drive ) + "  ->  " + line( scan.package ); break;
        }
        driveLabel.setText( drive, juce::dontSendNotification );
        stateLabel.setText( kick_update::describe( scan.state, scan.installed, scan.package, scan.reason ), juce::dontSendNotification );
        stateLabel.setValueColour( scan.state == kick_update::State::PackageBroken ? juce::Colours::orangered : ui::Theme::textColour( 2 ) );
        repaint();
    }

    void KickUpdateScreen::handleCommand( CPktMgr::HWMsg_t& msg )
    {
        // MAIN / ENTER / Data Dial press leave the screen, like the Back button; STOP is not used here.
        if( contracts::DclContractsHelper::isButtonMessage( msg ) && contracts::DclContractsHelper::getButtonState( msg ) == contracts::DeviceControlContracts::BTN_PRESSED )
        {
            switch( contracts::DclContractsHelper::getButtonCode( msg ) )
            {
                case contracts::DeviceControlContracts::EN_MAINMODE:
                case contracts::DeviceControlContracts::EN_KNOB_ENTER: closeScreen(); return;
                default: break;
            }
        }
    }

    void KickUpdateScreen::resized()
    {
        auto b = getLocalBounds();
        header.setBounds( b.removeFromTop( ui::layout::header.getHeight() ) );
        b = b.reduced( 20, 10 );
        auto bottom = b.removeFromBottom( 44 );
        back.setBounds( bottom.removeFromLeft( 140 ) );
        refresh.setBounds( bottom.removeFromRight( 140 ) );
        const int h = ( b.getHeight() - 30 ) / 4;
        installedLabel.setBounds( b.removeFromTop( h ) );
        b.removeFromTop( 10 );
        driveLabel.setBounds( b.removeFromTop( h ) );
        b.removeFromTop( 10 );
        stateLabel.setBounds( b.removeFromTop( h ) );
        b.removeFromTop( 10 );
        howtoLabel.setBounds( b );
    }

    void KickUpdateScreen::paint( juce::Graphics& g )
    {
        g.fillAll( ui::Theme::backgroundColour( 4 ) );
    }
}
