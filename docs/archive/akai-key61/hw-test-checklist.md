> **ARCHIVÁLT, NEM FEJLESZTENDŐ, scope-on kívül (2026-11-13).** Az Akai MPC támogatás megszűnt (owner: "a kick-es akai fejlesztését el tudjuk engedni mostantól, az akaira fejlesztünk csak (a SynthyBoy-t)! max mobilra fogjuk folytatni"). Történeti hivatkozás: ne fejleszts rá, ne használd kiindulásnak. Az itt említett kódútvonalak/linkek elavultak lehetnek.

# Akai MPC Key 61 – hardveres próbalista (csak a még nem igazolt pontok)

Az eszközön végigpróbálandó, **hardveren/hallgatással még nem igazolt** pontok (az igazolt: indulás, érintés, gombok és gombpárok, LED-protokoll `cc`, bank/MIX/MUTE/AUTOMATION LED-ek, két Shift LED, pad egyszeri trigger, strip bemenet, SOUNDS, megállított bar-seek injektálva; lásd [hw-behaviour.md](hw-behaviour.md), [led-colours.md](led-colours.md)). A nyitott tételek összefoglalója: [../../planning/todo-master.md](../../planning/todo-master.md). Ha valami elromlik, a motor-érintő munkák külön commitok (`git log --oneline -- Kick/modules/engine`), egyenként visszavonhatók.

Jelölés: `[ ]` próbálandó · **!** kockázatos (motor-mag, vagy a hardver-viselkedést változtat).

## 1. Pad és billentyűzet (alap)
- [ ] 16 pad: mindegyik a saját helyén szól/villan (Pad Mixer, Grid, Pad Perform); nincs duplázódás (**T3**: a híd nem indít külön hangot).
- [ ] Pad RGB: a pad a projekt saját színét mutatja (T14), ütéskor fehéren villan, felengedéskor a saját színére áll vissza; bankváltás után a színek frissülnek (a `KickLEDs` új csomagot küld-e?).
- [ ] Billentyűzet (61 billentyű): nem vált képernyőt, nem nyom gombot.
- [ ] OCT−/OCT+ (−3..+3): a hang oktávot vált, a LED követi; **Shift+OCT±**: ±1 félhang; **OCT−+OCT+ együtt**: reset; közben tartott hang helyesen elengedődik.
- [ ] **Pitch bend** kerék (±2 félhang): keygroup **és** dob programon hangol (**!** hallgatás: lépcsőzik-e gyors mozgásnál).
- [ ] **Sustain** pedál (CC64): a lenyomott hangok a felengedésig szólnak; ONE_SHOT-ot nem érinti.
- [ ] Mod wheel (CC1), expression (CC11), FS2 (CC65), channel aftertouch: a CV program / külső MIDI port megkapja-e (**a motor belül csak CC1-et fogyaszt**).
- [ ] Pad aftertouch: csak a „Hardware” beállítási oldalon látszik (kapcsold be előbb).

