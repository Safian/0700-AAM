// SPDX-License-Identifier: AGPL-3.0-or-later

namespace kick_ui
{
    namespace
    {
        class TimeSignatureContent : public juce::Component
        {
        public:
            TimeSignatureContent( const juce::String& Title, int Numerator, int Denominator )
                : numerator( "BEATS PER BAR", 1, 16, Numerator )
            {
                setName( Title );
                mt::utils::addAndMakeVisible( this, numerator, denominator, note );

                const std::vector<juce::String> dens{ "4", "8", "16", "32" };
                int                             sel = 0;
                for( int i = 0; i < (int)dens.size(); ++i )
                    if( dens[(size_t)i].getIntValue() == Denominator ) sel = i;
                denominator.setListOptions( dens, sel );
                denominator.setTitle( "NOTE VALUE" );

                note.setText( "Events keep their position. The sequence keeps its length and is rounded up to whole bars.", juce::dontSendNotification );
                note.setJustificationType( juce::Justification::centred );
                note.setColour( juce::Label::textColourId, ui::Theme::textColour( 2 ) );
            }

            int getNumerator() const { return numerator.getValue(); }
            int getDenominator() { return denominator.getSelectedValue().getIntValue(); }

        private:
            ui::KickNumber  numerator;
            ui::KickListBox denominator{ "NOTE VALUE", 4 };
            juce::Label     note;

            void paint( juce::Graphics& g ) override { g.fillAll( ui::Theme::backgroundColour( 1 ) ); }
            void resized() override
            {
                numerator.setBoundsRelative( .10f, .08f, .30f, .45f );
                denominator.setBoundsRelative( .50f, .08f, .40f, .30f );
                note.setBoundsRelative( .05f, .62f, .90f, .30f );
            }
        };
    }

    juce::String TimeSignatureDialog::describe( int sequence )
    {
        int num = 0, den = 0;
        if( !mt::getAudioInterface().getTimeSignature( sequence, num, den ) )
            return {};
        return juce::String( num ) + "/" + juce::String( den );
    }

    void TimeSignatureDialog::show( int sequence, std::function<void()> onApplied )
    {
        auto& ae  = mt::getAudioInterface();
        int   num = 4, den = 4;
        if( !ae.getTimeSignature( sequence, num, den ) )
            return;
        auto p = std::make_shared<TimeSignatureContent>( "TIME SIGNATURE", num, den );
        mt::utils::showOptions(
            p->getName(), p.get(), [p, sequence, onApplied = std::move( onApplied )]( int R ) mutable {
                if( R == UI::APPLY )
                {
                    const int res = mt::getAudioInterface().setTimeSignature( sequence, p->getNumerator(), p->getDenominator() );
                    if( res == -2 )
                        mt::utils::showNotification( "This sequence has several time signatures; they cannot be edited here." );
                    else if( res != 0 )
                        mt::utils::showNotification( "Invalid time signature." );
                    else if( onApplied )
                        onApplied();
                }
                p.reset();
            } );
    }
}
