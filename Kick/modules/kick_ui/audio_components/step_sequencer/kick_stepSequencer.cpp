

#define STEP_SEQ_SPACE    16
#define STEP_SEQ_WIDTH    20
#define STEP_BUTTON_SPACE 5

#include "kick_ui/dialogs/kick_AppHome.h"
#include "contracts/dcl/dcl_contracts_helper.h"

namespace kick_ui
{
    namespace sm = step_model;

    StepComponent::StepComponent( StepSequencerData& Data, StepSequencerTopControl& stepControl, StepPresets& stepPreset )
        : data( Data ), control( stepControl ), preset( stepPreset )
    {
        setLookAndFeel( ui::KickLooknFeel::getLnf() );
        preset.sliderOffsetOnDragOn = [&]( int value ) {
            if( ae.getNoteList().getNumChildren() > 0 )
            {
                triggerAsyncUpdate();
            }
        };

        preset.sliderOffsetOnDragEnd = [&]( int value ) {
            if( ae.getNoteList().getNumChildren() > 0 )
            {
                //Update velocites to engine...
                updateValuestoEngine( value );
            }
        };

        preset.modifyValues = [&]( std::vector<int>& Values ) {
            updateValuestoEngine( Values );
        };

        // Flip / Mirror work on whole steps (all parameters travel with the note).
        preset.structuralPreset = [&]( StepPresetType Type ) {
            if( Type == StepPresetType::Flip )
                flipSteps();
            else if( Type == StepPresetType::Mirror )
                mirrorSteps();
        };

        // Play head: only the play position is updated, never the whole step data.
        playheadAsync.fn = [this]() { updatePlayStep(); };
        gridData.gridPlayHeadChanged.connect( [&]() {
            if( active )
                playheadAsync.triggerAsyncUpdate();
        } );

        control.pageChanged = [&]() {
            if( mt::getAudioInterface().isProjectLoaded() )
            {
                contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::setBankMessage( control.getPage() ) );
                triggerAsyncUpdate();
            }
        };
        control.barChanged = [&]() { refresh(); };
        control.padChanged = [&]() { refresh(); };

        mt::signals::playStateChanged.connect( [&]( mt::TransportState State ) {
            state = ( State == mt::TransportState::Playing );
            if( active )
                playheadAsync.triggerAsyncUpdate();
        } );

        // The step button drawables come from disk: load each once, drop them when the theme changes.
        ui::themeChanged.connect( [this]() { svgCache.clear(); } );