## 2. Gombok és Shift-párok
Minden gomb: lenyomás → várt funkció; felengedés → **nem ragad be** (T6b).
- [ ] TAP TEMPO (tap-BPM), +Shift = Master. **T20** vizsgálat: a Data Dial/+/− tempót állít-e TAP után, és ENTER/STOP kilép-e.
- [ ] METRONOME ki/be; **+Shift = Preferences** (`EN_PREF`).
- [ ] ARP: ki/be (**valódi arpeggiator**: Up/Down/UpDown/Random, 1/16, 1 oktáv; csak futó transporton lép). LATCH: arp bekapcsolva az arp latch-ét kapcsolja.
- [ ] PAD BANK A–D; +Shift = E–H (a LED második színe).
- [ ] TOUCH FX = XYFX; NOTE REPEAT (+Shift LATCH); FULL LEVEL (+Shift HALF); 16 LEVEL.
- [ ] ERASE; COPY (+Shift = ERASE, felengedéskor nem ragad).
- [ ] SHIFT bal/jobb; MENU (+Shift Browse); **SOUNDS** (+Shift: következő kiosztott kedvenc a fájlböngészőben); **MAIN** (+Shift: Track fül).
- [ ] Q-LINK (+Shift KnobEdit); Q-Link 1–4 forgatás; Data Dial forgatás és push; +/−.
- [ ] EDIT (+Shift XYFX); GRID (+Shift Step Seq); MIX (+Shift Pad Mix); MUTE (+Shift Pad Mute); KEYBOARD CONTROL (+Shift MIDI Ctrl); SAMPLE EDIT (+Shift Sampler).
- [ ] **TIMING CORRECT**: megnyitja a List Edit TC-ablakot, az Apply ténylegesen kvantál (egy undo lépés); **Shift+TC** a globális TC-t kapcsolja (nincs LED-visszajelzés a List Edit-ből).
- [ ] **REC+Shift = RECALL** (30 s, a kiválasztott trackre; egy undo lépés; TC ki/be hatása). **!** Futó szekvenciánál a note-list verseny kockázata.
- [ ] UNDO / +Shift REDO.
- [ ] **AUTOMATION**: gomb Read↔Write, Shift = Off; LED Read=3, Write=4. Write módban játszás közben mozgasd a track szintjét (mixer/Q-Link), Read módban a lane visszajátszik; projekt mentése/betöltése megőrzi (**!** tick-szál: figyeld a hangot és a CPU-t). PAN és audio trackek nem automatizáltak.

## 3. Transport és ütemugrás (**!** a motor lejátszó-magját érinti)
- [ ] PLAY, PLAY START, STOP, REC, OVERDUB, LED-ek.
- [ ] **`<<` / `>>`**: egy ütem vissza/előre **lejátszás közben**, **megállítva**, **szünetben**; hosszú hang vagy ratchet az ugrás fölött nem lóg be; nincs hang-elcsúszás vagy kiesés.
- [ ] **LOCATE+`<<`** = 1. ütem, **LOCATE+`>>`** = az utolsó ütem eleje.
- [ ] **Shift+STOP (RETURN)**: megáll, a kijelzett playhead az 1. ütemre áll; a következő PLAY onnan indul. PLAY START parkolt seek után elölről indul.
- [ ] **LOCATE+`<` / `>`**: a kiválasztott track előző/következő note-kezdetére ugrik lejátszás közben és megállítva (PLAY a parkolt tickről indul); hosszú hang az ugrás fölött; nincs wrap (az első esemény előtt tick 0).

## 3b. Touch Strip (új módok)
- [ ] A TOUCH STRIP gomb módot vált: ki = Kick slider, halvány = pitch bend (felengedéskor középre ugrik), teli = mod wheel (CC1). Pitch módban a CC34 (finom) tényleges küldése és az érzet.

## 4. LED-ek (**!** a kétszínű LED-ek mostantól 3-as szintet kapnak 127 helyett)
- [ ] PLAY/REC/OVERDUB/BANK A–D teli fénnyel világít (ha halvány vagy rossz színű: `set_led(true)` 127 volt korábban).
- [ ] Mód-LED-ek követik a képernyőt (GRID, MIX full/A és Pad Mixer B-szín, MUTE, EDIT, SAMPLE EDIT, KEYBOARD CONTROL, MAIN, SOUNDS, MENU, QLINK, TOUCH FX).
- [ ] SHIFT (bal), ARP, LATCH, TC LED a gombnyomásra frissül; track-váltásra elavulhat.

