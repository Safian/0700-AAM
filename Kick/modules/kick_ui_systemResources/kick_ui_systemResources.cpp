#ifdef KICK_UI_H_INCLUDE
    /* When you add this cpp file to your project, you mustn't include it in a file where you've
already included any other headers - just put it inside a file on its own, possibly with your config
flags preceding it, but don't include anything else. That also includes avoiding any automatic prefix
header files that the compiler may be using.
*/
    #error "Incorrect use of JUCE cpp file"
#endif

#include "hardware/source/kick_Resorces.h"
#include "kick_ui_systemResources.h"

namespace kick_ui
{
    namespace
    {
        juce::String megabytes( double mb )
        {
            return mb >= 1024.0 ? juce::String( mb / 1024.0, 1 ) + " GB" : juce::String( juce::roundToInt( mb ) ) + " MB";
        }
        juce::String bytesText( juce::int64 bytes ) { return megabytes( (double)bytes / ( 1024.0 * 1024.0 ) ); }
    }

    /// Samples the system once a second on its own thread; the UI reads the latest snapshot.
    class KSystemResources::Sampler : public juce::Thread
    {
    public:
        Sampler()
            : juce::Thread( "System resources" ) {}
        ~Sampler() override { stopThread( 3000 ); }

        Snapshot get() const
        {
            const juce::ScopedLock sl( lock );
            return snapshot;
        }

        void run() override
        {
            auto& res = mt::SystemResource::getResourceInterface();
            while( !threadShouldExit() )
            {
                Snapshot s;
                {
                    double cpuUsage = 0.0;
                    s.haveCpu       = res.cpuInfo( cpuUsage );
                    s.cpu           = cpuUsage;
                    mt::sysRamInfo ri;
                    res.ramInfo( ri );
                    s.haveRam    = ri.status;
                    s.ramPercent = ri.perRam;
                    s.ramUsedMb  = ri.usedRam;
                    s.ramTotalMb = ri.totalRam;
                    s.processMb  = ri.processRam;
                }
                auto volume = [&]( kick_sysinfo::Volume v, bool& have, int& percent, double& freeMb ) {
                    mt::sysSizeInfo si;
                    const auto      path = res.volumePath( v );
                    if( !path.empty() )
                        res.sizeInfo( path, si );
                    have    = si.status;
                    percent = si.perSize;
                    freeMb  = si.avaSize;
                };
                volume( kick_sysinfo::Volume::Internal, s.haveInternal, s.internalPercent, s.internalFreeMb );
                volume( kick_sysinfo::Volume::SdCard, s.haveSd, s.sdPercent, s.sdFreeMb );
                volume( kick_sysinfo::Volume::Usb, s.haveUsb, s.usbPercent, s.usbFreeMb );
                {
                    const juce::ScopedLock sl( lock );
                    snapshot = s;
                }
                // Short sleeps so stop() is quick.
                for( int i = 0; i < 10 && !threadShouldExit(); ++i )
                    wait( 100 );
            }
        }

    private:
        mutable juce::CriticalSection lock;
        Snapshot                      snapshot;
    };

    KSystemResources& KSystemResources::Instance()
    {
        ui::KickLooknFeel::getLnf();
        static auto* resources = new KSystemResources;
        return *resources;
    }

    KSystemResources::~KSystemResources()
    {
        stopTimer();
        sampler.reset();
    }

    KSystemResources::KSystemResources()
    {
        setTitle( "SYSTEM RESOURCES" );

        mt::utils::addAndMakeVisible( this, cpu, ram, audio, flash, sdcard, usb, storage, info, purgeUnused_, ok );

        auto setup = [this]( ui::KickSlider& s, juce::Justification titleJust, juce::Slider::TextEntryBoxPosition box ) {
            s.setTitle( s.getName() );
            s.setTitleColour( ui::Theme::textColour( 2 ) );
            s.setTitleFontHeight( 16.f );
            s.setEnabled( false );
            s.setRange( 0.0, 100.0 );
            s.setTextValueSuffix( " %" );
            s.setTitleProportion( .35f );
            s.setValueProportion( .35f );
            s.showTitle( true, titleJust );
            s.showValue( true, juce::Justification::centred, box );
        };
        setup( cpu, juce::Justification::centredTop, juce::Slider::TextBoxRight );
        cpu.showValue( true, juce::Justification::centredBottom, juce::Slider::TextBoxRight );
        setup( ram, juce::Justification::centred, juce::Slider::TextBoxBelow );
        setup( audio, juce::Justification::centred, juce::Slider::TextBoxBelow );
        setup( flash, juce::Justification::centred, juce::Slider::TextBoxBelow );
        setup( sdcard, juce::Justification::centred, juce::Slider::TextBoxBelow );
        setup( usb, juce::Justification::centred, juce::Slider::TextBoxBelow );

        storage.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        storage.setValueColour( ui::Theme::textColour( 2 ) );
        storage.setValueJustification( juce::Justification::centredTop );
        storage.setValueFontHeight( 16 );

        info.setValueJustification( juce::Justification::centred );
        info.setValueFontHeight( 14 );
        info.setValueColour( ui::Theme::textColour( 2 ) );

        purgeUnused_.setPlacementFlag( juce::Justification::centred );
        ok.setPlacementFlag( juce::Justification::centred );

        // Purge Unused: drops the decoded samples that no slice uses. Nothing in use is touched, and a dropped sample is
        // decoded again the next time it is needed. (A "Purge All" would have to pull sample data from under the audio
        // thread, so the button does not exist.)
        purgeUnused_.onClick = [this]() {
            if( !purgeUnused )
                return;
            const auto freed = purgeUnused();
            info.setText( freed.count > 0 ? "Purged " + juce::String( freed.count ) + " unused sample(s), " + bytesText( freed.bytes ) + " freed"
                                          : "No unused samples to purge",
                          juce::dontSendNotification );
            refreshCache();
        };
        ok.onClick = [this]() {
            mt::utils::exitDialog( this, UI::APPLY );
        };

        ui::themeChanged.connect( [this]() {
            purgeUnused_.setImages( "purgeused" );
            ok.setImages( "ok_resource" );
        } );
    }