        for( auto& i : buttonNum )
            mt::utils::addAndMakeVisible( i.get() );
    }

    void StepComponent::setActive( bool Active )
    {
        active = Active;
        if( !Active )
        {
            gestureOpen = false;
            playStepIndex = -1;
        }
        else
            triggerAsyncUpdate();
    }

    sm::Param StepComponent::currentParam() const
    {
        auto sel = control.paramValueListBox.getSelected();
        if( sel == "Probability" )
            return sm::Param::Probability;
        if( sel == "Ratchet" )
            return sm::Param::Ratchet;
        return sm::Param::Velocity;
    }

    int StepComponent::stepStart( int Index ) const
    {
        return sm::stepStartTick( range.startTime, Index, data.ticksPerQl );
    }

    // The notes of the selected pad that BELONG to a step: they start inside it. A longer note that
    // began in an earlier step only overlaps this one and is left alone.
    void StepComponent::notesOfStep( int Index, std::vector<mt::MTNote>& Out )
    {
        Out.clear();
        if( !sm::stepExists( Index, data.numSteps ) || data.ticksPerQl <= 0 )
            return;
        const auto st = stepStart( Index );
        std::vector<mt::MTNote> found;
        mt::NotesInRange        r( range.noteStart, range.noteStart, st, st + data.ticksPerQl );
        if( ae.getNotesInRange( aeSel, found, r ) < 0 )
            return;
        for( auto& n : found )
            if( sm::noteStartsInStep( n.start.get(), st, data.ticksPerQl ) )
                Out.push_back( n );
    }

    void StepComponent::writeParam( const mt::MTNote& N, sm::Param P, int Value )
    {
        const auto id = N.id.get();
        switch( P )
        {
            case sm::Param::Velocity: ae.editNoteVelocity( aeSel, id, sm::clampParam( P, Value ) ); break;
            case sm::Param::Probability: ae.editNoteProbability( aeSel, id, sm::clampParam( P, Value ) ); break;
            case sm::Param::Ratchet: ae.editNoteRatchet( aeSel, id, sm::clampParam( P, Value ) ); break;
        }
    }

    void StepComponent::createStepNote( int Index, sm::Param P, int Value )
    {
        int velocity = sm::kVelMax, probability = sm::kProbMax, ratchet = sm::kRatMin;
        switch( P )
        {
            case sm::Param::Velocity: velocity = sm::clampParam( P, Value ); break;
            case sm::Param::Probability: probability = sm::clampParam( P, Value ); break;
            case sm::Param::Ratchet: ratchet = sm::clampParam( P, Value ); break;
        }
        ae.addOrModifyNote( range.noteStart, stepStart( Index ), data.ticksPerQl, velocity, probability, ratchet );
    }

    void StepComponent::eraseStepNotes( int Index )
    {
        std::vector<mt::MTNote> found;
        notesOfStep( Index, found );
        for( auto& n : found )
            ae.eraseNote( n.number.get(), n.start.get(), n.length.get() ); // exact match, not the covering note
    }

    void StepComponent::updateValuestoEngine( int Offset )
    {
        if( !isEnabled() ) // audio / clip track or no track: nothing to edit
            return;
        // Offset slider: every active step moves by the offset, kept inside the parameter range.
        mt::ScopedUndoTransaction undoScope( ae.getUndoManager(), "Step offset", !gestureOpen );
        const auto                param = currentParam();
        std::vector<mt::MTNote>   stepNotes;
        for( auto i = 0; i < data.numSteps; ++i )
        {
            notesOfStep( i, stepNotes );
            for( auto& n : stepNotes )
            {
                const auto current = param == sm::Param::Velocity ? (int)n.velocity.get() : ( param == sm::Param::Probability ? (int)n.probability.get() : (int)n.ratchet.get() );
                writeParam( n, param, sm::applyOffset( param, current, Offset ) );
            }
        }
        triggerAsyncUpdate();
    }

    void StepComponent::updateValuestoEngine( std::vector<int>& Values )
    {
        if( !isEnabled() ) // audio / clip track or no track: nothing to edit
            return;
        // Ramp / Triangle / Pulse / Inverse: set the parameter per step. A value that means "off"
        // (Pulse low half) deletes the step's note instead of leaving a silent one.
        mt::ScopedUndoTransaction undoScope( ae.getUndoManager(), "Step preset", !gestureOpen );
        const auto                param = currentParam();
        std::vector<mt::MTNote>   stepNotes;
        for( auto i = 0; i < data.numSteps && i < (int)Values.size(); ++i )
        {
            notesOfStep( i, stepNotes );
            if( stepNotes.empty() )
                continue;
            if( sm::presetValueMeansOff( param, Values[(size_t)i] ) )
            {
                eraseStepNotes( i );
                continue;
            }
            for( auto& n : stepNotes )
                writeParam( n, param, Values[(size_t)i] );
        }
        triggerAsyncUpdate();
    }

    // Mirror: the pattern of the pad is reversed inside the bar; each note moves with ALL its parameters.
    void StepComponent::mirrorSteps()
    {
        if( !isEnabled() ) // audio / clip track or no track: nothing to edit
            return;
        if( data.numSteps <= 1 || data.ticksPerQl <= 0 )
            return;
        mt::ScopedUndoTransaction undoScope( ae.getUndoManager(), "Mirror steps", !gestureOpen );

        struct Moved { int start, length, velocity, probability, ratchet; };
        std::vector<Moved>      moved;
        std::vector<mt::MTNote> stepNotes;
        for( auto i = 0; i < data.numSteps; ++i )
        {
            notesOfStep( i, stepNotes );
            const auto from = stepStart( i );
            const auto to   = stepStart( data.numSteps - 1 - i );
            for( auto& n : stepNotes )
                moved.push_back( { to + ( n.start.get() - from ), n.length.get(), (int)n.velocity.get(), (int)n.probability.get(), (int)n.ratchet.get() } );
        }
        for( auto i = 0; i < data.numSteps; ++i )
            eraseStepNotes( i );
        for( auto& m : moved )
            ae.addOrModifyNote( range.noteStart, m.start, m.length, m.velocity, m.probability, m.ratchet );
        triggerAsyncUpdate();
        if( onNudged )
            onNudged();
    }

    // Flip: active steps become empty and empty steps become active (default values).
    void StepComponent::flipSteps()
    {
        if( !isEnabled() ) // audio / clip track or no track: nothing to edit
            return;
        if( data.numSteps <= 0 || data.ticksPerQl <= 0 )
            return;
        mt::ScopedUndoTransaction undoScope( ae.getUndoManager(), "Flip steps", !gestureOpen );

        std::vector<mt::MTNote> stepNotes;
        std::vector<bool>       wasActive( (size_t)data.numSteps, false );
        for( auto i = 0; i < data.numSteps; ++i )
        {
            notesOfStep( i, stepNotes );
            wasActive[(size_t)i] = !stepNotes.empty();
        }
        for( auto i = 0; i < data.numSteps; ++i )
        {
            if( wasActive[(size_t)i] )
                eraseStepNotes( i );
            else
                createStepNote( i, sm::Param::Velocity, sm::kVelMax );
        }
        triggerAsyncUpdate();
        if( onNudged )
            onNudged();
    }

    void StepComponent::refresh()
    {
        range.noteStart = range.noteEnd = control.padValueListBox.getSelectedIndex();

        auto selBar     = control.getBar();
        range.startTime = gridData.getTicksUptoBar( selBar );
        range.endTime   = gridData.getTicksUptoBar( selBar + 1 );

        // Find out quantization settings.
        data.ticksPerQl = gridData.getTicksPerQuantization( selBar );
        data.timediv    = control.getTimeDivision();
        // Steps available at this quantization: from the real bar length, so any time signature fits.
        data.numSteps = sm::stepsPerBar( gridData.getTicksPerBar( selBar ), data.ticksPerQl, gridData.getNumQlPerBar( selBar ) );
        // Resize if required...
        if( data.selected.size() != (size_t)data.numSteps )
            data.selected.resize( (size_t)data.numSteps, false );
        if( data.indexValues.size() != (size_t)data.numSteps )
            data.indexValues.resize( (size_t)data.numSteps, 0 );
        if( data.indexProbValues.size() != (size_t)data.numSteps )
            data.indexProbValues.resize( (size_t)data.numSteps, 0 );
        if( data.indexRatchetValues.size() != (size_t)data.numSteps )
            data.indexRatchetValues.resize( (size_t)data.numSteps, 0 );

        // Resize canvas accordingly
        updateCanvasSize();
        // Trigger repaint
        triggerAsyncUpdate();
    }

    void StepComponent::handleAsyncUpdate()
    {
        if( !active ) // hidden: nothing to show, nothing to send to the pads
            return;

        std::vector<int> colourID( 16, contracts::DeviceControlContracts::PadColourTrans );
        std::vector<int> velocity( 16, 0 );
        auto             page     = control.getPage();
        auto             idxStart = ( page * data.numStepsPerPage );

        // One engine read for the whole bar, sorted into the steps the notes start in.
        std::vector<std::vector<int>> vel( (size_t)data.numSteps ), prob( (size_t)data.numSteps ), rat( (size_t)data.numSteps );
        if( data.ticksPerQl > 0 )
        {
            notes.clear();
            mt::NotesInRange barRange( range.noteStart, range.noteStart, range.startTime, range.endTime );
            ae.getNotesInRange( aeSel, notes, barRange );
            for( auto& n : notes )
            {
                const auto start = n.start.get();
                if( start < range.startTime || start >= range.endTime )
                    continue; // starts in an earlier bar and only reaches into this one
                const auto step = ( start - range.startTime ) / data.ticksPerQl;
                if( !juce::isPositiveAndBelow( step, data.numSteps ) )
                    continue;
                vel[(size_t)step].push_back( (int)n.velocity.get() );
                prob[(size_t)step].push_back( (int)n.probability.get() );
                rat[(size_t)step].push_back( (int)n.ratchet.get() );
            }
        }

        const auto param = currentParam();
        int        velocityIndex = 0;
        int        colourIndex   = 0;
        for( auto i = 0; i < data.numSteps; ++i )
        {
            const auto idx = (size_t)i;
            data.selected[idx]           = !vel[idx].empty();
            data.indexValues[idx]        = data.selected[idx] ? sm::averageValue( sm::Param::Velocity, vel[idx] ) : 0;
            data.indexProbValues[idx]    = data.selected[idx] ? sm::averageValue( sm::Param::Probability, prob[idx] ) : 0;
            data.indexRatchetValues[idx] = data.selected[idx] ? sm::averageValue( sm::Param::Ratchet, rat[idx] ) : 0;

            if( !isPadSelected && ( i >= idxStart && i < idxStart + 16 ) )
            {
                if( data.selected[idx] )
                {
                    if( param == sm::Param::Velocity )
                    {
                        colourID[(size_t)colourIndex++]   = contracts::DeviceControlContracts::PadColourRed;
                        velocity[(size_t)velocityIndex++] = data.indexValues[idx];
                    }
                    else if( param == sm::Param::Probability )
                    {
                        colourID[(size_t)colourIndex++]   = contracts::DeviceControlContracts::PadColourLightSkyBlue;
                        velocity[(size_t)velocityIndex++] = ( data.indexProbValues[idx] * 127 ) / 100;
                    }
                    else
                    {
                        colourID[(size_t)colourIndex++]   = contracts::DeviceControlContracts::PadColourLightSkyBlue;
                        velocity[(size_t)velocityIndex++] = ( data.indexRatchetValues[idx] * 127 ) / 8;
                    }
                }
                else
                {
                    colourID[(size_t)colourIndex++]   = contracts::DeviceControlContracts::PadColourTrans;
                    velocity[(size_t)velocityIndex++] = 0;
                }
            }
        }

        if( !isPadSelected && ( kick_ui::KickHome::Instance().getCurrentScreen() == kick_ui::Screen::HomeSequencerView ) )
        {
            contracts::DclSignals::sendCommandToDcl(
                contracts::DclContractsHelper::setPadColourByVelocityMessage( contracts::DeviceControlContracts::PadButtonState::StateOn, colourID, velocity ) );
        }
        if( param == sm::Param::Velocity )
            preset.setPresetValues( data.indexValues, sm::kVelMin, sm::kVelMax );
        else if( param == sm::Param::Probability )
            preset.setPresetValues( data.indexProbValues, sm::kProbMin, sm::kProbMax );
        else
            preset.setPresetValues( data.indexRatchetValues, sm::kRatMin, sm::kRatMax );

        playStepIndex = computePlayStep();
        repaint();
        if( onFirstLaunch )
        {
            onFirstLaunch = false;
            contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::
                                                         setPadColorModeMessage( mt::PadColorMode::DISABLE_PAD_COLOUR_MODE ) ); //update the mode only for current screen
        }
    };

    // Playhead tick -> step of the shown bar (-1 when stopped or the playhead is in another bar).
    int StepComponent::computePlayStep()
    {
        if( !ae.isSequencePlaying() || data.numSteps <= 0 || data.ticksPerQl <= 0 )
            return -1;
        const auto bars  = ae.getSequenceLength( aeSel.sequence );
        const auto total = (double)gridData.getTicksUptoBar( (unsigned int)juce::jmax( 0, bars ) );
        const auto tick  = (int)( ae.getPlayhead().normized * total );
        return sm::playStep( tick, range.startTime, data.ticksPerQl, data.numSteps );
    }

    void StepComponent::updatePlayStep()
    {
        if( !active )
            return;
        const auto previous = playStepIndex;
        const auto now      = computePlayStep();
        if( now == previous )
            return;
        playStepIndex = now;

        // Page following: when the playhead crosses into the next page the view goes along. Only the
        // crossing moves the page, so paging away by hand while playing is not undone every step.
        if( now >= 0 && previous >= 0 && data.numStepsPerPage > 0 )
        {
            const auto newPage = sm::pageOfStep( now, data.numStepsPerPage );
            if( newPage != sm::pageOfStep( previous, data.numStepsPerPage ) && newPage < sm::pageCount( data.numSteps, data.numStepsPerPage ) && !isPadSelected )
            {
                control.setSelectedPage( newPage );
                contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::setBankMessage( newPage ) );
                triggerAsyncUpdate();
                if( onPageFollowed )
                    onPageFollowed();
            }
        }
        repaint();
    }

    const juce::Drawable* StepComponent::stepDrawable( bool Active, int Number )
    {
        const auto name = juce::String( Active ? "stepseq/active/button" : "stepseq/default/button" ) + juce::String( Number ) + ".svg";
        auto       it   = svgCache.find( name );
        if( it == svgCache.end() )
            it = svgCache.emplace( name, ui::Theme::getDrawable( name ) ).first;
        return it->second.get();
    }

    void StepComponent::paint( juce::Graphics& g )
    {
        mt::utils::fillRoundedRect( g, getLocalBounds(), ui::Theme::backgroundColour( 1 ), 5.0f );

        // draw Level markings.
        {
            const auto lineMarkings = 6;
            g.setColour( ui::Theme::outlineColour( 1 ).withBrightness( 0.1 ) );

            auto h  = noteArea.getHeight() / lineMarkings;
            int  y  = noteArea.getY();
            auto sx = noteArea.getX();
            auto ex = sx + noteArea.getWidth();
            for( auto i = 0; i <= lineMarkings; ++i )
            {
                g.drawHorizontalLine( y, sx, ex );
                y += h;
            }
        }
        //draw note steps..
        drawSteps( g );
    }

    void StepComponent::drawSteps( juce::Graphics& g )
    {
        auto nArea = noteArea.reduced( STEP_SEQ_SPACE, 0 );
        auto bArea = buttonArea.reduced( STEP_SEQ_SPACE, 0 );

        juce::ColourGradient fillColour = juce::ColourGradient::vertical( juce::Colour( 0xff, 0x2a, 0x00 ), juce::Colour( 0xff, 0xb6, 0x08 ), nArea );

        auto offset    = preset.getOffset();
        auto page      = control.getPage();
        auto idxStart  = ( page * data.numStepsPerPage );
        auto beatNum   = gridData.getNumQlPerBar( control.getBar() ) / gridData.getBeatsPerBar( control.getBar() );
        auto beatValue = ( beatNum == 0 ) ? 1 : beatNum;

        for( auto i = 0; idxStart < data.numSteps && i < data.numStepsPerPage; ++i, ++idxStart )
        {
            auto nA = nArea.removeFromLeft( data.stepWidth ).reduced( STEP_BUTTON_SPACE, 0 );
            auto bA = bArea.removeFromLeft( data.stepWidth ).reduced( STEP_BUTTON_SPACE, 0 );

            // Note Background
            auto colour = ( 0 == ( idxStart % beatValue ) ) ? 3 : 5;
            mt::utils::fillRoundedRect( g, nA, ui::Theme::backgroundColour( colour ) );

            // Note Fill..
            if( control.paramValueListBox.getSelected() == "Velocity" && data.selected[idxStart] )
            {
                if( idxStart < data.indexValues.size() )
                {
                    auto velocity = juce::jlimit( MIDI::MIN, MIDI::MAX, ( data.indexValues[idxStart] + offset ) );
                    auto h        = nA.proportionOfHeight( ( MIDI::MAX - velocity ) / 127.f );
                    auto fA       = nA.withTrimmedTop( h );
                    mt::utils::fillGradientRoundedRect( g, fA, fillColour, 1.0f );
                }
            }
            else if( control.paramValueListBox.getSelected() == "Probability" && data.selected[idxStart] )
            {
                if( idxStart < data.indexProbValues.size() )
                {
                    auto probability = juce::jlimit( 0, 100, ( data.indexProbValues[idxStart] + offset ) );
                    auto h           = nA.proportionOfHeight( ( 100 - probability ) / 100.f );
                    auto fA          = nA.withTrimmedTop( h );
                    mt::utils::fillRoundedRect( g, fA, juce::Colours::skyblue, 1.0f, true, true, false, false );
                }
            }
            else if( control.paramValueListBox.getSelected() == "Ratchet" && data.selected[idxStart] )
            {
                if( idxStart < data.indexRatchetValues.size() )
                {
                    auto ratchet = juce::jlimit( 0, 8, ( data.indexRatchetValues[idxStart] + offset ) );
                    auto h       = nA.proportionOfHeight( ( 8 - ratchet ) / 8.f );
                    auto fA      = nA.withTrimmedTop( h );
                    mt::utils::fillRoundedRect( g, fA, juce::Colours::skyblue, 1.0f, true, true, false, false );
                }
            }

            // Button...
            if( auto* image = stepDrawable( data.selected[idxStart], i + 1 ) )
                image->drawWithin( g, bA.toFloat(), juce::RectanglePlacement::centred, 1.0f );

            // Play position
            if( idxStart == playStepIndex )
            {
                g.setColour( juce::Colours::white.withAlpha( 0.25f ) );
                g.fillRoundedRectangle( nA.getUnion( bA ).toFloat(), 4.0f );
            }
        }
    }

    void StepComponent::setMinimumSize( int W, int H )
    {
        minWidth = W;
        height   = H;

        // calc steps to display on one page.
        data.numStepsPerPage = juce::jlimit( 1, 16, data.timediv );
        data.stepWidth       = juce::jmax( StepSequencerConstants::stepMinWidth, ( ( ( ( W - ( 2 * STEP_SEQ_SPACE ) - STEP_SEQ_WIDTH ) ) / data.numStepsPerPage ) ) );

        // Also update number of pages.
        control.setMaxPages( sm::pageCount( data.numSteps, data.numStepsPerPage ) );

        // Set canvas size.
        width = juce::jmax( minWidth, ( data.stepWidth * data.numSteps ) );
        setSize( width, height );
    }

    void StepComponent::resized()
    {
        noteArea   = getLocalBounds().reduced( StepSequencerConstants::space );
        buttonArea = noteArea.removeFromBottom( StepSequencerConstants::stepButtonHeight );
        noteArea.removeFromBottom( StepSequencerConstants::space );
    }

    int StepComponent::getStepFromX( int X )
    {
        auto bArea = buttonArea.reduced( STEP_SEQ_SPACE, 0 );
        auto idx   = 0;
        for( ; idx < data.numStepsPerPage; ++idx )
        {
            auto bA = bArea.removeFromLeft( data.stepWidth ).reduced( STEP_BUTTON_SPACE, 0 );
            auto sX = bA.getX();
            auto eX = sX + bA.getWidth();
            if( sX <= X && X <= eX )
                break;
        }
        return ( idx >= data.numStepsPerPage ) ? -1 : ( idx + ( data.numStepsPerPage * control.getPage() ) );
    }

    float StepComponent::getNormalizedHeightFromY( int Y )
    {
        auto minY   = noteArea.getY();
        auto spread = noteArea.getHeight();
        auto maxY   = minY + spread;
        return ( spread > 0 ) ? (float)( maxY - Y ) / (float)( spread ) : 0;
    }

    void StepComponent::toggleStep( int Index, int Value )
    {
        if( !isEnabled() ) // audio / clip track or no track: nothing to edit
            return;
        if( !sm::stepExists( Index, data.numSteps ) ) // Safety Check.
            return;

        mt::ScopedUndoTransaction undoScope( ae.getUndoManager(), "Step on/off", !gestureOpen );

        // The engine decides whether the step is on, not the (possibly stale) screen copy.
        std::vector<mt::MTNote> found;
        notesOfStep( Index, found );
        if( found.empty() )
            createStepNote( Index, currentParam(), Value );
        else
            eraseStepNotes( Index ); // only this pad's notes that belong to this step
        triggerAsyncUpdate();
    }

    // Hardware encoder / Q-Link: Index is the position on the page (0..), the step is page relative.
    void StepComponent::knobRotated( int Index, int value )
    {
        const auto step = sm::absoluteStep( control.getPage(), data.numStepsPerPage, Index );
        if( value != 0 )
            adjustStep( step, value );
    }

    // Changes ONLY the shown parameter of a step, relative to its value in the engine. The other
    // parameters of the note stay as they are.
    void StepComponent::adjustStep( int Index, int Delta )
    {
        if( !isEnabled() ) // audio / clip track or no track: nothing to edit
            return;
        if( !sm::stepExists( Index, data.numSteps ) )
            return;

        mt::ScopedUndoTransaction undoScope( ae.getUndoManager(), "Step value", !gestureOpen );

        const auto              param = currentParam();
        std::vector<mt::MTNote> found;
        notesOfStep( Index, found );
        if( found.empty() )
        {
            // Turning on an empty step switches it on with the default value moved by the turn.
            const auto base = param == sm::Param::Velocity ? sm::kVelMax : ( param == sm::Param::Probability ? sm::kProbMax : sm::kRatMin );
            createStepNote( Index, param, base + Delta );
        }
        else
        {
            for( auto& n : found )
            {
                const auto current = param == sm::Param::Velocity ? (int)n.velocity.get() : ( param == sm::Param::Probability ? (int)n.probability.get() : (int)n.ratchet.get() );
                writeParam( n, param, current + Delta );
            }
        }
        triggerAsyncUpdate();
    }

    void StepComponent::updateStepValue( int Index, int Value )
    {
        if( !isEnabled() ) // audio / clip track or no track: nothing to edit
            return;
        if( !sm::stepExists( Index, data.numSteps ) ) // Safety Check.
            return;

        mt::ScopedUndoTransaction undoScope( ae.getUndoManager(), "Step value", !gestureOpen );

        const auto              param = currentParam();
        std::vector<mt::MTNote> found;
        notesOfStep( Index, found );
        if( found.empty() ) // Moving a slider on an empty step sets the note right away, no separate step button press needed.
            createStepNote( Index, param, Value );
        else
            for( auto& n : found )
                writeParam( n, param, Value );
        triggerAsyncUpdate();
    }

    void StepComponent::updateStepValue( int Index, float Normalized )
    {
        switch( currentParam() )
        {
            case sm::Param::Velocity: updateStepValue( Index, (int)( sm::kVelMax * Normalized ) ); break;
            case sm::Param::Probability: updateStepValue( Index, (int)( sm::kProbMax * Normalized ) ); break;
            case sm::Param::Ratchet: updateStepValue( Index, (int)( sm::kRatMax * Normalized ) ); break;
        }
    }

    void StepComponent::nudge( bool direction, int steps )
    {
        if( !isEnabled() )
            return;
        // nudgeNotes() erases notes pushed before tick 0 and lets notes run past the sequence end,
        // so nudge here and wrap the pad's notes around inside the sequence instead.
        const auto total = (int)gridData.getTicksUptoBar( (unsigned int)juce::jmax( 1, ae.getSequenceLength( aeSel.sequence ) ) );
        if( total <= 0 || data.ticksPerQl <= 0 )
            return;

        const auto              pad = range.noteStart;
        mt::NotesInRange        all( pad, pad );
        std::vector<mt::MTNote> found;
        if( ae.getNotesInRange( aeSel, found, all ) < 0 || found.empty() )
            return;

        mt::ScopedUndoTransaction undoScope( ae.getUndoManager(), "Nudge", !gestureOpen );

        struct Moved
        {
            int start, length, velocity, probability, ratchet;
        };
        std::vector<Moved> moved;
        moved.reserve( found.size() );
        for( auto& n : found )
            moved.push_back( { sm::nudgedStart( n.start.get(), direction, steps, data.ticksPerQl, total ), n.length.get(), (int)n.velocity.get(), (int)n.probability.get(), (int)n.ratchet.get() } );

        // Remove first, then re-add, so notes never collide with each other while moving.
        ae.deleteNotesInRange( aeSel, all );
        for( auto& m : moved )
            ae.addOrModifyNote( pad, m.start, m.length, m.velocity, m.probability, m.ratchet );

        triggerAsyncUpdate();
        if( onNudged )
            onNudged();
    }

    void StepComponent::handleMouse( const juce::MouseEvent& event )
    {
        auto index = getStepFromX( event.position.getX() );
        if( noteArea.contains( event.position.toInt() ) )
        {
            updateStepValue( index, getNormalizedHeightFromY( event.position.getY() ) );
        }
        else if( buttonArea.contains( event.position.toInt() ) && ( lastIndex != index ) )
        {
            switch( currentParam() )
            {
                case sm::Param::Velocity: toggleStep( index, sm::kVelMax ); break;
                case sm::Param::Probability: toggleStep( index, sm::kProbMax ); break;
                case sm::Param::Ratchet: toggleStep( index, sm::kRatMin ); break;
            }
            lastIndex = index;
        }
    }

    void StepComponent::mouseDown( const juce::MouseEvent& )
    {
        // A touch gesture (tap or a whole drag-paint) is ONE undo step.
        lastIndex = -1;
        ae.getUndoManager().beginNewTransaction( "Step edit" );
        gestureOpen = true;
    }

    void StepComponent::mouseUp( const juce::MouseEvent& event )
    {
        if( !event.mouseWasDraggedSinceMouseDown() )
        {
            lastIndex = -1;
            handleMouse( event );
        }
        lastIndex   = -1; // the next gesture may start on the same step again
        gestureOpen = false;
    }

    void StepComponent::mouseDrag( const juce::MouseEvent& event )
    {
        handleMouse( event );
    }

    /////////////////////////////////////////////////////////////////
    ///
    /// Class: StepSequencer
    ///
    /////////////////////////////////////////////////////////////////

    StepSequencer::StepSequencer()
        : kick_ui::IScreen( Screen::HomeSequencerView )
    {
        setLookAndFeel( ui::KickLooknFeel::getLnf() );

        //header
        header.onBackButtonPressed = [this]( bool ) { closeScreen( Screen::HomeListView ); };

        // Setup step sequencer...
        mt::utils::addAndMakeVisible( this, stepControl, stepPreset, stepViewport, header, topControl, tracks, totalBars, padName, bpmTempo, warningLabel, drumGrid, viewModeButton );
        mt::utils::setTitleProportion( .4f, totalBars, bpmTempo );
        mt::utils::setTitleColour( ui::Theme::textColour( 1 ), totalBars, bpmTempo );
        mt::utils::setTitleFontHeight( 16, totalBars, bpmTempo, tracks );

        warningLabel.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        warningLabel.setValueFontHeight( 25 );

        stepViewport.setViewedComponent( &stepComponent, false );
        stepViewport.setScrollBarPosition( false, true );

        padName.setColours( ui::Theme::textColour( 1 ), ui::Theme::backgroundColour( 9 ) );
        padName.setFontHeight( 16 );
        padName.setJustificationType( juce::Justification::centred );
        padName.setEditable( false );
        padName.setInterceptsMouseClicks( false, false );

        tracks.setTitleColour( ui::Theme::titleColour( 2 ) );
        tracks.setJustificationType( juce::Justification::centredLeft );

        // Handle control changes...
        stepControl.leftNudgeButton.onClick = [&]() {
            stepComponent.nudge( false );
            stepControl.leftNudgeButton.setToggleState( false, juce::dontSendNotification );
        };

        stepControl.rightNudgeButton.onClick = [&]() {
            stepComponent.nudge( true );
            stepControl.rightNudgeButton.setToggleState( false, juce::dontSendNotification );
        };

        stepControl.clearpad.onClick = [&]() {
            clearPadData();
            stepControl.clearpad.setToggleState( false, juce::dontSendNotification );
        };

        stepControl.clearbar.onClick = [&]() {
            clearBarData();
            stepControl.clearbar.setToggleState( false, juce::dontSendNotification );
        };

        stepControl.padMinus.onClick = [&]() {
            stepControl.padMinus.setToggleState( false, juce::dontSendNotification );
            movePad( -1 );
        };

        stepControl.padPlus.onClick = [&]() {
            stepControl.padPlus.setToggleState( false, juce::dontSendNotification );
            movePad( 1 );
        };

        stepControl.barPlus.onClick = [&]() {
            stepControl.barPlus.setToggleState( false, juce::dontSendNotification );
            moveBar( 1 );
        };

        stepControl.barMinus.onClick = [&]() {
            stepControl.barMinus.setToggleState( false, juce::dontSendNotification );
            moveBar( -1 );
        };

        stepControl.padSelect.onClick = [&]() {
            isPadSelect = stepControl.padSelect.getToggleState();
            if( isPadSelect )
            {
                contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::setBankMessage( topControl.padValueListBox.getSelectedIndex() / 16 ) );
                topControl.setPageEnable( false );

                contracts::DclSignals::sendCommandToDcl(
                    contracts::DclContractsHelper::setAllPadColourByVelocityMessage(
                        contracts::DeviceControlContracts::PadButtonState::StateOn,
                        contracts::DeviceControlContracts::PadColourOrange ) );

                contracts::DclSignals::sendCommandToDcl(
                    contracts::DclContractsHelper::setPadColourByVelocityMessage(
                        topControl.padValueListBox.getSelectedIndex() % 16,
                        contracts::DeviceControlContracts::StateOn,
                        contracts::DeviceControlContracts::padColourlightGreen1 ) );
            }
            else
            {
                topControl.setPageEnable( true );
                stepControl.padSelect.setToggleState( false, juce::dontSendNotification );
                stepComponent.refresh();
            }
            stepComponent.setPadSelect( isPadSelect );
        };

        totalBars.onValueChanged = [&]( int V ) {
            mt::ScopedUndoTransaction undoScope( ae.getUndoManager(), "Sequence length" );
            ae.setSequenceLength( aeSel.sequence, V );
            refresh();
        };

        bpmTempo.onValueChanged = [&]( int Value ) {
            ae.setSequenceBPM( aeSel.sequence, Value );
        };

        gridData.onNoteSelectionChanged.connect( [this]( int SelectedNote ) {
            topControl.padValueListBox.setSelected( SelectedNote, juce::dontSendNotification );
            stepComponent.refresh();
            drumGrid.refresh();
            updatePadName();
        } );

        topControl.padValueListBox.onSelectionChanged = [&]( int Index, const juce::String& ) {
            gridData.onNoteSelectionChanged( Index );
        };

        topControl.timeDivValueListBox.onSelectionChanged = [&]( int, const juce::String& Value ) {
            auto tempoMgr = ae.getTempoMgr( aeSel.sequence );
            if( tempoMgr )
                tempoMgr->setQuantization( gridData.getTimeBase( Value ) );
            stepComponent.refresh();
        };

        topControl.paramValueListBox.onSelectionChanged = [&]( int Index, const juce::String& Param ) {
            stepComponent.refresh();
            if( drumGrid.getLayer() != DrumGrid::Layer::Notes )
                drumGrid.setLayer( (DrumGrid::Layer)( Index + 1 ) );
        };

        // Drum grid: keep it in sync with the shared top controls (pad, bar, page, parameter).
        for( auto* cb : { &topControl.pageChanged, &topControl.barChanged, &topControl.padChanged } )
        {
            auto previous = *cb;
            *cb           = [this, previous]() {
                if( previous )
                    previous();
                drumGrid.refresh();
            };
        }

        drumGrid.onLayerChanged = [this]( DrumGrid::Layer L ) {
            if( L != DrumGrid::Layer::Notes && topControl.paramValueListBox.getSelectedIndex() != (int)L - 1 )
            {
                topControl.paramValueListBox.setSelected( (int)L - 1, juce::dontSendNotification );
                stepComponent.refresh();
            }
        };

        stepComponent.onNudged       = [this]() { drumGrid.refresh(); };
        stepComponent.onPageFollowed = [this]() { drumGrid.refresh(); };

        // Undo/redo, recording and edits from other screens show up at once (no polling).
        noteWatcher.onChanged = [this]() { externalNotesChanged(); };

        viewModeButton.setToggleState( false, juce::dontSendNotification ); // CLASSIC by default
        viewModeButton.setToggleButtonText( "GRID", "CLASSIC" );
        viewModeButton.onClick = [this]() { applyViewMode(); };

        tracks.onSelectionChanged = [&]( int Sel, const juce::String& ) {
            ae.setSelectedTrack( Sel );
            refresh();
            updateAvailability();
            noteWatcher.rebind();
        };

        ui::themeChanged.connect( [&]() {
            header.setIcon( "Icon_Stepsequencer" );
        } );
    }
    int StepSequencer::lastPressedPad = -1;

    void StepSequencer::resized()
    {
        header.setBoundsRelative( .0f, .0f, 1.0f, .1423f );
        tracks.setBoundsRelative( .230, .0299f, .1956f, .0823f );
        padName.setBoundsRelative( .4450f, .0299f, .1900f, .0823f );
        bpmTempo.setBoundsRelative( .7612f, .0299f, .0724f, .0823f );
        totalBars.setBoundsRelative( .6692f, .0299f, .0724f, .0823f );
        stepControl.setBoundsRelative( .0f, .9213f, 1.0f, .1011f );
        topControl.setBoundsRelative( .0100f, .1610f, .9814f, .0823f );
        stepViewport.setBoundsRelative( .010f, .2621f, .8775f, .6404f );
        stepPreset.setBoundsRelative( .8943f, .2621f, .0978f, .6404f );
        drumGrid.setBoundsRelative( .010f, .2621f, .9814f, .6404f );
        viewModeButton.setBoundsRelative( .8450f, .0299f, .1050f, .0823f ); // ends before the header's resources button at .96
        warningLabel.setBounds( getLocalBounds().removeFromTop( 300 ).withTrimmedTop( 200 ).removeFromLeft( 900 ).withTrimmedLeft( 300 ) );

        stepComponent.setMinimumSize( stepViewport.getWidth(), stepViewport.getHeight() );
    }

    void StepSequencer::handleCommand( CPktMgr::HWMsg_t& f_HwMsg )
    {
        if( contracts::DclContractsHelper::isButtonMessage( f_HwMsg ) && ( contracts::DclContractsHelper::getButtonState( f_HwMsg ) == contracts::DeviceControlContracts::BTN_PRESSED ) )
        {
            auto buttonCode = contracts::DclContractsHelper::getButtonCode( f_HwMsg );
            switch( buttonCode )
            {
                case contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_A:
                case contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_B:
                case contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_C:
                case contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_D:
                case contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_E:
                case contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_F:
                case contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_G:
                case contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_H:
                    if( isPadSelect )
                    {
                        contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::setBankMessage( buttonCode ) );
                        contracts::DclSignals::sendCommandToDcl(
                            contracts::DclContractsHelper::setAllPadColourByVelocityMessage(
                                contracts::DeviceControlContracts::PadButtonState::StateOn,
                                contracts::DeviceControlContracts::PadColourOrange ) );

                        auto offset = ae.getBankOffset();
                        if( topControl.padValueListBox.getSelectedIndex() >= ae.getBankOffset() && topControl.padValueListBox.getSelectedIndex() <= ae.getBankOffset() + 16 )
                        {
                            contracts::DclSignals::sendCommandToDcl(
                                contracts::DclContractsHelper::setPadColourByVelocityMessage(
                                    topControl.padValueListBox.getSelectedIndex() % 16, contracts::DeviceControlContracts::PadButtonState::StateOn, contracts::DeviceControlContracts::padColourlightGreen1 ) );

                            //Controls::InputEvents::setPadColourByVelocity( topControl.padValueListBox.getSelectedIndex() % 16, Controls::PadButtonState::StateOn, Controls::padColourlightGreen1 );
                            lastPressedPad = topControl.padValueListBox.getSelectedIndex() % 16;
                        }
                        else
                            lastPressedPad = -1;
                    }
                    else
                    {
                        // Pages are rounded up (16T = 24 steps = 2 pages); a button past the last page does nothing.
                        if( step_model::bankButtonSelectsPage( buttonCode, data.numSteps, data.numStepsPerPage ) )
                        {
                            contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::setBankMessage( buttonCode ) );
                            stepComponent.setPage( buttonCode );
                            stepComponent.asyncUpdate();
                        }
                    }
                default: break;
            }
            switch( buttonCode )
            {
                case contracts::DeviceControlContracts::EN_PLAY: ae.setSequencePlay( aeSel.sequence, false, true ); break;
                case contracts::DeviceControlContracts::EN_PLAYSTART: ae.setSequencePlay( aeSel.sequence, true, true ); break;
                case contracts::DeviceControlContracts::EN_STOP: ae.setSequencePause(); break;
            }
        }
        else if( contracts::DclContractsHelper::isPadButtonMessage( f_HwMsg ) )
        {
            int padNum      = contracts::DclContractsHelper::getPadNumber( f_HwMsg );
            int padState    = contracts::DclContractsHelper::getPadState( f_HwMsg );
            int padVelocity = contracts::DclContractsHelper::getPadVelocity( f_HwMsg );

            if( isPadSelect )
            {
                if( lastPressedPad < 0 )
                {
                    lastPressedPad = padNum;

                    contracts::DclSignals::sendCommandToDcl(
                        contracts::DclContractsHelper::setPadColourByVelocityMessage(
                            padNum, contracts::DeviceControlContracts::PadButtonState::StateOn, contracts::DeviceControlContracts::padColourlightGreen1 ) );

                    lastPressedPad = padNum;
                    topControl.padValueListBox.setSelected( padNum + ae.getBankOffset() );
                }
                else if( lastPressedPad != padNum )
                {
                    contracts::DclSignals::sendCommandToDcl(
                        contracts::DclContractsHelper::setPadColourByVelocityMessage(
                            lastPressedPad, contracts::DeviceControlContracts::PadButtonState::StateOn, contracts::DeviceControlContracts::PadColourOrange ) );

                    contracts::DclSignals::sendCommandToDcl(
                        contracts::DclContractsHelper::setPadColourByVelocityMessage(
                            topControl.padValueListBox.getSelectedIndex() % 16, contracts::DeviceControlContracts::PadButtonState::StateOn, contracts::DeviceControlContracts::padColourlightGreen1 ) );

                    lastPressedPad = padNum;
                    topControl.padValueListBox.setSelected( padNum + ae.getBankOffset() );
                }
            }
            else if( padState == contracts::DeviceControlContracts::BTN_PRESSED )
            {
                // The pad is the step at its position on the page, in every parameter view. Half (user) level
                // takes the pad's own velocity for the step, otherwise the value is fixed.
                const auto padStep = step_model::absoluteStep( stepComponent.getPage(), data.numStepsPerPage, padNum );
                const auto param   = topControl.paramValueListBox.getSelected();
                if( param == "Velocity" )
                {
                    if( contracts::DclContractsHelper::isHalfLevel() )
                        stepComponent.toggleStep( padStep, padVelocity );
                    else
                        stepComponent.toggleStep( padStep );
                }
                else if( param == "Probability" )
                    stepComponent.toggleStep( padStep, 100 );
                else if( param == "Ratchet" )
                    stepComponent.toggleStep( padStep, 1 );
            }
        }
        else if( contracts::DclContractsHelper::isKnobMessage( f_HwMsg ) )
        {
            auto knob         = contracts::DclContractsHelper::getKnobCode( f_HwMsg );
            auto dir          = contracts::DclContractsHelper::getKnobDirection( f_HwMsg );
            auto value        = contracts::DclContractsHelper::getKnobValue( f_HwMsg );

            const bool forward = value != 0 ? value > 0 : dir == contracts::DeviceControlContracts::EN_KNOB_CLOCKWISE;
            const auto steps   = step_model::turnSteps( value ); // a fast turn delivers several counts

            // In the grid the dial changes the selected notes' values; with nothing selected it nudges.
            if( knob == contracts::DeviceControlContracts::KnobSequence::EN_KNOB_CENTER && drumGrid.isVisible() )
            {
                if( !drumGrid.adjustSelected( value ) && value != 0 )
                    stepComponent.nudge( forward, steps );
                return;
            }

            if( knob != contracts::DeviceControlContracts::KnobSequence::EN_KNOB_CENTER )
                stepComponent.knobRotated( knob - 1, value );
            else
                stepComponent.nudge( forward, steps );
        }
        else if( contracts::DclContractsHelper::isButtonMessage( f_HwMsg ) )
        {
            int BtnNum = contracts::DclContractsHelper::getButtonCode( f_HwMsg );
            switch( BtnNum )
            {
                case contracts::DeviceControlContracts::BtnLedSequence::EN_PLUS:
                case contracts::DeviceControlContracts::BtnLedSequence::EN_KNOB_CW:
                    if( !drumGrid.adjustSelected( 1 ) )
                        stepComponent.nudge( TRUE );
                    break;
                case contracts::DeviceControlContracts::BtnLedSequence::EN_KNOB_CCW:
                case contracts::DeviceControlContracts::BtnLedSequence::EN_MINUS:
                    if( !drumGrid.adjustSelected( -1 ) )
                        stepComponent.nudge( FALSE );
                    break;
            }
        }
    }

    void StepSequencer::visibilityChanged()
    {
        stepComponent.setOnFirstLaunch( isVisible() );

        for( auto it : getChildren() )
            it->setVisible( isVisible() );

        noteWatcher.setEnabled( false );
        stepComponent.setActive( false );

        if( isVisible() )
        {
            updateAvailability();
            if( stepComponent.isEnabled() )
            {
                lastPressedPad = topControl.padValueListBox.getSelectedIndex() % 16;
                tracks.setListOptions( ae.getTrackList( aeSel.sequence ), aeSel.track );

                // Show the sequence's quantization, never write it back: opening the screen must not change
                // the quantization (1/32T used to turn into 1/16, 1/2 and 1/2T were overwritten too).
                auto tempoMgr = ae.getTempoMgr( aeSel.sequence );
                auto timeDiv  = tempoMgr ? tempoMgr->getQuantization( aeSel.sequence ) : magic_enum::enum_integer<mt::MTTimeBase>( mt::defaultQuantize );
                topControl.timeDivValueListBox.setSelected( step_model::timeDivIndex( timeDiv ), juce::dontSendNotification );
                setTempo();
                stepControl.padSelect.setToggleState( false, juce::dontSendNotification );
                //Updating PadSelectState
                stepComponent.setPadSelect( false );
                isPadSelect = false;
                topControl.setPageEnable( true );
                warningLabel.setVisible( false );
                applyViewMode();

                stepComponent.setActive( true );
                noteWatcher.setEnabled( true );
                // After the (asynchronous) list selection above has been applied.
                juce::MessageManager::callAsync( [this]() {
                    if( isVisible() )
                    {
                        stepComponent.refresh();
                        drumGrid.refresh();
                        updatePadName();
                    }
                } );
            }
        }
        else
        {
            auto MTIProg = ae.getProgramData( ae.getSelectedProgram() );

            contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::
                                                         setPadColorModeMessage( MTIProg ? MTIProg->colourMode.get() : mt::OFF ) );

            contracts::DclSignals::refreshPadLeds();
        }
    }

    // No track, or an Audio / Clip track (they have no note steps): the editing controls are off and a
    // notice says why.
    void StepSequencer::updateAvailability()
    {
        juce::String why;
        if( !ae.getNumTracks( ae.getSelectedSequence() ) )
            why = "Create any Track Before using StepSequencer";
        else if( auto* prog = ae.getProgramDataForTrack( aeSel.sequence, aeSel.track ) )
        {
            const auto type = prog->type.get();
            if( type == mt::MTProgramType::Audio || type == mt::MTProgramType::Clip )
                why = "Audio and Clip tracks have no steps. Select a Drum, Keygroup or MIDI track";
        }

        const bool usable = why.isEmpty();
        stepComponent.setEnabled( usable );
        if( isVisible() )
        {
            for( auto it : getChildren() )
                it->setVisible( usable );
            header.setVisible( true );
            if( !usable )
            {
                tracks.setVisible( ae.getNumTracks( ae.getSelectedSequence() ) > 0 ); // pick another track
                warningLabel.setText( why, juce::dontSendNotification );
            }
            warningLabel.setVisible( !usable );
            if( usable )
                applyViewMode();
        }
        if( !usable )
        {
            noteWatcher.setEnabled( false );
            stepComponent.setActive( false );
        }
        else if( isVisible() )
        {
            stepComponent.setActive( true );
            noteWatcher.setEnabled( true );
        }
    }

    // The shown track's notes changed (undo/redo, recording, another screen): re-read what is shown.
    void StepSequencer::externalNotesChanged()
    {
        if( !isVisible() || !stepComponent.isEnabled() )
            return;
        stepComponent.asyncUpdate();
        drumGrid.externalChange();
    }

    void StepSequencer::updatePadName()
    {
        juce::String name;
        const auto   prog = ae.isProjectLoaded() ? ae.getSelectedProgram() : -1;
        const auto   pad  = topControl.padValueListBox.getSelectedIndex();
        if( prog >= 0 && pad >= 0 )
        {
            std::vector<mt::MTISample*> samples;
            ae.getProgramSamples( prog, samples );
            for( auto* smp : samples )
                if( smp != nullptr && (int)smp->keyNote.get() == pad )
                {
                    auto src = smp->getSource();
                    if( src.isNotEmpty() )
                    {
                        name = juce::File( src ).getFileNameWithoutExtension();
                        break;
                    }
                }
        }
        padName.setText( name, juce::dontSendNotification );
    }

    // Pad / bar +- : the target index is computed here, not read back from the list box (its selection
    // is applied asynchronously and used to lag one step behind).
    void StepSequencer::movePad( int Delta )
    {
        const auto count  = topControl.padValueListBox.getNumOptions();
        const auto target = juce::jlimit( 0, juce::jmax( 0, count - 1 ), topControl.padValueListBox.getSelectedIndex() + Delta );
        topControl.padValueListBox.setSelected( target );
        if( isPadSelect )
        {
            contracts::DclSignals::sendCommandToDcl(
                contracts::DclContractsHelper::setPadColourByVelocityMessage(
                    lastPressedPad, contracts::DeviceControlContracts::PadButtonState::StateOn, contracts::DeviceControlContracts::PadColourOrange ) );
            contracts::DclSignals::sendCommandToDcl(
                contracts::DclContractsHelper::setPadColourByVelocityMessage(
                    target % 16, contracts::DeviceControlContracts::PadButtonState::StateOn, contracts::DeviceControlContracts::padColourlightGreen1 ) );
            lastPressedPad = target % 16;
        }
    }

    void StepSequencer::moveBar( int Delta )
    {
        const auto count  = topControl.barValueListBox.getNumOptions();
        const auto target = juce::jlimit( 0, juce::jmax( 0, count - 1 ), topControl.barValueListBox.getSelectedIndex() + Delta );
        topControl.barValueListBox.setSelected( target );
        if( isPadSelect )
        {
            contracts::DclSignals::sendCommandToDcl(
                contracts::DclContractsHelper::setPadColourByVelocityMessage(
                    lastPressedPad, contracts::DeviceControlContracts::PadButtonState::StateOn, contracts::DeviceControlContracts::PadColourOrange ) );
            contracts::DclSignals::sendCommandToDcl(
                contracts::DclContractsHelper::setPadColourByVelocityMessage(
                    topControl.padValueListBox.getSelectedIndex() % 16, contracts::DeviceControlContracts::PadButtonState::StateOn, contracts::DeviceControlContracts::padColourlightGreen1 ) );
        }
    }

    void StepSequencer::refresh()
    {
        auto length = ae.getSequenceLength( aeSel.sequence );
        totalBars.setValue( length, juce::dontSendNotification );
        topControl.setTotalBars( length );
        stepComponent.refresh();
        drumGrid.refresh();
    }

    void StepSequencer::showGridView( bool Grid )
    {
        viewModeButton.setToggleState( Grid, juce::dontSendNotification );
        applyViewMode();
    }

    void StepSequencer::applyViewMode()
    {
        const auto grid = viewModeButton.getToggleState();
        drumGrid.setVisible( grid );
        stepViewport.setVisible( !grid );
        stepPreset.setVisible( !grid );
    }

    // Erase exactly the notes of a pad that START in [FromTick, ToTick). A longer note that began earlier
    // and only reaches into the range belongs to an earlier step and stays.
    int StepSequencer::eraseNotesStartingIn( int Pad, int FromTick, int ToTick )
    {
        std::vector<mt::MTNote> found;
        mt::NotesInRange        r( Pad, Pad, FromTick, ToTick );
        if( ae.getNotesInRange( aeSel, found, r ) < 0 )
            return 0;
        int erased = 0;
        for( auto& n : found )
        {
            const auto start = n.start.get();
            if( start >= FromTick && start < ToTick )
            {
                ae.eraseNote( (int)n.number.get(), start, n.length.get() );
                ++erased;
            }
        }
        return erased;
    }

    void StepSequencer::clearPadData()
    {
        mt::ScopedUndoTransaction undoScope( ae.getUndoManager(), "Clear pad" );
        const auto                pad   = topControl.padValueListBox.getSelectedIndex();
        const auto                total = (int)gridData.getTicksUptoBar( (unsigned int)juce::jmax( 1, ae.getSequenceLength( aeSel.sequence ) ) );
        eraseNotesStartingIn( pad, 0, total );
        stepComponent.refresh();
        drumGrid.refresh();
    }

    void StepSequencer::clearBarData()
    {
        mt::ScopedUndoTransaction undoScope( ae.getUndoManager(), "Clear bar" );
        const auto                pad = topControl.padValueListBox.getSelectedIndex();
        const auto                bar = juce::jmax( 0, topControl.barValueListBox.getSelectedIndex() );
        eraseNotesStartingIn( pad, (int)gridData.getTicksUptoBar( (unsigned int)bar ), (int)gridData.getTicksUptoBar( (unsigned int)bar + 1 ) );
        stepComponent.refresh();
        drumGrid.refresh();
    }

    void StepSequencer::setTempo()
    {
        auto pSeq = ae.getSequenceData( aeSel.sequence );
        bpmTempo.setValue( pSeq ? pSeq->tempo.get() : mt::getSettings().projects.tempo(), juce::dontSendNotification );
    }

    void StepSequencer::handleAsyncUpdate()
    {
        repaint();
    }

    void StepSequencer::mouseDrag( const juce::MouseEvent& event )
    {
    }

    void StepSequencer::mouseUp( const juce::MouseEvent& )
    {
    }

    void StepSequencer::mouseDown( const juce::MouseEvent& event )
    {
    }
} //kick_ui
