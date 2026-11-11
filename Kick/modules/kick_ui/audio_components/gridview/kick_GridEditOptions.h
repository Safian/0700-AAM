#pragma once

namespace kick_ui
{
    class APPTrackOptionPitchQuantize : public juce::Component
    {
    public:
        APPTrackOptionPitchQuantize();

        const mt::track::pitchQuantizeInfo& getData()
        {
            info.onlySelected  = selectedEvents.getToggleState();
            info.rootNoteValue = rootNote.getSelectedValue();
            info.scaleValue    = scale.getSelectedValue();
            info.range.start   = startNote.getNoteName();
            info.range.end     = endNote.getNoteName();
            return info;
        };

    private:
        ui::KickLabel          setupLabel{ "Setup" };
        ui::KickListBox        rootNote{ "ROOT NOTE", mt::Note::rootNames(), 0, 3 };
        ui::KickListBox        scale{ "SCALE", mt::ScaleAndNotes::scaleNames, 0, 2 };
        ui::KickLabel          noteInputFilter{ "Note Input Filter" };
        ui::KickCheckBoxButton selectedEvents{ TRANS( "Only apply to selected events" ) };
        ui::KickLabel          noteInputFilter2{ "Output Note Range" };
        ui::KickLabel          noteInputFilter3{ "Quantized noted will be moved up/down by an octave to fit into this note range" };
        ui::KickNoteSelector   startNote{ "START NOTE" };
        ui::KickNoteSelector   endNote{ "END NOTE" };

        mt::track::pitchQuantizeInfo info;

        void paint( juce::Graphics& g ) override;
        void resized() override;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( APPTrackOptionPitchQuantize )
    };

    class APPTrackOptionHumanize : public juce::Component
    {
    public:
        APPTrackOptionHumanize();

        const mt::track::humanizeInfo& getData()
        {
            info.onlySelected        = onlySelectedCheckBox.getToggleState();
            info.isTimeEnabled       = timeCheckBox.getToggleState();
            info.time                = pulses.getValue();
            info.eagerness           = eagerness.getValue();
            info.isNoteLengthEnabled = lengthCheckBox.getToggleState();
            info.noteLength          = strength.getValue();
            info.isVelocityEnabled   = velocityCheckBox.getToggleState();
            info.velocity            = velocity.getValue();
            return info;
        };

    private:
        ui::KickCheckBoxButton onlySelectedCheckBox{ "Only selected notes", "Only selected notes" };
        ui::KickCheckBoxButton timeCheckBox{ "Humanize Time", "Humanize Time" };
        ui::KickSlider         pulses{ "AMOUNT(PULSES)", juce::Slider::LinearBar, juce::Slider::TextBoxAbove };
        ui::KickLabel          beatLabel{ "Exactly a 16th note" };
        ui::KickSlider         eagerness{ "EAGERNESS", juce::Slider::LinearBar, juce::Slider::TextBoxAbove };
        ui::KickLabel          noteLabel{ "Always Embarassingly late" };
        ui::KickCheckBoxButton lengthCheckBox{ "Humanize Note Length", "Humanize Note Length" };
        ui::KickSlider         strength{ "STRENGTH(%)", juce::Slider::LinearBar, juce::Slider::TextBoxAbove };
        ui::KickCheckBoxButton velocityCheckBox{ "Humanize Velocity", "Humanize Velocity" };
        ui::KickSlider         velocity{ "VELOCITY(%)", juce::Slider::LinearBar, juce::Slider::TextBoxAbove };

        mt::track::humanizeInfo info;

        void updateLabels();
        void paint( juce::Graphics& g ) override;
        void resized() override;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( APPTrackOptionHumanize )
    };

    class APPTrackOptionRandomEvents : public juce::Component
    {
    public:
        APPTrackOptionRandomEvents();

        const mt::track::randomEventsInfo& getData()
        {
            info.eventType   = 0 == eventType.getSelectedIndex() ? mt::track::EventType::DrumEvents : mt::track::EventType::MelodicEvents;
            info.replace     = 0 == replace.getSelectedIndex() ? mt::track::RandomEventType::ReplaceAll :
                               1 == replace.getSelectedIndex() ? mt::track::RandomEventType::ReplaceInRange :
                                                                 mt::track::RandomEventType::AppendExisting;
            info.patternSize = patternSize.getValue();
            info.noteLength  = magic_enum::enum_value<mt::MTTimeBase>( noteLength.getSelectedIndex() );

            info.bankValue     = Bank.getSelectedValue();
            info.pads.start    = startPad.getSelectedValue();
            info.pads.end      = endPad.getSelectedValue();
            info.range.start   = startNote.getNoteName();
            info.range.end     = endNote.getNoteName();
            info.rootNoteValue = rootNote.getSelectedValue();
            info.scaleValue    = scale.getSelectedValue();

            info.densityValue         = (int)density.getValue();
            info.rhythmVariationValue = (int)rhythm.getValue();
            return info;
        };

