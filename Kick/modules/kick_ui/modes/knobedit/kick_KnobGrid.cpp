#include "kick_KnobGrid.h"
namespace kick_ui
{

#define qlinkGridHorSpacing 7
#define qlinkGridVerSpacing 16
#define qlinkTotalGridKnobs 16
#define qlinkTotalRows      4
#define qlinkTotalColumns   4

    namespace knobgrid
    {
        constexpr int knobWidth  = 70;
        constexpr int knobHeight = 80;
        constexpr int startX     = 20;
        constexpr int startY     = 74;
    }

    KnobGrid::KnobGrid()
    {
        mt::utils::addAndMakeVisible( this, label, knobdata );
        mt::utils::makeHidden( knobdata );

        for( int i = 0; i < qlinkTotalGridKnobs; i++ )
        {
            std::unique_ptr<ui::KickRotary>& ptr = qlinkKnobs.emplace_back( std::make_unique<ui::KickRotary>( "OFF" ) );
            ptr->showTitleAndValue( true, juce::Slider::TextBoxBelow, juce::Justification::centred );
            ptr->setDisplayProportions( .6f, .2f, .2f );
            ptr->addMouseListener( this, true );
            ptr->setRange( 0, 1, 0 );
            ptr->setFocus( true );
            addAndMakeVisible( ptr.get() );
            ptr->onValueChanged = [this, i]( double value ) {
                auto data = juce::String( "QLINK - " ) + juce::String( i + 1 );
                knobdata.setText( data, juce::dontSendNotification );
                if( onknobSelection )
                    onknobSelection( i );

                if( !initialized )
                    return;
            };
        }
        label.setValueJustification( juce::Justification::centredLeft );
        label.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        label.setValueColour( ui::Theme::textColour( 1 ) );
        knobdata.setFontHeight( 20 );
        knobdata.setValueJustification( juce::Justification::centredLeft );
        restoreAssignments();
    }
    KnobGrid::~KnobGrid()
    {
    }

    void KnobGrid::paint( juce::Graphics& g )
    {
        mt::utils::fillRoundedRect( g, { 0, 0, 342, 460 }, ui::Theme::backgroundColour( 4 ) );

        int y      = knobgrid::startY;
        int width  = ( knobgrid::knobWidth * qlinkTotalColumns ) + ( qlinkGridHorSpacing * ( qlinkTotalColumns - 1 ) );
        int height = knobgrid::knobHeight;

        if( state >= 1 && state <= qlinkTotalRows )
            y = knobgrid::startY + ( state - 1 ) * ( knobgrid::knobHeight + qlinkGridVerSpacing );

        if( !knob.getMode() )
            mt::utils::renderRoundedRect( g, { knobgrid::startX, y, width, height }, ui::Theme::backgroundColour( 6 ) );
    }

    void KnobGrid::setInfoText2( juce::String txt )
    {
        infoText = txt;
        label.setText( infoText, juce::dontSendNotification );
    }

    void KnobGrid::labelBounds( bool isScreenClicked )
    {
        knobdata.setVisible( isScreenClicked );
        screenClicked = !isScreenClicked;
        resized();
    }

    void KnobGrid::resized()
    {
        knobdata.setBoundsRelative( .4883, .0217f, .4824f, .0956f );
        if( screenClicked )
            label.setBoundsRelative( .0584f, .0543f, .883f, .0434f );
        else
            label.setBoundsRelative( .0584f, .0543f, .3128f, .0434f );

        int x = knobgrid::startX;
        int y = knobgrid::startY;
        for( unsigned int i = 0; i < qlinkTotalRows; i++ )
        {
            for( unsigned int j = 0; j < qlinkTotalColumns; j++ )
            {
                std::unique_ptr<ui::KickRotary>& qlinkK = qlinkKnobs[i * qlinkTotalColumns + j];
                qlinkK->setBounds( x, y, knobgrid::knobWidth, knobgrid::knobHeight );
                x += knobgrid::knobWidth + qlinkGridHorSpacing;
            }
            y += knobgrid::knobHeight + qlinkGridVerSpacing;
            x = knobgrid::startX;
        }
    }

    namespace
    {
        constexpr const char* kAssignTree = "QLINKASSIGN";
    }

    void KnobGrid::labelInfo( int knobnum, juce::String name, int source )
    {
        if( knobnum < 0 || knobnum >= (int)qlinkKnobs.size() )
            return;
        qlinkKnobs[knobnum]->setTitle( name );
        sources[knobnum] = source;
        applyRange( knobnum );
        refreshKnob( knobnum );
        qlinkKnobs[knobnum]->repaint();

        // The Custom assignment survives a restart (global, in the settings file).
        auto tree = mt::getSettings().getState().getOrCreateChildWithName( kAssignTree, nullptr );
        tree.setProperty( juce::Identifier( "k" + juce::String( knobnum ) ), juce::String( source ) + "|" + name, nullptr );
    }

