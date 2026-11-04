#include "kick_KnobTabs.h"
namespace kick_ui
{
    Content::Content()
    {}

    Content::~Content()
    {}

    void Content::resized()
    {
        auto content = getLocalBounds();
    }

    void Content::paint( juce::Graphics& g )
    {
        mt::utils::fillRoundedRect( g, { 0, 0, 652, 460 }, ui::Theme::backgroundColour( 4 ) );
    }

    CustomTab::CustomTab( KnobGrid& Knob )
        : knob( Knob )
    {
        mt::utils::addAndMakeVisible( this, topControls, middleControls, bottomControls );
        middleControls.onButtonClick            = [&]( CustomButtons button ) { type( button ); };
        bottomControls.level.onSelectionChanged = [&]( int index, const juce::String value ) {
            if( knobnum != -1 )
            {
                auto selectedParameter = bottomControls.level.getSelectedValue();
                setParameter( knobnum, selectedParameter );
            }
        };
        // MIDI program CC list: the CC of the program of the track chosen in the Q-Link Edit (not the selected program).
        // A zero step only reads: writing a CC property sends a CC message.
        knob.parameterSelection = [&]( juce::String parameter, int value ) -> int {
            auto parameterIndex = bottomControls.level.getIndex( parameter );
            if( parameterIndex < 0 )
                return 0;
            auto& ae    = mt::getAudioInterface();
            auto* pProg = ae.getProgramDataForTrack( ae.getSelectedSequence(), indexNum );
            if( !pProg )
                return 0;
            auto valueTree = ae.getProgramVT( ae.getProgramIndexByName( pProg->name.get() ) );
            if( !valueTree.isValid() )
                return 0;
            const juce::Identifier id( "CC" + juce::String( parameterIndex ) );
            const int              current = valueTree.getProperty( id );
            if( value == 0 )
                return current;
            auto updatedValue = juce::jlimit( MIDI::MIN, MIDI::MAX, current + value );
            valueTree.setProperty( id, updatedValue, nullptr );
            return updatedValue;
        };
        topControls.isPad = [&]() {
            auto padState = topControls.type.getToggleState();
            knob.setPadState( padState );
        };
        topControls.onPadChange = [&]( int bankIndex, int padIndex ) {
            knob.setPadIndex( bankIndex, padIndex );
        };
    }

    void CustomTab::resized()
    {
        topControls.setBoundsRelative( .0f, .0f, .9969f, .2217f );
        bottomControls.setBoundsRelative( .0f, .3391f, .9969f, .6608f );
        middleControls.setBoundsRelative( .0f, .2434f, 1.0f, .0739f );
    }

    void CustomTab::setIndex( int index )
    {
        indexNum = index;
    }

    void CustomTab::setInsertsList( int insertIndex )
    {
        auto&                     ae          = mt::getAudioInterface();
        auto*                     pProg       = ae.getProgramDataForTrack( ae.getSelectedSequence(), indexNum );
        auto*                     program     = pProg ? ae.getProgram( pProg->name.get() ) : nullptr;
        std::vector<juce::String> list;

        if( program )
            for( const auto& it : program->getInsertsInfo( insertIndex ).params )
                list.push_back( it.name );

        if( list.empty() )
            list.push_back( "OFF" );

        bottomControls.level.setListOptions( list );
    }

