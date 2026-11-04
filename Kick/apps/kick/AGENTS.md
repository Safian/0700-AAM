# Kick App – Agent Instructions

## Mobile App (iOS / iPadOS)
- **Architecture**: Full-screen scaled Kick DAW UI (1024x600 layout scaled to screen aspect ratio).
- **Control Model**: External MIDI controller connectivity (CoreMIDI, USB MIDI / Bluetooth MIDI). Full operational control is handled via connected external MIDI devices.
- **UI Policy**: No hardware-mimicking chassis, simulated front panel, or slide-out/peeking drawer. The mobile port is a pure full-screen mobile app identical to the standalone DAW engine on Akai, designed for tactile control through external MIDI hardware (owner decision 2026-11-04).

## Desktop (macOS)
- Native document window presenting the Kick DAW interface directly at 1024x600 without hardware chassis simulation.
