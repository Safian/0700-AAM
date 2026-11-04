
#include "../audio_components/gridview/kick_GridData.h"

namespace kick_ui
{
#define stpButtonWidth 119
#define boxButtonWidth 320
#define bbtButtonWidth 60
#define buttonHeight   44
#define controlY       7
#define sequenceX      20
#define trackX         160
#define programX       300
#define boxX           440
#define barX           780
#define beatX          860
#define tickX          940

    AppStatusbar& AppStatusbar::Instance()
    {
        // Register the shared look-and-feel before this dependent component.
        ui::KickLooknFeel::getLnf();
        static auto* instance = new AppStatusbar;
        return *instance;
    };

    AppStatusbar::AppStatusbar()
    {
        setName( "Statusbar" );

        boxInfo.setText( "", juce::dontSendNotification );
        boxInfo.setValueJustification( juce::Justification::centred );
        boxInfo.setColours( ui::Theme::textColour( 2 ), ui::Theme::backgroundColour( 5 ) );
        boxInfo.setFontHeight( 18 );
        // Progress Bar...
        progressBar.setTextToDisplay( "" );
        progressBar.setColour( juce::ProgressBar::backgroundColourId, ui::Theme::backgroundColour( 12 ) );
        progressBar.setColour( juce::ProgressBar::foregroundColourId, ui::Theme::foregroundColour( 2 ) );

        setInterceptsMouseClicks( false, true );

        for( auto* b : { &tempoBtn, &timeSigBtn } )
        {
            b->setValueColour( ui::Theme::textColour( 2 ) );
            b->setValueFontHeight( 16 );
            b->setBackgroundColour( ui::Theme::backgroundColour( 5 ) );
        }
        tempoBtn.onClick   = [this]() { editTempo(); };
        timeSigBtn.onClick = [this]() { TimeSignatureDialog::show( mt::getAudioInterface().getSelectedSequence(), [this]() { refreshTempo(); } ); };

        mt::utils::addChildComponents( this, progressBar, boxInfo, tempoBtn, timeSigBtn, left, right );

        auto& grid{ GridData::Instance() };

        grid.gridPlayHeadChanged.connect( [&]() {
            right.bbt.updatePlayHead();
        } );

        grid.gridChanged.connect( [&]() {
            left.stp.updateSelection();
            right.bbt.updatePlayHead();
        } );

        left.stp.updateSelection();
        right.bbt.updatePlayHead();

        ui::KickTempoEditor::numTapsSource = []() { return mt::getSettings().hardware.numTaps().getIntValue(); };
        refreshTempo();
        startTimerHz( 4 );
    }

    //==============================================================================
    void AppStatusbar::paint( juce::Graphics& g )
    {
        g.setColour( ui::Theme::backgroundColour( 1 ) );
        g.fillRect( getLocalBounds() );
        mt::utils::fillRoundedRect( g, boxInfo.getBounds(), ui::Theme::backgroundColour( 5 ), false, false, false, false );
    }

    void AppStatusbar::resized()
    {
        progressBar.setBoundsRelative( .0f, .0f, 1.0f, .094 );
        left.setBoundsRelative( .0097f, .2031f, .3536f, .6875f );
        tempoBtn.setBoundsRelative( .3680f, .2031f, .0950f, .6875f );
        timeSigBtn.setBoundsRelative( .4660f, .2031f, .0560f, .6875f );
        boxInfo.setBoundsRelative( .5260f, .2031f, .2300f, .6875f );
        right.setBoundsRelative( .7617f, .2031f, .2382f, .6875f );

        init = true;
    }


    void AppStatusbar::setBoxInfo( juce::String BoxInfo )
    {
        boxState.setMessage( BoxInfo.toStdString(), juce::Time::getMillisecondCounterHiRes() );
        refreshBox();
    }

    void AppStatusbar::setStanding( juce::String Text )
    {
        projectStanding = Text;
        refreshBox();
    }

    void AppStatusbar::refreshBox()
    {
        // A device fault of the startup report stands until the app is restarted (chooseStanding orders the standing texts).
        juce::String audioFault;
        {
            const auto snap = kick_startup::state().snapshot();
            const auto& a   = snap.steps[(int)kick_startup::Step::AudioEngine];
            if( a.state == kick_startup::StepState::Failed )
                audioFault = "AUDIO: " + juce::String( a.detail );
        }
        boxState.setStanding( kick_ui::statusbar_logic::chooseStanding( audioFault.toStdString(), lowStorage.toStdString(), projectStanding.toStdString(), lowMemory.toStdString() ) );
        const juce::String text = boxState.textAt( juce::Time::getMillisecondCounterHiRes() );
        if( text != shownBox )
        {
            shownBox = text;
            boxInfo.setText( text, juce::dontSendNotification );
        }
    }