    void CustomTab::setListInfo( mt::MTProgramType type, CustomButtons buttonName, bool topState )
    {
        if( type == mt::MTProgramType::Drum )
        {
            if( !topState )
            {
                if( buttonName == mix )
                    bottomControls.level.setListOptions( { "OFF", "Pan", "Velocity", "Solo", "Mute" } );
                else if( buttonName == program )
                    bottomControls.level.setListOptions( { "OFF", "Polyphony", "GlobalSemiTune", "GlobalFineTune" } );
                else if( buttonName == ins1 || buttonName == ins2 || buttonName == ins3 || buttonName == ins4 )
                {
                    int insertIndex = ( buttonName == ins1 ) ? 0 : ( buttonName == ins2 ) ? 1 :
                                                               ( buttonName == ins3 )     ? 2 :
                                                                                            3;
                    setInsertsList( insertIndex );
                }
            }
            else
            {
                if( buttonName == mix )
                    bottomControls.level.setListOptions( { "OFF", "Level", "Pan", "PadSolo", "PadMute" } );
                else if( buttonName == program )
                    bottomControls.level.setListOptions( { "OFF", "SemiTune", "FineTune" } );
                else if( buttonName == ins1 || buttonName == ins2 || buttonName == ins3 || buttonName == ins4 )
                {
                    int insertIndex = ( buttonName == ins1 ) ? 0 : ( buttonName == ins2 ) ? 1 :
                                                               ( buttonName == ins3 )     ? 2 :
                                                                                            3;
                    setInsertsList( insertIndex );
                }
            }
        }
        else if( type == mt::MTProgramType::Clip )
        {
            if( !topState )
            {
                if( buttonName == mix )
                    bottomControls.level.setListOptions( { "OFF", "Pan", "Velocity", "Solo", "Mute" } );
                else if( buttonName == program )
                    bottomControls.level.setListOptions( { "OFF", "Polyphony", "GlobalSemiTune", "GlobalFineTune" } );
                else if( buttonName == ins1 || buttonName == ins2 || buttonName == ins3 || buttonName == ins4 )
                {
                    int insertIndex = ( buttonName == ins1 ) ? 0 : ( buttonName == ins2 ) ? 1 :
                                                               ( buttonName == ins3 )     ? 2 :
                                                                                            3;
                    setInsertsList( insertIndex );
                }
            }
            else
            {
                if( buttonName == mix )
                    bottomControls.level.setListOptions( { "OFF", "Level", "Pan", "PadSolo", "PadMute" } );
                else if( buttonName == program )
                    bottomControls.level.setListOptions( { "OFF", "SemiTune", "FineTune" } );
                else if( buttonName == ins1 || buttonName == ins2 || buttonName == ins3 || buttonName == ins4 )
                {
                    int insertIndex = ( buttonName == ins1 ) ? 0 : ( buttonName == ins2 ) ? 1 :
                                                               ( buttonName == ins3 )     ? 2 :
                                                                                            3;
                    setInsertsList( insertIndex );
                }
            }
        }
        else if( type == mt::MTProgramType::Keygroup )
        {
            if( !topState )
            {
                if( buttonName == mix )
                    bottomControls.level.setListOptions( { "OFF", "Pan", "Velocity", "Solo", "Mute" } );
                else if( buttonName == program )
                    bottomControls.level.setListOptions( { "OFF", "Program Polyphony", "Global SemiTune", "Global FineTune" } );
                else if( buttonName == ins1 || buttonName == ins2 || buttonName == ins3 || buttonName == ins4 )
                {
                    int insertIndex = ( buttonName == ins1 ) ? 0 : ( buttonName == ins2 ) ? 1 :
                                                               ( buttonName == ins3 )     ? 2 :
                                                                                            3;
                    setInsertsList( insertIndex );
                }
            }
        }
        else if( type == mt::MTProgramType::Midi )
        {
            if( !topState && buttonName == midi )
                bottomControls.level.setListOptions( { "Bank Select", "Modulation", " Breath", " CC3", " Foot", " Portamento ", " DataEntry ", " Volume ", " Balance ", " CC9 ", " Pan ", " Expression ", " Effect1 ", " Effect2 ", " CC14 ", " CC15 ", " GenPurpose1 ", " GenPurpose2 ", " genpurpose3 ", " genpurpose4 ", " CC20 ", " CC21 ", " CC22 ", " CC23 ", " CC24 ", " CC25 ", " CC26 ", " CC27 ", " CC28 ", " CC29 ", " CC30 ", " CC31 ", " BankSelLSB ", " ModulationLSB ", " BreathLSB ", " CC35 ", " FootLSB ", " PortamentoLSB ", " DataEntryLSB ", " VolumeLSB ", " BalanceLSB ", " CC41 ", " PanLSB ", " ExpressionLSB ", " Effect1LSB ", " Effect2LSB ", " CC46 ", " CC47 ", " GenPur1LSB ", " GenPur2LSB ", " GenPur3LSB ", " GenPur4LSB ", " CC52 ", " CC53 ", " CC54 ", " CC55 ", " CC56 ", " CC57 ", " CC58 ", " CC59 ", " CC60 ", " CC61 ", " CC62 ", " CC63 ", " SustainOnOff ", " PmentoOnOff ", " SostOnOff ", " SoftPedOnOff ", " LegatoAwitch ", " Hold2 ", " SndControl1 ", " SndControl2 ", " SndControl3 ", " SndControl4 ", " SndControl5 ", " SndControl6 ", " SndControl7 ", " SndControl8 ", " SndControl9 ", " SndControl10 ", " GenPurpose5 ", " GenPurpose6 ", " GenPurpose7 ", " GenPurpose8 ", " PmentoControl ", " CC85 ", " CC86 ", " CC87 ", " HiResVelocity ", " CC89 ", " CC90 ", " Send1 ", " Send2 ", " Send3 ", " Send4 ", " Send5 ", " DataIncrement ", " DataDecrement ", " NRPNLSB ", " NRPNMSB ", " RPNLSB ", " RPNMSB ", " CC102 ", " CC103 ", " CC104 ", " CC105 ", " CC106 ", " CC107 ", " CC108 ", " CC109 ", " CC110 ", " CC111 ", " CC112 ", " CC113 ", " CC114 ", " CC115 ", " CC116 ", " CC117 ", " CC118 ", " CC119 ", " AllSoundOff ", " ResetAll ", " Localonoff ", " AllnoteOff ", " OmniModeOff ", " OmniModeOn ", " MonoMode ", " PolyMode " } );
        }
        /*else if( type == mt::MTProgramType::Cv )
        {
            if( !topState )
            {
                if( buttonName == mix )
                    bottomControls.level.setListOptions( { "OFF", "Solo", "Mute" } );
                else if( buttonName == program )
                    bottomControls.level.setListOptions( { "CV Out1", "CV Out2", "CV Out3", "CV Out4", "CV Out5", "CV Out6", "CV Out7", "CV Out8" } );
            }
        }*/
    }

