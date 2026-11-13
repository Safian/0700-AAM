> **ARCHIVÁLT, NEM FEJLESZTENDŐ, scope-on kívül (2026-11-13).** Az Akai MPC támogatás megszűnt (owner: "a kick-es akai fejlesztését el tudjuk engedni mostantól, az akaira fejlesztünk csak (a SynthyBoy-t)! max mobilra fogjuk folytatni"). Történeti hivatkozás: ne fejleszts rá, ne használd kiindulásnak. Az itt említett kódútvonalak/linkek elavultak lehetnek.

# Key 61 USB: Controller Mode, tablet and phone use (research, 2026-11-03)

Question: can a phone or tablet use the MPC Key 61 as a MIDI controller (the idea: Kick on a phone + the Key 61 hardware, without the stock MPC UI)?
Each claim carries its source and how reliable it is. Nothing here is measured on our hardware yet, except where it says so.

## Official manual (MPC Standalone OS User Guide v3.10, `manuals/`)
- Controller Mode: the mode icon in the status bar switches the MPC to Controller Mode; it is meant for host software on a computer ("In Controller Mode: in your host software, make sure your MPC hardware is selected as a MIDI controller device"). Switching closes the current project.
- Preferences > "USB Type-C Data Role" (**MPC One G2 and MPC Key 37 G2 only**, not the Key 61): "Act as device" forwards the standalone MIDI ports and the audio device to the connected device (the manual names a smartphone as a dual-role device example); "Act as host" lets the MPC host the device's MIDI ports, storage or audio. "USB Type-C Power Role" can charge the connected device.
- The manual does not describe phone or tablet use for the Key 61.

## Community reports (mpc-forums.com and others; user reports, not official)
- Key 61, 2022: an iPad connected through a camera adapter did not work with the Key 61's USB; a moderator said triggering an iPad from an MPC track needs Bluetooth MIDI or a 5-pin cable through a MIDI interface.
- The Key 61 behaves either as a controller or as a standalone unit; in Standalone Mode it does not send MIDI over USB (user report). In Controller Mode the unit restarts into a "dumb" controller mode.
- Key 37 (not the 61), 2024: connecting the MPC's USB-B to an iPad's USB-C gave audio only in Controller Mode, not in Standalone Mode.
- One report says MIDI over USB worked with an iPad but without audio. Anecdotes only.
- Akai markets "two class-compliant USB ports" on the Key 61 (host ports for controllers, interfaces, drives), which is the opposite direction (the Key 61 as host).
- No reverse-engineering notes for the Key 61's own SysEx/pad protocol were found. Closest: https://github.com/bcrowe306/MPC-Studio-Mk2-Midi-Sysex-Charts (MPC Studio Mk2; different device, different format, useful as a structure reference).

Sources: https://www.mpc-forums.com/viewtopic.php?p=1883342 , https://www.mpc-forums.com/viewtopic.php?p=1857582 , https://www.mpc-forums.com/viewtopic.php?p=1857584 , https://www.mpc-forums.com/viewtopic.php?p=1857575 , https://github.com/bcrowe306/MPC-Studio-Mk2-Midi-Sysex-Charts

## What this means for the phone idea
- Controller Mode is the only documented USB-device mode of the Key 61; whether a phone or tablet accepts it as a class-compliant MIDI device is unverified (reports conflict).
- The Key 61 as a pure controller loses the Private-mode features we measured (pad LEDs, buttons) unless the controller still accepts our init sequence in Controller Mode; to be measured.
- Next measurement (needs the owner to switch the Key 61 to Controller Mode and connect it to the Mac): the MIDI ports it exposes, the messages for keys, pads, buttons and LEDs, and whether the Private-mode init block works. Then the same with an iPhone/iPad.
