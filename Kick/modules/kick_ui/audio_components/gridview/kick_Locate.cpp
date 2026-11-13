#pragma once

namespace mt::ac
{
    /////////////////////////////////////////
    //LocateDialog
    ////////////////////////////////////////

    struct QuickLocatorSlot
    {
        int bar = 1;
        int beat = 1;
        int tick = 0;
    };

    static std::array<QuickLocatorSlot, 6> s_quickLocators = {{
        { 1, 1, 0 },
        { 2, 1, 0 },
        { 3, 1, 0 },
        { 5, 1, 0 },
        { 9, 1, 0 },
        { 17, 1, 0 }
    }};

    class LocateDialog : public juce::Component
        , public juce::Button::Listener
    {
    public:
        std::function<void( int, int, int, int )> bbtCallBack;
        LocateDialog( int, int, int );
        ~LocateDialog();

    private:
        void resized() override;
        void paint( juce::Graphics& g ) override;
        void buttonClicked( juce::Button* ) override;
        void close();
        void applyTimeSignature(); // beat / tick ranges follow the time signature of the selected sequence
        juce::String getLocatorText( int idx ) const;
        void updateStoreBtnStyle();

        ui::KickImageButton timeSigBtn{ "Time signature", "" };
        ui::KickSlider      start{ "SEQ LENGTH", juce::Slider::LinearBar };
        ui::KickSlider      bar{ "BAR", juce::Slider::LinearBar };
        ui::KickSlider      beat{ "BEAT", juce::Slider::LinearBar };
        ui::KickSlider      tick{ "TICKS", juce::Slider::LinearBar };
        ui::KickImageButton cancelBtn{ "Cancel", "Icon_Cancel" }, applyBtn{ "Apply", "Icon_Apply" };

        ui::KickLabel locatorsTitle{ "QUICK LOCATORS" };
        ui::KickButton storeBtn{ "Store", "STORE" };
        std::array<ui::KickButton, 6> locButtons;
        bool m_storeActive = false;

        ui::KickLabel startLable{ "START" };
        ui::KickLabel barLable{ "BAR" };
        ui::KickLabel beatLable{ "BEAT" };
        ui::KickLabel tickLable{ "TICK" };

        int startInt = 1, barInt = 1, beatInt = 1, tickInt = 1;
        sigslot::scoped_connection themeConn; // a theme change must not reach a closed dialog
    };