    void CustomTab::type( CustomButtons buttonName )
    {
        auto& ae       = mt::getAudioInterface();
        auto  pProg    = ae.getProgramDataForTrack( ae.getSelectedSequence(), indexNum );
        auto  type     = pProg ? pProg->type.get() : mt::MTProgramType::Unknown;
        bool  topState = topControls.type.getToggleState();
        currentButton  = buttonName;
        setListInfo( type, buttonName, topState );
    }

    void CustomTab::setParameter( int Knobnum, juce::String parameter )
    {
        // Ins 1..4 parameters stay bound to their insert slot; everything else is a built-in parameter.
        int source = -1;
        if( currentButton >= ins1 && currentButton <= ins4 )
            source = currentButton - ins1;
        knob.labelInfo( Knobnum, parameter, source );
    }

    TopContent::TopContent()
    {
        mt::utils::addAndMakeVisible( this, type, typeLabel, pad, bank );
        mt::utils::makeHidden( pad, bank );
        typeLabel.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        typeLabel.setValueColour( ui::Theme::textColour( 1 ) );
        typeLabel.setValueJustification( juce::Justification::centred );
        type.setValueColour( ui::Theme::textColour( 1 ) );
        type.setValueFontHeight( 18 );
        type.onClick = [&] {
            padContent();
            if( isPad )
                isPad();
        };

        bank.onSelectionChanged = [&]( int index, juce::String name ) {
            if( onPadChange )
                onPadChange( index, pad.getSelectedIndex() );
        };
        pad.onSelectionChanged = [&]( int index, juce::String name ) {
            if( onPadChange )
                onPadChange( bank.getSelectedIndex(), index );
        };

        ui::themeChanged.connect( [&]() {
            type.setToggleButtonText( "PROGRAM", "PAD" );
        } );
    }
    void TopContent::paint( juce::Graphics& g )
    {
        mt::utils::fillRoundedRect( g, { 0, 0, 652, 102 }, ui::Theme::backgroundColour( 4 ) );
    }

    void TopContent::resized()
    {
        type.setBoundsRelative( .0306f, .3725f, .1840f, .4314f );
        typeLabel.setBoundsRelative( .0306f, .196f, .184f, .196f );
        pad.setBoundsRelative( .7852f, .196f, .184f, .6078f );
        bank.setBoundsRelative( .5858f, .196f, .184f, .6078f );
    }

    void TopContent::click( mt::MTProgramType types )
    {
        switch( types )
        {
            default:
                type.setToggleState( false, juce::dontSendNotification );
                bank.setVisible( false );
                pad.setVisible( false );
                type.setInterceptsMouseClicks( false, false );
                break;
            case mt::MTProgramType::Drum:
            case mt::MTProgramType::Clip:
                type.setInterceptsMouseClicks( true, true );
                break;
        }
    }

    void TopContent::padContent()
    {
        pad.setTitleColour( ui::Theme::textColour( 1 ) );
        bank.setTitleColour( ui::Theme::textColour( 1 ) );
        if( type.getToggleState() )
        {
            pad.setVisible( true );
            bank.setVisible( true );
        }
        else
        {
            pad.setVisible( false );
            bank.setVisible( false );
        }
    }

