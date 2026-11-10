/*
 * Copyright (c) 2021 MUSIC Tribe
 *
 * This file is part of Kick.
 *
 * Kick is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Kick is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Kick. If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#ifndef KICK_APP_VERSION
 #define KICK_APP_VERSION ""
#endif
#ifndef KICK_GIT_HASH
 #define KICK_GIT_HASH ""
#endif

namespace kick_ui
{
    namespace
    {
        juce::String startupDataDir()
        {
#if JUCE_IOS
            const auto envRoot = juce::SystemStats::getEnvironmentVariable( "KICK_DATA_ROOT", {} );
            if( envRoot.isNotEmpty() ) return envRoot;
            return getAppDirs( MTProjects ).getParentDirectory().getFullPathName();
#else
            return juce::SystemStats::getEnvironmentVariable( "KICK_DATA_ROOT", "/data" );
#endif
        }
    }

    StartupScreen::StartupScreen()
    {
        setInterceptsMouseClicks( true, true );
        addAndMakeVisible( logo );
        startMs = juce::Time::currentTimeMillis();

        const auto dataDir   = startupDataDir().toStdString();
        const auto installed = kick_status::readWhole( dataDir + "/Kick.version" );
        buildText            = kick_startup::resolveBuildInfo( installed, KICK_APP_VERSION, KICK_GIT_HASH, __DATE__ ).text();

        snap = kick_startup::state().snapshot();
        startTimerHz( 10 );
    }

    void StartupScreen::resized()
    {
        const auto b = getLocalBounds();
        logo.setBounds( b.getCentreX() - 139, b.getHeight() / 7, 278, 51 );
    }

    void StartupScreen::paint( juce::Graphics& g )
    {
        g.fillAll( ui::Theme::backgroundColour( 0 ).darker( 0.6f ) );

        auto b = getLocalBounds();
        g.setColour( ui::Theme::textColour( 1 ) );
        g.setFont( juce::Font( 18.0f ) );
        g.drawFittedText( buildText, b.withTop( logo.getBottom() + 6 ).withHeight( 28 ), juce::Justification::centred, 1 );

        // Step lines.
        auto lines = b.withTop( logo.getBottom() + 60 ).withSizeKeepingCentre( 560, 4 * 34 );
        g.setFont( juce::Font( 22.0f ) );
        for( int i = 0; i < (int)kick_startup::Step::Count; ++i )
        {
            const auto& info = snap.steps[i];
            switch( info.state )
            {
                case kick_startup::StepState::Failed: g.setColour( juce::Colour( 0xffe5322b ) ); break;
                case kick_startup::StepState::Ok: g.setColour( ui::Theme::textColour( 2 ) ); break;
                case kick_startup::StepState::Running: g.setColour( ui::Theme::textColour( 2 ) ); break;
                default: g.setColour( ui::Theme::textColour( 1 ).withAlpha( 0.6f ) ); break;
            }
            g.drawFittedText( juce::String( kick_startup::stepLine( (kick_startup::Step)i, info ) ), lines.removeFromTop( 34 ), juce::Justification::centredLeft, 1 );
        }

        // Indeterminate progress bar.
        auto bar = b.withTop( lines.getBottom() + 24 ).withSizeKeepingCentre( 560, 8 );
        g.setColour( ui::Theme::backgroundColour( 2 ) );
        g.fillRoundedRectangle( bar.toFloat(), 4.0f );
        const float w = (float)bar.getWidth() * 0.25f;
        const float x = (float)bar.getX() + ( (float)bar.getWidth() - w ) * barPhase;
        g.setColour( ui::Theme::foregroundColour( 1 ) );
        g.fillRoundedRectangle( x, (float)bar.getY(), w, (float)bar.getHeight(), 4.0f );
    }

    void StartupScreen::dismiss( const char* why )
    {
        if( dismissed ) return;
        dismissed = true;
        kick_log( "info", "[INFO] [STARTUP] screen done (%s) after %lld ms\n", why, (long long)( juce::Time::currentTimeMillis() - startMs ) );
        setVisible( false );
        stopTimer();
        startTimerHz( 4 ); // keep polling for problems that arrive later (project load errors, ...)
    }

    void StartupScreen::timerCallback()
    {
        auto& state = kick_startup::state();
        if( !dismissed )
        {
            auto current = state.snapshot();
            barPhase     = std::fmod( barPhase + 0.04f, 2.0f );
            if( current.revision != lastRevision ) { snap = current; lastRevision = current.revision; }
            repaint();
            if( state.isComplete() ) dismiss( "complete" );
            else if( juce::Time::currentTimeMillis() - startMs > kMaxShowMs )
            {
                for( int i = 0; i < (int)kick_startup::Step::Count; ++i )
                {
                    const auto s = state.info( (kick_startup::Step)i );
                    if( s.state == kick_startup::StepState::Pending || s.state == kick_startup::StepState::Running )
                        kick_log( "error", "[ERROR] [STARTUP] step '%s' did not finish within %d ms\n", kick_startup::stepLabel( (kick_startup::Step)i ), kMaxShowMs );
                }
                dismiss( "timeout" );
            }
            return;
        }
        showNextProblem();
    }

    void StartupScreen::showNextProblem()
    {
        if( dialogOpen ) return;
        // Wait for any modal (e.g. the project loading animation / keyboard) so the message is not hidden behind it.
        if( juce::ModalComponentManager::getInstance()->getNumModalComponents() > 0 ) return;
        kick_startup::Problem p;
        if( !kick_startup::state().takeUnshownProblem( p ) ) return;
        dialogOpen = true;
        auto* msg  = new ProblemMessage( p );
        mt::utils::showDialog(
            p.title, msg, [this, msg]( int ) {
                delete msg;
                dialogOpen = false;
            },
            ui::layout::warningPopup, ui::Theme::backgroundColour( 1 ), ui::Theme::titleColour( 1 ), ui::Theme::backgroundColour( 2 ) );
    }

    //////////////////////////////////////////////////////////////////////////////////////////////
    //////                            Status strip
    //////////////////////////////////////////////////////////////////////////////////////////////

    StatusStrip::StatusStrip()
    {
        setInterceptsMouseClicks( false, false );
    }

    void StatusStrip::visibilityChanged()
    {
        if( isVisible() )
        {
            refresh();
            startTimer( 5000 );
        }
        else
            stopTimer();
    }

    void StatusStrip::refresh()
    {
        auto info = kick_status::gatherFsInfo( startupDataDir().toStdString() );
        info.audio = kick_startup::state().info( kick_startup::Step::AudioEngine ).detail;
        if( info.audio == "starting" ) info.audio.clear();
        info.midiDevices = juce::MidiInput::getAvailableDevices().size();
        const auto t     = juce::String( kick_status::formatStrip( info ) );
        if( t != text )
        {
            text = t;
            repaint();
        }
    }

    void StatusStrip::paint( juce::Graphics& g )
    {
        g.setColour( ui::Theme::textColour( 1 ) );
        g.setFont( juce::Font( 15.0f ) );
        g.drawFittedText( text, getLocalBounds().reduced( 10, 0 ), juce::Justification::centredLeft, 1 );
    }
}