    LocateDialog::LocateDialog( int barValue, int beatValue, int tickvalue )
        : barInt( barValue ), beatInt( beatValue ), tickInt( tickvalue )
    {
        setSize( 700, 480 );
        // The UI counts bars and beats from 1 (the engine from 0); a tick is 0..95 (96 per quarter note).
        auto& ae = getAudioInterface();
        bar.setRange( 1, juce::jmax( 1, ae.getSequenceLength( ae.getSelectedSequence() ) ), 1 );
        start.setVisible( false ); // no sequence-length field: the seek does not change the sequence

        bar.setValue( juce::jmax( 1, barInt ), juce::dontSendNotification );
        applyTimeSignature();
        beat.setValue( juce::jlimit( (int)beat.getMinimum(), (int)beat.getMaximum(), beatInt ), juce::dontSendNotification );
        tick.setValue( juce::jlimit( 0, (int)tick.getMaximum(), tickInt ), juce::dontSendNotification );

        utils::addAndMakeVisible( this, start, bar, beat, tick, cancelBtn, applyBtn, startLable, barLable, beatLable, tickLable, timeSigBtn );
        timeSigBtn.setValueColour( ui::Theme::textColour( 2 ) );
        timeSigBtn.setValueFontHeight( 18 );
        timeSigBtn.setBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        timeSigBtn.onClick = [this]() {
            auto& api = getAudioInterface();
            kick_ui::TimeSignatureDialog::show( api.getSelectedSequence(), [this]() { applyTimeSignature(); } );
        };
        start.setVisible( false );
        start.showTitleAndValue( true, juce::Slider::TextBoxBelow, juce::Justification::centred );
        bar.showTitleAndValue( true, juce::Slider::TextBoxBelow, juce::Justification::centred );
        beat.showTitleAndValue( true, juce::Slider::TextBoxBelow, juce::Justification::centred );
        tick.showTitleAndValue( true, juce::Slider::TextBoxBelow, juce::Justification::centred );

        startLable.setValueJustification( juce::Justification::centred );
        startLable.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        startLable.setValueFontHeight( 18.f );
        barLable.setValueJustification( juce::Justification::centred );
        barLable.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        barLable.setValueFontHeight( 18.f );
        beatLable.setValueJustification( juce::Justification::centred );
        beatLable.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        beatLable.setValueFontHeight( 18.f );
        tickLable.setValueJustification( juce::Justification::centred );
        tickLable.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        tickLable.setValueFontHeight( 18.f );

        locatorsTitle.setValueJustification( juce::Justification::centred );
        locatorsTitle.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        locatorsTitle.setValueFontHeight( 16.f );
        utils::addAndMakeVisible( this, locatorsTitle, storeBtn );

        updateStoreBtnStyle();
        storeBtn.onClick = [this]() {
            m_storeActive = !m_storeActive;
            updateStoreBtnStyle();
        };

        for( size_t i = 0; i < 6; ++i )
        {
            locButtons[i].setButtonText( getLocatorText( (int)i ) );
            locButtons[i].setValueFontHeight( 14.f );
            utils::addAndMakeVisible( this, locButtons[i] );
            locButtons[i].onClick = [this, idx = (int)i]() {
                if( m_storeActive )
                {
                    s_quickLocators[(size_t)idx].bar = (int)bar.getValue();
                    s_quickLocators[(size_t)idx].beat = (int)beat.getValue();
                    s_quickLocators[(size_t)idx].tick = (int)tick.getValue();
                    locButtons[(size_t)idx].setButtonText( getLocatorText( idx ) );
                    m_storeActive = false;
                    updateStoreBtnStyle();
                }
                else
                {
                    bar.setValue( s_quickLocators[(size_t)idx].bar, juce::sendNotification );
                    beat.setValue( s_quickLocators[(size_t)idx].beat, juce::sendNotification );
                    tick.setValue( s_quickLocators[(size_t)idx].tick, juce::sendNotification );
                }
            };
        }

        applyBtn.onClick = [this]() {
            startInt = (int)start.getValue();
            barInt   = (int)bar.getValue();
            beatInt  = (int)beat.getValue();
            tickInt  = (int)tick.getValue();
            if( bbtCallBack )
                bbtCallBack( startInt, barInt, beatInt, tickInt );
            utils::exitDialog( this, UI::APPLY );
        };
        cancelBtn.onClick = [this]() {
            utils::exitDialog( this, UI::CANCEL );
        };

        themeConn = ui::themeChanged.connect( [this]() {
            cancelBtn.setImages( "Icon_Cancel" );
            applyBtn.setImages( "Icon_Apply" );
            updateStoreBtnStyle();
        } );
    }

    LocateDialog::~LocateDialog() {}

    juce::String LocateDialog::getLocatorText( int idx ) const
    {
        const auto& loc = s_quickLocators[(size_t)idx];
        return juce::String( idx + 1 ) + ":  " + juce::String( loc.bar ) + "." + juce::String( loc.beat ) + "." + juce::String( loc.tick );
    }

    void LocateDialog::updateStoreBtnStyle()
    {
        if( m_storeActive )
        {
            storeBtn.setButtonText( "STORE (ACTIVE)" );
            storeBtn.setColours( juce::Colours::white, ui::Theme::outlineColour( 2 ), juce::Colour( 0xffe53935 ), 2 );
        }
        else
        {
            storeBtn.setButtonText( "STORE" );
            storeBtn.setColours( ui::Theme::textColour( 1 ), ui::Theme::outlineColour( 1 ), ui::Theme::backgroundColour( 9 ), 1 );
        }
    }