    void AppStatusbar::refreshResources()
    {
        auto&           res = mt::SystemResource::getResourceInterface();
        mt::sysSizeInfo si;
        const auto      path = res.volumePath( kick_sysinfo::Volume::Internal );
        if( !path.empty() )
            res.sizeInfo( path, si );
        mt::sysRamInfo ri;
        res.ramInfo( ri );
        const auto w = kick_sysinfo::resourceWarnings( si.status, si.perSize, si.avaSize, ri.status, ri.perRam );
        lowStorage   = w.disk;
        lowMemory    = w.ram;
    }

    void AppStatusbar::refreshTempo()
    {
        auto& ae   = mt::getAudioInterface();
        int   bpm  = 0;
        int   num = 0, den = 0;
        const int seq = ae.getSelectedSequence();
        if( auto* data = ae.getSequenceData( seq ) )
            bpm = data->isMasterBPM.get() ? ae.getMasterBPM() : (int)data->tempo.get();
        ae.getTimeSignature( seq, num, den );
        const auto tempoText = juce::String( kick_ui::statusbar_logic::tempoText( bpm ) );
        const auto tsText    = juce::String( kick_ui::statusbar_logic::timeSignatureText( num, den ) );
        if( tempoText != shownTempo )
        {
            shownTempo = tempoText;
            tempoBtn.setButtonText( tempoText );
        }
        if( tsText != shownTimeSig )
        {
            shownTimeSig = tsText;
            timeSigBtn.setButtonText( tsText );
        }
    }

    void AppStatusbar::editTempo()
    {
        auto&      ae  = mt::getAudioInterface();
        const int  seq = ae.getSelectedSequence();
        auto*      data = ae.getSequenceData( seq );
        if( !data )
            return;
        const bool master = data->isMasterBPM.get(); // a sequence that follows the master tempo edits the master
        const int  cur    = master ? ae.getMasterBPM() : (int)data->tempo.get();
        auto&      editor = ui::KickTempoEditor::Instance();
        editor.setRange( (int)BPM::MIN, (int)BPM::MAX, (int)BPM::DEFAULT, 1, juce::jlimit( (int)BPM::MIN, (int)BPM::MAX, cur ) );
        editor.show( "BPM SELECTOR", [this, seq, master]( int R ) {
            if( R != UI::BUTTONS::APPLY )
                return;
            auto&     api   = mt::getAudioInterface();
            const int value = ui::KickTempoEditor::Instance().getValue();
            if( master )
                api.setMasterBPM( value );
            else
                api.setSequenceBPM( seq, value );
            refreshTempo();
        } );
    }

    void AppStatusbar::timerCallback()
    {
        if( !isVisible() ) // not isShowing(): the top-level window of the MPC is hidden
            return;
        if( resourceTicks-- <= 0 )
        {
            resourceTicks = 20; // every 5 s (the timer runs at 4 Hz)
            refreshResources();
        }
        refreshBox();
        refreshTempo();
    }

    void AppStatusbar::visibilityChanged()
    {
        auto visible = isVisible();

        for( auto& it : getChildren() )
            it->setVisible( visible );
        right.bbt.refresh();
        if( visible )
        {
            refreshBox();
            refreshTempo();
        }
    }

    //////////////////////////////////////////////////////////////////////////////////////////////
    //////////////Left Control
    //////////////////////////////////////////////////////////////////////////////////////////////

    LeftControl::LeftControl()
    {
        mt::utils::addAndMakeVisible( this, stp );
        setInterceptsMouseClicks( true, false );
    }

    void LeftControl::resized()
    {
        auto b = getLocalBounds();
        stp.setBoundsRelative( 0.F, 0.F, 1.F, 1.F );
    }
    void LeftControl::mouseDown( const juce::MouseEvent& e )
    {
        stp.displaySTPPopup();
    }
    //////////////////////////////////////////////////////////////////////////////////////////////
    //////////////Right Control
    //////////////////////////////////////////////////////////////////////////////////////////////

    RightControl::RightControl()
    {
        mt::utils::addAndMakeVisible( this, bbt );
        setInterceptsMouseClicks( true, false );
    }

    void RightControl::resized()
    {
        auto b = getLocalBounds();
        bbt.setBoundsRelative( 0.F, 0.F, 1.F, 1.F );
    }
    void RightControl::mouseDown( const juce::MouseEvent& e )
    {
        bbt.displayPopup();
    }
}