    void KnobGrid::restoreAssignments()
    {
        auto tree = mt::getSettings().getState().getChildWithName( kAssignTree );
        if( !tree.isValid() )
            return;
        for( int i = 0; i < (int)qlinkKnobs.size(); ++i )
        {
            auto text = tree.getProperty( juce::Identifier( "k" + juce::String( i ) ) ).toString();
            auto bar  = text.indexOfChar( '|' );
            if( bar < 1 )
                continue;
            qlinkKnobs[i]->setTitle( text.substring( bar + 1 ) );
            sources[i] = text.substring( 0, bar ).getIntValue();
            applyRange( i );
        }
    }

    void KnobGrid::setRow( int row )
    {
        state = juce::jlimit( 0, qlinkTotalRows, row + 1 );
        repaint();
    }

    // Range of the knob display of its parameter.
    void KnobGrid::applyRange( int knobnum )
    {
        auto& k = *qlinkKnobs[knobnum];
        if( sources[knobnum] >= 0 )
        {
            k.setRange( 0.0, 1.0, 0.01 );
            return;
        }
        const auto title = k.getTitle().toStdString();
        const auto param = isPadState ? knob_logic::parsePadParam( title ) : knob_logic::parseProgramParam( title );
        if( param == knob_logic::Param::None )
        {
            k.setRange( MIDI::MIN, MIDI::MAX, MIDI::STEP ); // MIDI CC knobs (the MIDI program list) and OFF
            return;
        }
        const auto r = knob_logic::isSwitch( param ) ? knob_logic::Range{ 0.0, 1.0, 1.0 } : knob_logic::rangeOf( param );
        k.setRange( r.min, r.max, r.step );
    }

    void KnobGrid::refreshAll()
    {
        for( int i = 0; i < (int)qlinkKnobs.size(); ++i )
        {
            applyRange( i );
            refreshKnob( i );
        }
    }

    // The engine value of the parameter a knob is assigned to, in the units of the knob display.
    bool KnobGrid::readValue( int knobnum, double& out )
    {
        auto&      ae    = mt::getAudioInterface();
        auto*      pProg = ae.getProgramDataForTrack( ae.getSelectedSequence(), indexNum );
        if( !pProg )
            return false;
        auto* program = ae.getProgram( pProg->name.get() );
        if( !program )
            return false;

        const auto title  = qlinkKnobs[knobnum]->getTitle();
        const int  source = sources[knobnum];
        if( source >= 0 )
        {
            auto info  = program->getInsertsInfo( source );
            int  index = 0;
            for( const auto& param : info.params )
            {
                if( title == param.name )
                {
                    out = program->getParameterValue( source, index );
                    return true;
                }
                ++index;
            }
            return false;
        }
        if( program->type.get() == mt::MTProgramType::Midi )
        {
            if( !parameterSelection )
                return false;
            out = parameterSelection( title, 0 ); // a zero step only reads
            return true;
        }
        if( program->type.get() != mt::MTProgramType::Drum && program->type.get() != mt::MTProgramType::Clip && program->type.get() != mt::MTProgramType::Keygroup )
            return false;

        using knob_logic::Param;
        if( isPadState )
        {
            const int note = knob_logic::padNote( bankNum, padNum );
            switch( knob_logic::parsePadParam( title.toStdString() ) )
            {
                case Param::PadLevel: out = mt::mixlogic::gainToMidi127( (float)program->getNoteGain( note ) ); return true;
                case Param::PadPan: out = program->getNotePan( note ); return true;
                case Param::PadSemi: out = program->getNoteSemi( note ); return true;
                case Param::PadFine: out = program->getNoteFine( note ); return true;
                case Param::PadMute: out = program->isNoteMute( note ) ? 1 : 0; return true;
                case Param::PadSolo: out = program->isNoteSolo( note ) ? 1 : 0; return true;
                default: return false;
            }
        }
        switch( knob_logic::parseProgramParam( title.toStdString() ) )
        {
            case Param::Pan: out = program->getPan(); return true;
            case Param::Level: out = program->getLevel(); return true;
            case Param::Fine: out = program->getFine(); return true;
            case Param::Semi: out = program->getSemi(); return true;
            case Param::Poly: out = program->getPolyphony(); return true;
            case Param::Mute: out = program->getMute() ? 1 : 0; return true;
            case Param::Solo: out = program->getSolo() ? 1 : 0; return true;
            default: return false;
        }
    }

    void KnobGrid::refreshKnob( int knobnum )
    {
        double v = 0;
        if( !readValue( knobnum, v ) )
            v = 0;
        qlinkKnobs[knobnum]->setValue( v, juce::dontSendNotification );
    }

    void KnobGrid::setPadState( bool padState )
    {
        isPadState = padState;
        refreshAll();
    }