    void LocateDialog::applyTimeSignature()
    {
        // A beat is a note of the denominator (6/8: an eighth), so the beats of a bar are the numerator and a beat has
        // PPQN * 4 / denominator ticks.
        auto& api = getAudioInterface();
        int   num = 4, den = 4;
        api.getTimeSignature( api.getSelectedSequence(), num, den );
        const int ticksPerBeat = juce::jmax( 1, MTTimeSignature::defaultPPQN * 4 / juce::jmax( 1, den ) );
        beat.setRange( 1, juce::jmax( 1, num ), 1 );
        tick.setRange( 0, ticksPerBeat - 1, 1 );
        beat.setValue( juce::jlimit( 1, juce::jmax( 1, num ), (int)beat.getValue() ), juce::dontSendNotification );
        tick.setValue( juce::jlimit( 0, ticksPerBeat - 1, (int)tick.getValue() ), juce::dontSendNotification );
        timeSigBtn.setButtonText( "Time signature  " + juce::String( num ) + "/" + juce::String( den ) );
        bar.setRange( 1, juce::jmax( 1, api.getSequenceLength( api.getSelectedSequence() ) ), 1 );
    }

    void LocateDialog::paint( juce::Graphics& g )
    {
        g.setColour( ui::Theme::textColour( 5 ) );
        // Vertical divider between left sliders and right locators
        g.drawLine( { 375.0f, 20.0f, 375.0f, 390.0f } );
        // Horizontal divider above Cancel / Apply
        g.drawLine( { 20.0f, 405.0f, (float)getWidth() - 20.0f, 405.0f } );
    }

    void LocateDialog::resized()
    {
        const int leftX = 20;
        const int leftW = 340;
        bar.setBounds( leftX, 20, leftW, 70 );
        beat.setBounds( leftX, 100, leftW, 70 );
        tick.setBounds( leftX, 180, leftW, 70 );
        timeSigBtn.setBounds( leftX, 260, leftW, 40 );

        cancelBtn.setBounds( leftX, 420, 150, 40 );
        applyBtn.setBounds( getWidth() - 170, 420, 150, 40 );

        // Right column: Quick locators
        const int rightX = 390;
        const int rightW = getWidth() - rightX - 20; // 290
        locatorsTitle.setBounds( rightX, 20, rightW, 25 );
        storeBtn.setBounds( rightX, 55, rightW, 35 );

        const int colW = ( rightW - 10 ) / 2; // ~140
        for( int i = 0; i < 6; ++i )
        {
            const int r = i / 2;
            const int c = i % 2;
            const int bx = rightX + c * ( colW + 10 );
            const int by = 105 + r * 55;
            locButtons[(size_t)i].setBounds( bx, by, colW, 45 );
        }
    }

    void LocateDialog::close()
    {
        utils::exitDialog( this, UI::CANCEL ); // this dialog's own modal, not whichever is on top
    }

    void LocateDialog::buttonClicked( juce::Button* ) {}

    /////////////////////////////////////////
    //Locate
    ////////////////////////////////////////

    Locate::Locate()
    {
        setInterceptsMouseClicks( false, false );

        barBeatTickLabel.setValueJustification( juce::Justification::centred );
        barBeatTickLabel.setTitleJustification( juce::Justification::centredTop );
        barBeatTickLabel.showTitle( true );
        barBeatTickLabel.setTitleColour( ui::Theme::textColour( 1 ) );
        barBeatTickLabel.setValueFontHeight( 18.f );
        barBeatTickLabel.setTitleFontHeight( 16.f );
        barBeatTickLabel.setTitleProportion( .4f );
        barBeatTickLabel.setValueBackgroundColour( ui::Theme::backgroundColour( 13 ) );

        utils::addAndMakeVisible( this, barBeatTickLabel );
        barBeatTickLabel.setText( juce::String( 0 ).paddedLeft( '0', 3 ), juce::dontSendNotification );
    }

    Locate::~Locate()
    {
    }

    void Locate::resized()
    {
        barBeatTickLabel.setBoundsRelative( .4878f, .0f, .4459f, 1.0f );
    }

