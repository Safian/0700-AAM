// SPDX-License-Identifier: AGPL-3.0-or-later
// Documents the MPC trap behind "the touch piano does not sound": with an invisible top-level window (no X display) JUCE's
// Component::reallyContains() is false for every point of a visible child. KeyboardComponent::xyToNote must therefore use the
// component's own bounds. (The real MPC window also has a peer; this models only the invisible-top-level half of the trap.) Needs JUCE (juce_gui_basics), no display: components are never put on the desktop.
#include <juce_gui_basics/juce_gui_basics.h>
#include <cstdio>

struct Probe : juce::Component {};

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    Probe top, key;
    top.setBounds( 0, 0, 1024, 600 );   // never made visible: the hidden DocumentWindow of the MPC
    top.addChildComponent( key );
    key.setBounds( 0, 300, 1024, 300 );
    key.setVisible( true );              // a visible child of an invisible top level

    const juce::Point<float> p( 240.0f, 150.0f ); // local to `key`
    bool ok = true;
    // What KeyboardComponent::xyToNote uses now: the component's own bounds.
    if( ! key.getLocalBounds().toFloat().contains( p ) ) { std::fprintf( stderr, "FAIL: local bounds must contain the point\n" ); ok = false; }
    // What it used before (JUCE trap): false for a visible child of an invisible top level, so no key was ever found.
    if( key.reallyContains( p, false ) ) { std::fprintf( stderr, "FAIL: expected reallyContains() == false under an invisible top level (JUCE behaviour changed?)\n" ); ok = false; }
    if( ok ) std::puts( "OK" );
    return ok ? 0 : 1;
}