## 5b. Érintő-zongora (Menü → Keyboard, `MenuPiano`) — W3-A (három egyidejű ujj injektálva igazolt; a többi nem mért)
Injektálás a `/data/kick_cmd` FIFO-n (fb-tér 1280x800): `touch_down <slot> X Y`, `touch_move <slot> X Y`, `touch_up <slot>` (slot 0..9); a slot nélküli alakok slot 0. `snapshot /data/x.ppm` a képhez, `/data/kick.log` az eseményekhez.
- [ ] **Két ujj, két hang:** `touch_down 0 X1 Y`, `touch_down 1 X2 Y` két külön billentyűre → a logban két `[TOUCH] manual mouseDown`, két note-on; `touch_up 0` csak az elsőt engedi el. (Azonos billentyűre két ujj: egy note-on, note-off az utolsó ujjnál.)
- [ ] **Glissando:** `touch_down 0 X Y`, majd `touch_move 0` több billentyűn át, `touch_up 0` → Glide módban minden váltásnál off-majd-on, a végén egyetlen off; Legato módban on-majd-off (átfedés); sehol nem marad beragadt hang (a motor-meter és a hang elhallgat).
- [ ] **Velocity:** Fixed (a `-`/`+` értéke), Position (a billentyű alja = 127, teteje = a minimum), Speed (lassú csúszás = halk, gyors = hangos; az egyszerű koppintásnak nincs sebessége → a Fixed érték).
- [ ] **Bend / Vibrato:** Bend módban vízszintes húzás a billentyűn → pitch wheel (a hang magassága a motorban: dob/keygroup programon, a tartomány ±2/4/7/12 a gombbal), elengedéskor 8192-re visszaáll; Vibrato módban CC1. MIDI programnál a külső szinti dönt a tartományról.
- [ ] **Sustain gomb:** gomb le, billentyű le-fel → a hang szól tovább; gomb fel → elhalkul (CC64 előbb, aztán a note-off-ok); a lábpedál és a touch strip Sustain módja nem emeli fel egymás pedálját.
- [ ] **Közös transzponálás / track:** a Key 61 OCT-/OCT+ gombja és a képernyő OCT-/ST- gombjai ugyanazt állítják (a képernyő felirata és az OCT LED-ek követik egymást); a képernyő és a fizikai billentyű ugyanarra a kijelölt trackre ütnek; track-váltás lenyomott hang közben → a note-off az eredeti trackre megy.
- [ ] **Külső hangok kirajzolása:** fizikai billentyű / lejátszott szekvencia hangja cián kerettel jelenik meg a megfelelő billentyűn; a skála/gyökér/akkord jelölés a Pad Perform beállításával egyezik.
- [ ] **Késleltetés (touch → hang):** `echo "latency 1" > /data/kick_cmd` (vagy `KICK_LATENCY=1` indításkor), majd `touch_down 0 X Y` / `touch_up 0`. A `/data/kick.log` `[LAT]` sorai: `touch down ... read_us=` (az eseményszál olvasta), `message thread ... +Nus` (az üzenetszál felvette), `note-on ... addLiveMidiNote called Nus after the touch was read (call took Mus)` (a hívásig eltelt idő). A mért érték a motorba kerülésig tart; a hallható késleltetéshez a hangblokk-méret (audio puffer) adódik hozzá. Mérés a 30 Hz-es festés mellett és nélküle (pl. állandó képernyő vs. üres képernyő) összevetve; `latency 0` kikapcsolja.

## 6. Hang
- [ ] A kimenet peak ~0 dBFS körül (korábbi −54 dBFS hiba javítva); Main és Phones.
- [ ] Audio bemenet (INPUT 1/2, +48V): a Looper/Sampler látja-e (a teljes app a JUCE audioeszközt használja, nem az adaptert).

- [ ] **MIDI program PAN → CC10** (`def27ac2`): egy MIDI programnál a mixer PAN-gombja CC10-et küld a külső portra (bal szélső 0, közép 64, jobb szélső 127; azonos értéknél nem ismétel); **projekt betöltésekor ne menjen ki felesleges CC10**.

## Sequencer screens (Step Sequencer, List Edit, Next Sequence, Song, Color Edit) - added after the S-C code round

Inject with `/data/kick_cmd` (`btn`, `pad N V`, `jog N`, `rawmidi PORT hex...`, `snapshot /data/x.ppm`); read `/data/kick.log`. Q-Link 1 = `rawmidi <port> B0 10 01` (RPT L143).