    void KnobGrid::setPadIndex( int bankIndex, int padIndex )
    {
        bankNum = bankIndex;
        padNum  = padIndex;
        refreshAll();
    }

    void KnobGrid::setIndex( int index )
    {
        indexNum = index;
        refreshAll();
    }

    int KnobGrid::getState( int knobNum )
    {
        return ( ( knobNum - 1 ) / qlinkTotalColumns ) + 1;
    }

    void KnobGrid::knobCommand( int Knob, int value )
    {
        state = getState( Knob );
        setValues( Knob - 1, value );
        repaint();
    }

    void KnobGrid::setValues( int knobnum, int value )
    {
        if( knobnum < 0 || knobnum >= (int)qlinkKnobs.size() )
            return;

        auto& ae    = mt::getAudioInterface();
        auto* pProg = ae.getProgramDataForTrack( ae.getSelectedSequence(), indexNum );
        auto* program = pProg ? ae.getProgram( pProg->name.get() ) : nullptr;
        if( !program )
            return; // no program on this track: nothing to drive

        const auto title = qlinkKnobs[knobnum]->getTitle();
        if( title == "OFF" )
            return;

        // Switches follow the direction (right = on, left = off); numbers move by the sign-correct, accelerated count.
        const int  count  = knob_logic::acceleratedCount( value );
        const int  source = sources[knobnum];
        const auto type   = program->type.get();

        using knob_logic::Param;
        if( source >= 0 )
        {
            // The insert the knob was assigned from: only that insert's parameter, never a same-named built-in one.
            auto info  = program->getInsertsInfo( source );
            int  index = 0;
            for( const auto& param : info.params )
            {
                if( title == param.name )
                {
                    program->setParameterValue( source, index, (float)knob_logic::stepInsert( program->getParameterValue( source, index ), count ) );
                    break;
                }
                ++index;
            }
        }
        else if( type == mt::MTProgramType::Midi )
        {
            if( parameterSelection )
                parameterSelection( title, count );
        }
        else if( type == mt::MTProgramType::Drum || type == mt::MTProgramType::Clip || type == mt::MTProgramType::Keygroup )
        {
            if( isPadState )
            {
                const int  note = knob_logic::padNote( bankNum, padNum );
                const auto p    = knob_logic::parsePadParam( title.toStdString() );
                switch( p )
                {
                    case Param::PadLevel:
                        program->setNoteGain( note, mt::mixlogic::midiToGain127( (int)knob_logic::stepValue( p, mt::mixlogic::gainToMidi127( (float)program->getNoteGain( note ) ), count ) ) );
                        break;
                    case Param::PadPan: program->setNotePan( note, knob_logic::stepValue( p, program->getNotePan( note ), count ) ); break;
                    case Param::PadSemi: program->setNoteSemi( note, (int)knob_logic::stepValue( p, program->getNoteSemi( note ), count ) ); break;
                    case Param::PadFine: program->setNoteFine( note, (int)knob_logic::stepValue( p, program->getNoteFine( note ), count ) ); break;
                    case Param::PadMute: program->setNoteMute( note, mt::mixlogic::toggleFromDelta( program->isNoteMute( note ), value ) ); break;
                    case Param::PadSolo: program->setNoteSolo( note, mt::mixlogic::toggleFromDelta( program->isNoteSolo( note ), value ) ); break;
                    default: break;
                }
            }
            else
            {
                const auto p = knob_logic::parseProgramParam( title.toStdString() );
                switch( p )
                {
                    case Param::Pan: program->setPan( knob_logic::stepValue( p, program->getPan(), count ) ); break;
                    case Param::Level: program->setLevel( (int)knob_logic::stepValue( p, program->getLevel(), count ) ); break;
                    case Param::Fine: program->setFine( (int)knob_logic::stepValue( p, program->getFine(), count ) ); break;
                    case Param::Semi: program->setSemi( (int)knob_logic::stepValue( p, program->getSemi(), count ) ); break;
                    case Param::Poly: program->setPolyphony( (int)knob_logic::stepValue( p, program->getPolyphony(), count ) ); break;
                    case Param::Mute: program->setMute( mt::mixlogic::toggleFromDelta( program->getMute(), value ) ); break;
                    case Param::Solo: program->setSolo( mt::mixlogic::toggleFromDelta( program->getSolo(), value ) ); break;
                    default: break;
                }
            }
        }

        // Every knob shows the engine value again (same-titled knobs follow, the display never drifts from the engine).
        for( int i = 0; i < (int)qlinkKnobs.size(); ++i )
            refreshKnob( i );
    }
#undef qlinkGridHorSpacing
#undef qlinkGridVerSpacing
#undef qlinkTotalGridKnobs
#undef qlinkTotalRows
#undef qlinkTotalColumns

}