    MiddleContent::MiddleContent()
    {
        mt::utils::addAndMakeVisible( this, mix, program, midi, ins1, ins2, ins3, ins4 );
        mix.setRadioGroupId( (int)kick_ui::RadioGroupId::QlinkTab, juce::dontSendNotification );
        program.setRadioGroupId( (int)kick_ui::RadioGroupId::QlinkTab, juce::dontSendNotification );
        ins1.setRadioGroupId( (int)kick_ui::RadioGroupId::QlinkTab, juce::dontSendNotification );
        ins2.setRadioGroupId( (int)kick_ui::RadioGroupId::QlinkTab, juce::dontSendNotification );
        ins3.setRadioGroupId( (int)kick_ui::RadioGroupId::QlinkTab, juce::dontSendNotification );
        ins4.setRadioGroupId( (int)kick_ui::RadioGroupId::QlinkTab, juce::dontSendNotification );
        midi.setRadioGroupId( (int)kick_ui::RadioGroupId::QlinkTab, juce::dontSendNotification );

        auto onClickHandler = [this]( CustomButtons button ) { return [&, button]() { onClick( button ); }; };

        mix.onClick     = onClickHandler( CustomButtons::mix );
        program.onClick = onClickHandler( CustomButtons::program );
        ins1.onClick    = onClickHandler( CustomButtons::ins1 );
        ins2.onClick    = onClickHandler( CustomButtons::ins2 );
        ins3.onClick    = onClickHandler( CustomButtons::ins3 );
        ins4.onClick    = onClickHandler( CustomButtons::ins4 );
        midi.onClick    = onClickHandler( CustomButtons::midi );

        mix.setValueColour( ui::Theme::textColour( 1 ) );
        mix.setValueFontHeight( 18 );
        midi.setValueColour( ui::Theme::textColour( 1 ) );
        midi.setValueFontHeight( 18 );
        program.setValueColour( ui::Theme::textColour( 1 ) );
        program.setValueFontHeight( 18 );
        ins1.setValueColour( ui::Theme::textColour( 1 ) );
        ins1.setValueFontHeight( 18 );
        ins2.setValueColour( ui::Theme::textColour( 1 ) );
        ins2.setValueFontHeight( 18 );
        ins3.setValueColour( ui::Theme::textColour( 1 ) );
        ins3.setValueFontHeight( 18 );
        ins4.setValueColour( ui::Theme::textColour( 1 ) );
        ins4.setValueFontHeight( 18 );

        ui::themeChanged.connect( [&]() {
            mix.setToggleButtonText( "Mix", "Mix" );
            midi.setToggleButtonText( "MIDI CC", "MIDI CC" );
            program.setToggleButtonText( "Program", "Program" );
            ins1.setToggleButtonText( "Ins 1", "Ins 1" );
            ins2.setToggleButtonText( "Ins 2", "Ins 2" );
            ins3.setToggleButtonText( "Ins 3", "Ins 3" );
            ins4.setToggleButtonText( "Ins 4", "Ins 4" );
        } );
    }
    void MiddleContent::resized()
    {
        mix.setBoundsRelative( .0f, .0f, .158f, 1.0f );
        midi.setBoundsRelative( .0f, .0f, .158f, 1.0f );
        program.setBoundsRelative( .167f, .0f, .158f, 1.0f );
        ins1.setBoundsRelative( .3358f, .0f, .158f, 1.0f );
        ins2.setBoundsRelative( .5038f, .0f, .158f, 1.0f );
        ins3.setBoundsRelative( .671f, .0f, .158f, 1.0f );
        ins4.setBoundsRelative( .839f, .0f, .158f, 1.0f );
    }

    void MiddleContent::paint( juce::Graphics& g )
    {}

    void MiddleContent::visibilityChanged()
    {}
    void MiddleContent::updateButtons( mt::MTProgramType type )
    {
        for( auto it : getChildren() )
            it->setVisible( false );
        switch( type )
        {
            default:
                mt::utils::makeVisible( mix, program, ins1, ins2, ins3, ins4 );
                mt::utils::makeHidden( midi );
                break;
            case mt::MTProgramType::Keygroup:
                mt::utils::makeHidden( midi );
                mt::utils::makeVisible( mix, program, ins1, ins2, ins3, ins4 );
                break;
            case mt::MTProgramType::Cv:
                mt::utils::makeVisible( mix, program );
                mt::utils::makeHidden( ins1, ins2, ins3, ins4 );
                break;
            case mt::MTProgramType::Midi:
                mt::utils::makeVisible( midi );
                mt::utils::makeHidden( mix, program, ins1, ins2, ins3, ins4 );
                break;
        }
    }

    void MiddleContent::onClick( CustomButtons button )
    {
        if( onButtonClick )
            onButtonClick( button );
    }

    BottomContent::BottomContent()
    {
        mt::utils::addAndMakeVisible( this, parameter, level, momentary );
        parameter.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        parameter.setFontHeight( 18 );
        momentary.setValueColour( ui::Theme::textColour( 1 ) );
        momentary.setValueFontHeight( 18 );
    }

    void BottomContent::paint( juce::Graphics& g )
    {
        mt::utils::fillRoundedRect( g, { 0, 0, 652, 304 }, ui::Theme::backgroundColour( 4 ) );
    }

    void BottomContent::resized()
    {
        parameter.setBoundsRelative( .0168f, .0657f, .2453f, .1447f );
        level.setBoundsRelative( .5858f, .0657f, .3834f, .1447f );
        momentary.setBoundsRelative( .784f, .2697f, .1837f, .1447f );
    }
}