Step Sequencer
1. Shift+GRID / STEP SEQ opens it; 16 pads = 16 steps of the page, display updates at once; Full/Half Level changes the stored velocity.
2. Q-Link 1-4 in Velocity view on an active step: step keeps its Probability / Ratchet (List Edit: Prob column unchanged), plays; on page 2 (16T / 32T) the 4 Q-Links change the steps of page 2.
3. Bank buttons A/B page through 16T (24 steps = 2 pages), 32T, 64T.
4. Dial and +/- nudge; a fast dial turn moves several steps; after a display touch check that the dial still reaches the screen.
5. Mirror / Flip / Pulse / Inverse: Probability / Ratchet of the steps survive Mirror; Pulse switches steps off (notes deleted); no velocity 0 notes.
6. UNDO after a touch drag, Clear Pad, Clear Bar, Nudge: ONE press reverts the gesture; REDO. Display refreshes after UNDO and during recording.
7. Open the screen with Grid TIME DIV = 1/32T, then back: the Grid quantization is unchanged.
8. Playing: play position marker and page following in CLASSIC; CPU / frame time with the screen open and on another screen (hidden screen must cost nothing).
9. Audio / Clip track selected: notice shown, controls off.
10. GRID: finger accuracy on the 19 px rows, SELECT toggle without the physical Shift.
11. Pad LEDs in the Step Sequencer: expected NOT lit (bridge drops the step LED commands), report what is lit.

List Edit
12. TC button, ENTER = Apply, STOP = Apply, Shift+STOP / MAIN = Cancel, same with a TYPE / EVENTS popup open; after one Apply press UNDO once: the whole pass is reverted. EVENTS offers All / Selected only.
13. Data Dial / +/- walk the rows; Q-Link 1-4 edit the selected row (also on an empty track: nothing happens, no crash); Cursor L/R behaviour (acts on release).
14. Delete with nothing selected deletes nothing; Insert BAR 1 / BEAT 1 / TICK 0 lands on 01:01:00; tap on Time / Pad opens the editors.
15. Open time and memory with 100 / 500 / 1000 notes (the pad cell is lighter now; row virtualisation is still open and depends on this measurement).
16. Record pads with the list open: the list grows without reopening.

Next Sequence / Song / Color Edit
17. Next Sequence: PLAY, queue a pad, STOP (touch header or hardware), change bank; repeat with STOP on another screen and coming back: no crash (`/data/kick.crash` must stay absent). PLAY vs PLAY START; header Play/Stop button.
18. Song: create (empty / duplicate / "create new song" names are refused), Insert 3 steps, PLAY from step 2, wait for the end (UI unlocks, transport stops), Clear / Delete + UNDO, rename and delete a song, delete a sequence used by a step (step turns red, PLAY refuses).
19. Color Edit: press a hardware pad: it plays and takes the colour, the hardware pad LED follows; Data Dial steps the colours; +/- change the bank; Make Default gives the program type's defaults in all banks; the Pad Off / Pad On / Empty controls still have NO hardware effect on the MPC (bridge does not implement the colour mode).