    void KSystemResources::start()
    {
        if( !sampler )
            sampler = std::make_unique<Sampler>();
        sampler->startThread();
        info.setText( {}, juce::dontSendNotification );
        applySnapshot( Snapshot() );
        refreshCache();
        refreshAudio();
        startTimerHz( 2 );
    }

    void KSystemResources::stop()
    {
        stopTimer();
        if( sampler )
            sampler->stopThread( 3000 );
    }

    void KSystemResources::refreshCache()
    {
        purgeUnused_.setEnabled( (bool)purgeUnused && ( !getCacheStats || getCacheStats().unusedCount > 0 ) );
    }

    void KSystemResources::timerCallback()
    {
        if( sampler )
            applySnapshot( sampler->get() );
        refreshAudio();
    }

    // The audio callback load and the xrun count (an xrun = a dropout): how close the audio thread is to missing its deadline,
    // which the system CPU figure does not tell. Hidden when no audio device is running.
    void KSystemResources::refreshAudio()
    {
        double load  = 0.0;
        int    xruns = -1;
        const bool have = getAudioLoad && getAudioLoad( load, xruns );
        audio.setVisible( have );
        if( !have )
            return;
        audio.setValue( juce::jlimit( 0.0, 100.0, load * 100.0 ), juce::dontSendNotification );
        audio.setTitle( xruns >= 0 ? "AUDIO  (dropouts: " + juce::String( xruns ) + ")" : juce::String( "AUDIO" ) );
    }

    void KSystemResources::applySnapshot( const Snapshot& s )
    {
        last = s;
        cpu.setValue( s.haveCpu ? s.cpu : 0.0, juce::dontSendNotification );
        ram.setValue( s.ramPercent, juce::dontSendNotification );
        ram.setVisible( s.haveRam );
        flash.setValue( s.internalPercent, juce::dontSendNotification );
        flash.setVisible( s.haveInternal );
        sdcard.setValue( s.sdPercent, juce::dontSendNotification );
        sdcard.setVisible( s.haveSd );
        usb.setValue( s.usbPercent, juce::dontSendNotification );
        usb.setVisible( s.haveUsb );

        if( s.haveRam )
        {
            juce::String text = "RAM " + megabytes( s.ramUsedMb ) + " of " + megabytes( s.ramTotalMb ) + " used";
            if( s.processMb > 0.0 )
                text += "   Kick " + megabytes( s.processMb );
            if( getCacheStats )
            {
                const auto cs = getCacheStats();
                text += "   Samples " + juce::String( cs.count ) + " (" + bytesText( cs.bytes ) + ", " + juce::String( cs.unusedCount ) + " unused)";
                purgeUnused_.setEnabled( (bool)purgeUnused && cs.unusedCount > 0 );
            }
            // A purge result stays visible until the dialog is opened again.
            if( !info.getText().startsWith( "Purged" ) && !info.getText().startsWith( "No unused" ) )
                info.setText( text, juce::dontSendNotification );
        }
        repaint();
    }

    void KSystemResources::paint( juce::Graphics& g )
    {
        // Divider lines left and right of the STORAGE caption.
        g.setColour( ui::Theme::backgroundColour( 14 ) );
        const auto  y  = (float)storage.getBounds().getCentreY();
        const float x0 = 20.f, x1 = (float)storage.getX() - 8.f, x2 = (float)storage.getRight() + 8.f, x3 = (float)getWidth() - 20.f;
        g.drawLine( x0, y, x1, y );
        g.drawLine( x2, y, x3, y );
    }

    void KSystemResources::resized()
    {
        // Relative layout: the dialog gets its size from the caller.
        auto b = getLocalBounds().reduced( 20, 10 );
        auto buttons = b.removeFromBottom( juce::jmax( 48, b.getHeight() / 9 ) );
        info.setBounds( b.removeFromBottom( 26 ) );

        const int rowH = b.getHeight() / 7;
        cpu.setBounds( b.removeFromTop( rowH ).reduced( 60, 0 ) );
        ram.setBounds( b.removeFromTop( rowH ).reduced( 60, 0 ) );
        audio.setBounds( b.removeFromTop( rowH ).reduced( 60, 0 ) );
        storage.setBounds( b.removeFromTop( 24 ).withSizeKeepingCentre( 100, 24 ) );
        const int volH = b.getHeight() / 3;
        flash.setBounds( b.removeFromTop( volH ).reduced( 60, 0 ) );
        sdcard.setBounds( b.removeFromTop( volH ).reduced( 60, 0 ) );
        usb.setBounds( b.reduced( 60, 0 ) );

        const int bw = 160;
        purgeUnused_.setBounds( buttons.removeFromLeft( buttons.getWidth() / 2 ).withSizeKeepingCentre( bw, 44 ) );
        ok.setBounds( buttons.withSizeKeepingCentre( bw, 44 ) );
    }
}
