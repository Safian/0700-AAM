// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "kick_UndoWatcher.h"

namespace kick_ui
{
    // The Undo History window: the steps that can be undone, newest first; choosing a row and pressing Apply goes back that far
    // (every newer step is undone too). Opened from the Main screen toolbar. Not while a take is being recorded.
    class UndoHistory : public juce::Component
    {
    public:
        static void show()
        {
            auto& ae = mt::getAudioInterface();
            if( ae.isSequenceRecording() )
            {
                mt::utils::showNotification( "Undo History is off while recording" );
                return;
            }
            auto content = std::make_unique<UndoHistory>();
            if( content->rows == 0 )
            {
                mt::utils::showNotification( "Nothing to undo" );
                return;
            }
            const auto bounds = content->getBounds();
            mt::utils::showDialogOwned<UndoHistory>(
                "UNDO HISTORY", std::move( content ),
                []( int R, UndoHistory& h ) {
                    if( R != UI::APPLY )
                        return;
                    const int steps = undo_watch::stepsToUndo( h.list.getSelectedIndex(), h.rows );
                    if( steps <= 0 || mt::getAudioInterface().isSequenceRecording() )
                        return;
                    for( int i = 0; i < steps; ++i )
                        mt::getAudioInterface().performUndo();
                    mt::utils::showNotification( steps == 1 ? juce::String( "Undid 1 step" ) : "Undid " + juce::String( steps ) + " steps" );
                    UndoWatcher::undoRedoDone(); // the screens refresh
                },
                bounds, true );
        }

        UndoHistory()
        {
            setSize( 650, 520 );
            const auto names = mt::getAudioInterface().getUndoManager().getUndoDescriptions();
            rows             = names.size();
            std::vector<juce::String> options;
            for( int i = 0; i < rows; ++i )
                options.push_back( juce::String( undo_watch::rowText( i, names[i].toStdString() ) ) );
            list.setListOptions( options, 0, juce::dontSendNotification );
            list.setTitle( "STEPS (newest first)" );
            list.onSelectionChanged = [this]( int, const juce::String& ) { updateSummary(); };
            summary.setJustificationType( juce::Justification::centred );
            summary.setColour( juce::Label::textColourId, ui::Theme::textColour( 2 ) );
            mt::utils::addAndMakeVisible( this, list, summary );
            updateSummary();
        }

    private:
        int             rows{ 0 };
        ui::KickListBox list{ "STEPS", 1 };
        juce::Label     summary;

        void updateSummary() { summary.setText( juce::String( undo_watch::historySummary( undo_watch::stepsToUndo( list.getSelectedIndex(), rows ) ) ), juce::dontSendNotification ); }
        void paint( juce::Graphics& g ) override { g.fillAll( ui::Theme::backgroundColour( 1 ) ); }
        void resized() override
        {
            list.setBoundsRelative( .08f, .05f, .84f, .78f );
            summary.setBoundsRelative( .05f, .86f, .90f, .08f );
        }
    };
}