Pad Perform (Pad Notes) and Program Edit (S-F)
20. KEYBOARD CONTROL and Shift+16 LEVEL open Pad Notes. Chromatic / Notes (Major, Minor) / Chords: the 16 pad names match what you hear; pad LED root = red, scale note = dark yellow.
21. Chords, `1-3-5-7`: C-major root pad must sound C E G B (a major third, not a fourth); `1-3-5-7b` = C E G Bb. A chord near the top of the range (octave 8) plays fewer notes, never a wrong high note.
22. Progression: the pads are the scale degrees; with C major `1-3-5` pad 2 = D F A (minor), pad 7 = B D F (diminished); `1-3-5-7` pad 5 = G B D F.
23. Q-Link 1-4 (Screen mode, default): 1 Scale, 2 Root, 3 Octave, 4 Chord list (Chords / Progression) or the mode (Chromatic > Notes > Chords > Progression). Fast turns do not lag the pad LEDs (the colour packet is sent once per burst).
24. Hold a pad, then turn Q-Link 1 / 2 or change bank, release the pad: the note ends (no stuck note). Hide the screen (MAIN) while holding pads: all notes end.
25. Touch the on-screen pads: one note per finger (try 3 fingers), velocity higher towards the bottom of the pad, note ends on lift; the pad highlights while held; the hardware pads do NOT sound twice.
26. Touch the +/- bank buttons, then hit the hardware pads: they must play the notes of the bank that the screen shows (and the LEDs show that bank). Octave list on bank H changes the pads too.
27. 16 LEVEL button: Pad Notes opens with the last pad on all 16 pads, velocity 8..127 rising; the LED lights; the Tune switch plays pad 9 = the note, others +-semitones; pressing 16 LEVEL again (or leaving the screen) ends it and the LED goes out. Check the LED really follows (DCL LED packet).
28. Held pads show the note / chord name next to the 16 LEVELS buttons ("Cmaj7"). Pad LED while held: NOT implemented (needs a look at what the bridge already does on a hit).
29. Keyboard aftertouch on a MIDI-program track arrives on the track's output channel (not always channel 1).
30. Program Edit, EDIT button: Clip A <-> Clip B switching does not crash and keeps both programs; Data Dial turn without a touched control plays NO note; Q-Link 1-4 move the first four rotaries of the page (Drum Main: Master Level / Pan / Semi / Fine). Touch the PAD list and pick a pad: the page shows that pad, the bank follows.
31. Program Edit on a Plugin program: the CLAP panel opens (button OPEN PLUGIN EDITOR re-opens it).
32. MIDI program: moving a CC slider sends the CC to the external synth / appears in the project.
33. Drum Layer Play: Cycle goes round all layers with a sound (repeat 8 hits), Random uses only loaded layers, Velocity with a 127 hit (Full Level) plays the layer whose range includes 127; layer velocity ranges are edited on Layer Mix (VEL START / VEL END).
34. Keygroup: Sample Play "Note On" holds the sound while the key is held; "Note Off" starts it on release; Layer Pan / Pan give fine values; ZONE LOW / HIGH change which sample plays a key when the keygroup has several samples.
35. Audio program: SPLIT with a sample selected and the playhead inside it cuts it in two at the playhead; Fade / Semi / Fine / TC / Don't Snap / Copy are hidden by design (nothing behind them yet).

Looper, Inserts, XY FX, Q-Link Edit, MIDI Control (S-E)
36. Looper: OVERDUB, leave OVERDUB (press the button again), then plain REC: it replaces (no overdub); then in Step Sequencer arm REC and press PLAY: the take starts (the overdub flag used to block sequence takes). Looper hardware REC / STOP, then check Export / Clear are enabled without touching the screen buttons. 100 BPM project: record, leave the screen, come back: the loop is still there. Change LENGTH with a recording: a confirmation appears. UNDO button swaps the last take back (press again = redo). Q-Link 1-4 = threshold, monitor level, output gain, output pan. Leaving the screen stops the looper but does NOT pause a running sequence. (Known open, R-2: overdub does not play the old loop while recording, gain / pan are applied twice, waveform copy under the spin lock.)
37. Looper export: the WAV lands in `<project>/samples/<name>.wav`, a second export with the same name gets a new name; pad it to a drum pad and play it (5 s cache limit still applies).
38. Inserts: put an effect on a Return, a Submix, an Output and the Master bus and send audio to it: it must be HEARD (bus inserts were silent before; host-tested, never heard on the device). Pre-fader: the bus fader scales the effect's output. Add / delete / replace inserts while playing, repeated: no crash. Save, reload: names and values are back (also NC-17 = "NCSeventeen"). MIDI / CV program: the Inserts screen says it has no inserts, ADD does nothing.
39. Inserts Q-Link 1-4 = first four parameters of the shown insert; Data Dial = next / previous slot; value texts show the effect's own units; ALL ENABLE shows the real state; ALL DELETE asks first.
40. XY FX: choose each of the 11 presets and each destination (Track, Return 1-4, Submix, Outputs), turn On, touch the grid while a sequence plays: the filter / beat repeat / phaser must be audible; finger up closes it (attack / release times), Latch holds it. Setup closed: Q-Link 1-4 (preset, attack, release, wet/dry) show their values in the status line; Data Dial and +/- change the preset, ENTER toggles Latch. Leave the screen while latched: the effect stops. CPU / xrun on ARMv7 for Beat Repeat and Phaser (not measured).
41. Q-Link Edit: Custom mode, Q-LINK button = next row of four knobs (the highlighted row follows); Pad Level / Pan in bank B-H change the right pad (and not the A bank pad); Mute / Solo switch on turning right and off turning left; Pan moves in 0.01 steps; the assignment survives a restart. The Q-LINK button no longer closes dialogs after 5 s. MIDI program CC list on a track that is not the selected one: the CC goes out on that program's channel.
42. MIDI Control: Hardware tab assignment survives a restart; press Shift / REC / OVERDUB / bank buttons / Data Dial: NO MIDI message appears on the monitor (OUT); each front-panel button sends its own CC (Toggle flips 127 / 0 on press only), Q-Link Absolute / Relative (relative = 0x01 / 0x7F); output port list starts with "None" and remembers the port; XY tab sends a CC only when a value changes; MIDI Learn: a learned row reacts only to its own channel / CC / note, Flip inverts, Mute / Solo are on at >= 64; the table is filled after project load and the first Add keeps the old rows; IN view of the monitor shows the learn input.