    private:
        std::vector<juce::String> padNames;
        ui::KickListBox           eventType{ "Event Type", mt::Kick::eventType };
        ui::KickListBox           replace{ "Replace", mt::Kick::replaceOptions };
        ui::KickListBox           noteLength{ "Note length", mt::Note::lengthNames, 3, 4 };
        ui::KickSlider            patternSize{ "Bars", juce::Slider::LinearBar, juce::Slider::TextBoxAbove };
        ui::KickLabel             label{ "Drum event options" };
        // Drum Events...
        ui::KickListBox Bank{ "Bank", mt::Kick::padBankNames };
        ui::KickListBox startPad{ "Start Pad", mt::Kick::padNames, 0, 4 };
        ui::KickListBox endPad{ "End Pad", mt::Kick::padNames, 0, 4 };
        // Melodic Events...
        ui::KickListBox      rootNote{ "Root note", mt::Note::rootNames() };
        ui::KickListBox      scale{ "Scale", mt::ScaleAndNotes::scaleNames };
        ui::KickNoteSelector startNote{ "START NOTE" };
        ui::KickNoteSelector endNote{ "END NOTE" };

        ui::KickSliderWithTitle density{ "DENSITY ( % )" }, rhythm{ "RHYTHM VARIATION (%)" };

        mt::track::randomEventsInfo info;

        void paint( juce::Graphics& g ) override;
        void resized() override;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( APPTrackOptionRandomEvents )
    };

    class APPTrackOptionSplitEvents : public juce::Component
    {
    public:
        APPTrackOptionSplitEvents()
        {
            setName( "SPLIT EVENTS" );
            mt::utils::addAndMakeVisible( this, split, selectedEvents );
            split.showTitleAndValue( true, juce::Slider::TextBoxBelow, juce::Justification::centred );
            split.setRange( 2, 32, 1 );
        };

        const mt::track::splitInfo& getData( void )
        {
            info.parts        = (int)split.getValue();
            info.onlySelected = selectedEvents.getToggleState();
            return info;
        }

    private:
        ui::KickSlider         split{ "Split events into" };
        ui::KickCheckBoxButton selectedEvents{ "SelectedEvent", TRANS( "Only selected events" ) };
        mt::track::splitInfo   info;

        void paint( juce::Graphics& g ) override
        {
            g.fillAll( ui::Theme::backgroundColour( 1 ) );
        }

        void resized() override
        {
            split.setBoundsRelative( .05f, .20f, .90f, 0.40f );
            selectedEvents.setBoundsRelative( .05f, .70f, .90f, .15f );
        }
    };

    class GridNotePropertiesDialog : public juce::Component
    {
    public:
        GridNotePropertiesDialog( const grid_logic::NotePropertiesState& initialState,
                                  bool                                   isDrumTrack,
                                  int                                    gridStepTicks,
                                  int                                    maxStartTicks,
                                  std::function<juce::String( int )>     formatStartBbt );

        const grid_logic::NotePropertiesState& getState() const noexcept { return state; }
        bool                                   wasDeleteRequested() const noexcept { return deleteRequested; }

    private:
        struct StepRow
        {
            ui::KickLabel  title;
            ui::KickLabel  value;
            ui::KickButton coarseMinus{ "-Step", "-" };
            ui::KickButton fineMinus{ "-", "-" };
            ui::KickButton finePlus{ "+", "+" };
            ui::KickButton coarsePlus{ "+Step", "+" };
            bool           hasCoarse{ true };

            void attachTo( juce::Component& parent,
                           const juce::String& labelText,
                           const juce::String& coarseMinusText,
                           const juce::String& fineMinusText,
                           const juce::String& finePlusText,
                           const juce::String& coarsePlusText,
                           bool                useCoarse );
            void layout( juce::Rectangle<int> bounds );
        };

        void refreshLabels();
        void paint( juce::Graphics& g ) override;
        void resized() override;

        grid_logic::NotePropertiesState    state;
        bool                               drumTrack{ false };
        int                                stepTicks{ 96 };
        int                                maxStart{ 0 };
        bool                               deleteRequested{ false };
        std::function<juce::String( int )> bbtFormatter;

        StepRow              pitchRow, startRow, lengthRow, velocityRow, probRow, ratchetRow;
        ui::KickToggleButton muteBtn{ "MUTED", "MUTED", "ACTIVE" };
        ui::KickButton       deleteBtn{ "DELETE NOTE", "DELETE NOTE" };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( GridNotePropertiesDialog )
    };

}