    void Locate::displayPopup()
    {
        auto dlg = std::make_unique<LocateDialog>( playHeadBnB.bar + 1, playHeadBnB.beat + 1, playHeadBnB.tick );
        dlg->bbtCallBack = [this]( int /*unused*/, int barValue, int beatValue, int tickValue ) {
            // A real locate: the engine seeks (stopped: parked for the next PLAY; playing: jumps on the next tick). The
            // playhead snapshot is written by the tick thread only.
            const int seq = ae.getSelectedSequence();
            if( ae.seekToBar( seq, barValue - 1 ) < 0 )
                return;
            const int barStart = ae.getSequenceSeekTick( seq );
            int num = 4, den = 4;
            ae.getTimeSignature( seq, num, den );
            const int ticksPerBeat = juce::jmax( 1, MTTimeSignature::defaultPPQN * 4 / juce::jmax( 1, den ) ); // a beat is a note of the denominator
            if( barStart >= 0 && ( beatValue > 1 || tickValue > 0 ) )
                ae.seekToTick( seq, barStart + ( beatValue - 1 ) * ticksPerBeat + tickValue );
            barBeatTickLabel.setText( getBarBeatTickInfo( barValue, beatValue, tickValue ).paddedLeft( '0', 3 ), juce::dontSendNotification );
        };
        const auto bounds = dlg->getBounds();
        utils::showDialogOwned<LocateDialog>( "LOCATE", std::move( dlg ), nullptr, bounds );
    }

    void Locate::updatePlayHead()
    {
        barBeatTickLabel.setText( getBarBeatTickInfo( playHeadBnB.bar + 1, playHeadBnB.beat + 1, playHeadBnB.tick ).paddedLeft( '0', 3 ), juce::dontSendNotification );
    }

    void Locate::refresh()
    {
        updatePlayHead();
        repaint();
    }

    juce::String Locate::getBarBeatTickInfo( int barValue, int beatValue, int tickValue )
    {
        juce::String beats;
        juce::String ticks;
        juce::String barInfo         = juce::String( ( barValue == -1 ? 0 : barValue ) ) + " : ";
        juce::String beatInfo        = ( ( beatValue >= 10 ) ? ( beats = juce::String( beatValue ) ) : juce::String( beats = "0" + juce::String( beatValue ) ) ) + " : ";
        juce::String tickInfo        = ( ( tickValue >= 100 ) ? ( ticks = juce::String( tickValue ) ) : ( ( tickValue >= 10 ) ? ( juce::String( ( ticks = "0" + juce::String( tickValue ) ) ) ) : juce::String( ticks = "00" + juce::String( tickValue ) ) ) );
        juce::String barBeatTickInfo = juce::String( barInfo + beatInfo + tickInfo );

        return barBeatTickInfo;
    }

    /////////////////////////////////////////
    //LocateSTPDialog
    ////////////////////////////////////////
    class LocateSTPDialog : public juce::Component
        , public juce::Button::Listener
    {
    public:
        std::function<void( juce::String, juce::String, juce::String )> stpCallBack;

        LocateSTPDialog();
        ~LocateSTPDialog();

        // The dialog applies a sequence change at once (the track / program lists depend on it): any exit that is not Apply
        // (Cancel, the X, a tap outside) puts the selection back.
        void restoreSelection();

    private:
        bool applied{ false };
        int  trackRow{ 0 }, progRow{ 0 };
        void updateTrackAndProgramLists();
        int               stpEnumNum = -1;
        void              resized() override;
        void              buttonClicked( juce::Button* ) override;
        void              close( int exitCode );
        void              updateList();
        MTAudioInterface& ae{ getAudioInterface() };

        std::vector<juce::String> seqList{};
        std::vector<juce::String> trackList{};
        std::vector<juce::String> progList{};
        ui::KickListBox           seq{ "SEQUENCE", {} }, track{ "TRACK", { "Track-1" } }, prog{ "PROGRAM", { "Program-1" } };
        ui::KickImageButton       cancelBtn{ "Cancel", "Icon_Cancel" }, applyBtn{ "Apply", "Icon_Apply" };
        juce::String              seqString, trackString, progString;
        int                       seqRow{ 0 };
        sigslot::scoped_connection themeConn;
    };