## Sequencer editors, round R-8 (Step Sequencer, List Edit, Next Sequence, Song, Color Edit, status bar)

Labels R8-n (the numbering of the main list is shared with other rounds). Inject with `/data/kick_cmd`; read `/data/kick.log`.

R8-1. Step Sequencer on a REAL non-4/4 project (3/4 and 7/8, set in Main > Sequence > Time Signature): at 1/16 the bar has 12 (3/4) / 14 (7/8) steps, the play marker runs across exactly those steps and wraps with the bar; at 1/4 notes in 7/8 the last eighth of the bar has no step (expected, see the plan); a note recorded there shows in List Edit.
R8-2. GRID view finger accuracy on the 19 px rows (touch each of 16 rows 10 times in the middle and at the edges): count the wrong rows. If more than 1 in 10 is wrong: 8 rows per page and rows of >= 36 px are needed (SS-31).
R8-3. Next Sequence: PLAY, queue a pad, press Hold: the queued pad is dropped, the current sequence repeats; press other pads: they get a green border, nothing queues; Next Bar and Sudden then switch to the last selected one; Hold off: nothing is queued. Clear on a queued (not playing) sequence asks, deletes it, UNDO in the status bar brings it back; Clear is off for the playing sequence. Song button (stopped) copies the playlist: pick a song or NEW SONG, then check the Song screen (one UNDO removes all copied steps).
R8-4. List Edit: open time and scrolling with 100 / 500 / 1000 / 3000 notes (rows are virtual); Select button and Shift + tap select several rows; Vel -/+ and Nudge < > change all selected, UNDO once reverts one operation; Delete with several selected; Playhead button scrolls to the playing row and the red arrow follows the playhead while playing; Play sounds the selected note; Shift + dial moves the selection by the TC division, Shift + Q-Link 1 shifts the pad by a bank, Shift + Q-Link 2 changes the length by the TC division; REC (sequence armed), stopped: pads insert notes at the playhead (TC division long); with Settings > Auto Advance On the position moves on; View > Track Automation shows the Level / Pan lane breakpoints (record some with AUTOMATION Write first), edit value / time, Insert > Track Automation, Delete; double tap on a number opens the keypad.
R8-5. Timing Correct window: open TYPE, press ENTER / STOP / MAIN / Shift+STOP: the list popup closes first, the TC window stays; the next press applies / cancels the window (Cancel is still unmeasured).
R8-6. Song: Rec toggle (and hardware REC armed): pads add steps at the end, the same pad again raises the repeat, nothing plays; STOP ends it; Data Dial with a selected step changes its sequence, Shift + dial or no selection walks the steps; PLAY a 3-step song with repeat 2: Sudden jumps to the next step at once, Next at the start of the next bar (listen for the exact bar), both are off on the last step and while stopped.
R8-7. Color Edit: Shift + tap a colour button colours all pads (UNDO offered); Shift + pad (hardware or touch) selects the colour of that pad; Make Default asks (Do It / Cancel) and stores the program's colours as the default of its type (create a new drum program: it starts with them; restart Kick: they are still there); Reset (asks, UNDO offered) returns the program to that default.
R8-8. Step Sequencer: the Settings button (bottom bar, right of Clear Bar) opens the settings: Velocity 80, pad a step on: velocity 80 in List Edit; Pad velocity On: the pad's own velocity; Shift + pad selects the pad without touching a step; Q-Link down to the lowest velocity and one more: the step is deleted; Clear Pad / Clear Bar / shorten TOTAL BARS: the status bar shows the message and UNDO.
R8-9. Status bar UNDO button: appears next to a message after Clear Pad, a Next Sequence delete, List Edit Delete / Insert, Color Edit All Pads; pressing it undoes ONE step and shows "Undone"; it disappears after 8 s or with the next message; the hardware UNDO button still works.