    LocateSTPDialog::LocateSTPDialog()
    {
        seqRow   = ae.getSelectedSequence();
        trackRow = ae.getSelectedTrack();
        progRow  = ae.getSelectedProgram();

        setSize( 604, 560 );

        utils::addAndMakeVisible( this, cancelBtn, applyBtn, seq, track, prog );
        seq.setJustificationType( juce::Justification::centred );
        track.setJustificationType( juce::Justification::centred );
        prog.setJustificationType( juce::Justification::centred );

        seq.setTitleColour( ui::Theme::textColour( 1 ) );
        track.setTitleColour( ui::Theme::textColour( 1 ) );
        prog.setTitleColour( ui::Theme::textColour( 1 ) );

        applyBtn.onClick = [&]() {
            seqString   = seq.getSelectedValue();
            trackString = track.getSelectedValue();
            progString  = prog.getSelectedValue();
            if( stpCallBack )
                stpCallBack( seqString, trackString, progString );
            ae.setSelectedSequence( seq.getSelectedIndex() );
            ae.setSelectedTrack( track.getSelectedIndex() );
            ae.setSelectedProgram( prog.getSelectedIndex() );
            ae.setProgramToTrack( prog.getSelectedIndex(), seq.getSelectedIndex(), track.getSelectedIndex() );
            applied = true;
            close( UI::APPLY );
        };
        cancelBtn.onClick = [&]() {
            close( UI::CANCEL );
        };

        seq.onSelectionChanged = [&]( int row, const juce::String& ) {
            ae.setSelectedSequence( row );
            updateTrackAndProgramLists(); // the tracks of the new sequence
        };
        track.onSelectionChanged = [&]( int row, const juce::String& ) {
            ae.setSelectedTrack( row );
            auto* pData = ae.getProgramDataForTrack( ae.getSelectedSequence(), row );
            if( pData != nullptr )
            {
                int pIdx = ae.getProgramIndexByName( pData->getName() );
                if( pIdx >= 0 )
                    prog.setSelected( pIdx, juce::dontSendNotification );
            }
        };
        prog.onSelectionChanged = [&]( int row, const juce::String& ) {
            ae.setSelectedProgram( row );
        };
        updateList();

        themeConn = ui::themeChanged.connect( [this]() {
            applyBtn.setImages( "Icon_Apply" );
            cancelBtn.setImages( "Icon_Cancel" );
        } );
    }

    LocateSTPDialog::~LocateSTPDialog() {}

    void LocateSTPDialog::resized()
    {
        seq.setBoundsRelative( .2698f, .1423f, .4586f, .1307f );
        track.setBoundsRelative( .2698f, .3386f, .4586f, .1307f );
        prog.setBoundsRelative( .2698f, .5538f, .4586f, .1307f );
        cancelBtn.setBoundsRelative( .0298f, .9f, .4586f, .0596f );
        applyBtn.setBoundsRelative( .5066f, .9f, .4586f, .0596f );
    }

    void LocateSTPDialog::close( int exitCode )
    {
        utils::exitDialog( this, exitCode ); // this dialog's own modal, not whichever is on top
    }

    void LocateSTPDialog::restoreSelection()
    {
        if( applied )
            return;
        ae.setSelectedSequence( seqRow );
        ae.setSelectedTrack( trackRow );
        ae.setSelectedProgram( progRow );
    }

    void LocateSTPDialog::updateTrackAndProgramLists()
    {
        trackList = ae.getTrackList( ae.getSelectedSequence() );
        progList  = ae.getProgramList();
        track.setNumColumns( juce::jmax( 1, (int)trackList.size() ) );
        track.setListOptions( trackList, ae.getSelectedTrack() );

        auto* pData = ae.getProgramDataForTrack( ae.getSelectedSequence(), ae.getSelectedTrack() );
        int pIdx = pData != nullptr ? ae.getProgramIndexByName( pData->getName() ) : ae.getSelectedProgram();
        if( pIdx < 0 ) pIdx = ae.getSelectedProgram();

        prog.setNumColumns( juce::jmax( 1, (int)progList.size() ) );
        prog.setListOptions( progList, pIdx );
    }

    void LocateSTPDialog::updateList()
    {
        seqList.clear();
        trackList.clear();
        progList.clear();
        seqList   = ae.getSequenceList();
        trackList = ae.getTrackList( ae.getSelectedSequence() );
        progList  = ae.getProgramList();

        seq.setNumColumns( ae.getNumSequences() );
        seq.setListOptions( seqList, ae.getSelectedSequence() );

        track.setNumColumns( juce::jmax( 1, (int)trackList.size() ) );
        track.setListOptions( trackList, ae.getSelectedTrack() );

        auto* pData = ae.getProgramDataForTrack( ae.getSelectedSequence(), ae.getSelectedTrack() );
        int pIdx = pData != nullptr ? ae.getProgramIndexByName( pData->getName() ) : ae.getSelectedProgram();
        if( pIdx < 0 ) pIdx = ae.getSelectedProgram();

        prog.setNumColumns( juce::jmax( 1, (int)progList.size() ) );
        prog.setListOptions( progList, pIdx );
    }