Menu, header, status bar, Kick Update (R-7)
43. MENU: the Menu shows page 1 of 16 tiles; the page arrows (touch) switch to page 2 (Resources, Kick Update, Favorites, ...); the 16 pads press the tiles of the SHOWN page (pad 1 = bottom left tile), also on page 2. Data Dial turns across the page edge; Shift + Data Dial turns by a row.
44. MENU, "Move tiles": touch a tile, then its new place: the order changes and is still there after a restart; "Reset menu" puts it back. Shift + Data Dial press toggles the mode; with a tile picked up (press the Data Dial on it) the dial moves it, a press drops it. The status bar shows the tile name while the dial / pads move over it.
45. MENU > Favorites opens the file browser on the next assigned favourite (same as the second SOUNDS press).
46. Status bar: BPM and time signature of the selected sequence are shown; touch the BPM field (Tempo editor, the TAP TEMPO button there uses the tap count of Preferences) and the time signature field (3/4, 6/8, 7/8: the grid, the BBT popup beat range and the Metronome click with RATE: SEQ follow). A "Project saved" / "Saved: ..." message disappears after about 6 s; "Unsaved project changes" stays until saved.
47. Header: the CPU percentage under the Resources icon follows the load (red at 85 %); the Automation icon follows the AUTOMATION button of the hardware while the screen is open.
48. Main screen: Undo / Redo (touch and the hardware UNDO) show "Undid: <step>" and refresh the tab; the History button lists the steps newest first and goes back as far as chosen. Undo is refused while a take is recorded (touch).
49. System Resources: the AUDIO bar moves with the load; "dropouts" counts after you overload the audio on purpose; with the internal storage nearly full the status bar says "Low storage".
50. Kick Update / pendrive: Preferences > Info > Firmware Update opens Kick Update; with a package that has a CHANGELOG the first lines show. Rollback: put an empty `ROLLBACK` file on the USB drive after an update and restart the MPC: the previous Kick starts and the same pendrive does not install the newer one again (`/data/kick-boot.log` says "ROLLBACK complete" / "was rolled back from").
51. Recent projects (Projects screen): open an older project - it moves to the end (newest) of the list; the list never shows more than the Preferences maximum; rename / delete from the tile menu keep one entry per project.


## Inserts, XY FX, Q-Link, MIDI Control, Looper, round R-11

Labels R11-n. Inject with `/data/kick_cmd`; read `/data/kick.log`. Everything below is host- and Mac-smoke-tested; nothing was heard or measured on the MPC.