    void LocateSTPDialog::buttonClicked( juce::Button* ) {}

    /////////////////////////////////////////////////////////
    /////////LocateSTP
    /////////////////////////////////////////////////////////

    LocateSTP::LocateSTP()
    {
        setInterceptsMouseClicks( false, false );
        sequenceLable.setValueJustification( juce::Justification::centred );
        sequenceLable.setTitleJustification( juce::Justification::centredTop );
        sequenceLable.showTitle( true );
        sequenceLable.setTitleColour( ui::Theme::textColour( 1 ) );
        sequenceLable.setValueFontHeight( 18.f );
        sequenceLable.setTitleFontHeight( 16.f );
        sequenceLable.setTitleProportion( .4f );
        sequenceLable.setValueBackgroundColour( ui::Theme::backgroundColour( 13 ) );

        trackLable.setValueJustification( juce::Justification::centred );
        trackLable.setTitleJustification( juce::Justification::centredTop );
        trackLable.showTitle( true );
        trackLable.setTitleColour( ui::Theme::textColour( 1 ) );
        trackLable.setValueFontHeight( 18.f );
        trackLable.setTitleFontHeight( 16.f );
        trackLable.setTitleProportion( .4f );
        trackLable.setValueBackgroundColour( ui::Theme::backgroundColour( 13 ) );

        programLable.setValueJustification( juce::Justification::centred );
        programLable.setTitleJustification( juce::Justification::centredTop );
        programLable.showTitle( true );
        programLable.setTitleColour( ui::Theme::textColour( 1 ) );
        programLable.setValueFontHeight( 18.f );
        programLable.setTitleFontHeight( 16.f );
        programLable.setTitleProportion( .4f );
        programLable.setValueBackgroundColour( ui::Theme::backgroundColour( 13 ) );

        utils::addAndMakeVisible( this, sequenceLable, trackLable, programLable );

        signals::onSequenceSelectionChange.connect( [this]( const MTSelected& Selected ) {
            juce::MessageManager::callAsync( [this]() { updateSelection(); } );
        } );
    }

    LocateSTP::~LocateSTP()
    {
    }

    void LocateSTP::resized()
    {
        sequenceLable.setBoundsRelative( .0238f, .0f, .2840f, 1.0f );
        trackLable.setBoundsRelative( .3341f, .0f, .2840f, 1.0f );
        programLable.setBoundsRelative( .6873f, .0f, .2840f, 1.0f );
    }

    void LocateSTP::displaySTPPopup()
    {
        auto dlg = std::make_unique<LocateSTPDialog>();
        dlg->stpCallBack = [this]( juce::String seqString, juce::String trackString, juce::String progString ) {
            sequenceLable.setText( seqString, juce::dontSendNotification );
            trackLable.setText( trackString, juce::dontSendNotification );
            programLable.setText( progString, juce::dontSendNotification );
        };
        const auto bounds = dlg->getBounds();
        utils::showDialogOwned<LocateSTPDialog>(
            "PLAYHEAD CONTROL MENU", std::move( dlg ),
            [this]( int, LocateSTPDialog& d ) {
                d.restoreSelection(); // any exit but Apply
                updateSelection();
            },
            bounds );
    }

    void LocateSTP::updateSelection()
    {
        auto pSeq      = ae.getSequenceData( ae.getSelectedSequence() );
        auto pTrack    = ae.getTrackData( ae.getSelectedSequence(), ae.getSelectedTrack() );
        auto pTrackPgm = ae.getProgramDataForTrack( ae.getSelectedSequence(), ae.getSelectedTrack() );
        sequenceLable.setText( pSeq ? pSeq->name.get() : "", juce::dontSendNotification );
        trackLable.setText( pTrack ? pTrack->name.get() : "", juce::dontSendNotification );
        programLable.setText( pTrackPgm ? pTrackPgm->name.get() : "", juce::dontSendNotification );
    }

}