R11-1. Inserts add / delete while the sequence plays, repeated 20 times on a program, a Return, a Submix, Output and Master (no crash, no click you did not cause). Return 1 with `Reverb`, send to it: audible. Buses: the PRE-FADER / POST-FADER button on the bus Inserts screen: pre-fader (default), pull the bus fader down: the effect tail goes with it; post-fader: the tail stays audible when the fader is down; the choice survives save and reload.
R11-2. Insert CPU on ARMv7, one stereo insert at a time: `Pockey` (host: 0.68 of real time, expect it not to run), then `BitGlitter`, `LeadAmp`, `FireAmp`, `GrindAmp`, `BigAmp`, `PocketVerbs`, `MidAmp`, `LilAmp` (host 0.03-0.05), and `Reverb`, `Chamber2`, `TapeDelay`, `PitchDelay` for comparison. Note xruns and the System Resources CPU. These nine show HEAVY / VERY HEAVY on the insert screen: confirm the list is the right one for the MPC, and tell which others to add to `kick_InsertCost.h`.
R11-3. Inserts save / load: project with 3 inserts incl. `NC-17`, a bypassed one and a changed parameter: reload, the names, the bypass and the value are as left. Stepped parameters (for example `Biquad` Type): the knob and Q-Link 1..4 jump between 4 positions, no in-between values. Favourites (star button) and the user preset SAVE / LOAD survive a restart of Kick; A/B toggles two sets of a slot.
R11-4. Insert-parameter automation: AUTOMATION to Write, play the sequence, turn an insert knob (Inserts screen of the selected track's program): turn AUTOMATION to Read, play: the parameter follows; leaving Read puts the stored value back; save, reload, still there. (Only the selected track's own program chain records.)
R11-5. XY FX: AUTOMATION Write, play, touch and drag in the XY screen with the location Track, Latch off: in Read the filter / beat effect repeats the gesture with the gate opening and closing where the finger was; Clear in List Edit > View Track Automation is not shown for these lanes (expected); the Automation icon in the header works. Location "MIDI CC": the XY tab of MIDI Control sends the same CCs, channels, ranges and uses the same output port (`amidi -d`); a muted axis sends nothing; a drag sends at most 127 messages per axis.
R11-6. XY FX cell labels: the bottom row and the left column show the value at the centre of each cell (Hz / Q / % / beat division) and follow the preset. CPU while Beat Repeat and Phaser are active (host: 0.3 % of a 48 kHz block): xruns on the MPC?
R11-7. Q-Link Edit, Screen mode: open it from the Grid, XY, Looper, Sampler, Pad Mixer, Track Mute, Pad Mute, Pad Perform, List Edit, Next Sequence, Song screens (via Shift+Q-LINK): the right side names the four Q-Links of the screen you came from; turn each Q-Link on that screen and confirm the named role. On a screen that is not listed it says so. Q-Link 1..4 and the Q-LINK row button as in the earlier list (checklist 41).
R11-8. MIDI Control: start Kick with a MIDI controller plugged in and a learned row, do NOT open MIDI Control: the row works. Plug a second controller in while Kick runs: within 3 s its learned rows work; unplug: no log errors. Two rows learned on the same CC both react. Delete / Clear of a row shows the status bar UNDO; the output port setting survives a restart.
R11-9. MIDI Control TSAN run (MC-28): Kick built with `-fsanitize=thread` on the Mac (`cmake -DCMAKE_CXX_FLAGS="-fsanitize=thread -DKICK_APP_SMOKE_TEST=1"`), a hardware or virtual MIDI source (IAC bus, `sendmidi` / `midi-monkey`) sending 1000 CC / s on a learned row for 60 s with the MIDI Control screen open and the table being edited: no TSAN report.
R11-10. Looper: Export from a 4 bar loop (record a few seconds only): the file is exactly 4 bars long (the bar count of the screen), a 2 bar loop exports 2 bars; the Looper screen shows the sequence's time signature on its scale (try 3/4 and 7/8). Also: INPUT meter, REC / OVERDUB audibility, SYNC phase, 16 bar loop at 60 BPM, leaving the screen; the v3.10 Looper parity table is in `docs/planning/screen-backlog.md` (Looper).
