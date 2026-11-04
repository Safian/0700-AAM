# Akai MPC Key 61 – hardveres próbalista

Ezt kell az eszközön végigpróbálni. A `safian/main` kódja natív Linux (x86) buildben lefordul, és 9 host-teszt (parser, pad-szín, bar-seek, pitch bend, live capture, key transpose, arpeggiator, timing correct, LED-map) átmegy, de **hardveren, ARMv7-en és hallgatással semmi nem lett kipróbálva**. Minden pont mellett: mit kell látni/hallani, és mi a gyanús.

Jelölés: `[ ]` próbálandó · **!** kockázatos (motor-mag, vagy a hardver-viselkedést változtat).

## 0. Indulás
- [ ] Az app elindul, a 1280×800 kijelző forgatva helyes, az érintés pontos.
- [ ] Nincs összeomlás 5 perc használat alatt; a `/data/kick.log` nem tele hibával.
- [ ] Gomb-LED-ek induláskor: minden ki, az aktuális képernyő mód-LED-je világít.

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

## 5. Kijelző, érintés, popupok
- [ ] Popup/dropdown/modális ablakok megjelennek és érinthetők.
- [ ] Multitouch (2 ujj).

## 5b. Érintő-zongora (Menü → Keyboard, `MenuPiano`) — W3-A, hardveren még NEM mért
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

## 7. Próbák, amik döntést hoznak (nem hibakeresés)
**Egyszerűen:** az app futása közben `echo probe > /data/kick_cmd`, majd küldd el a `/data/kick_probe.txt` tartalmát (csak olvasó rendszerjelentés). Kézzel, ha kell (root):
```
cat /proc/asound/cards; cat /proc/asound/pcm; arecord -l; aplay -l
arecord -D plughw:1,0 --dump-hw-params -d 1 /dev/null
arecord -D plughw:1,0 -f S32_LE -c 2 -r 44100 -d 5 /tmp/in.wav
amixer -c 1 scontrols; amixer -c 1 contents; cat /etc/asound.conf
i2cdetect -l; ls -l /dev/i2c-*
ls /sys/bus/i2c/devices; cat /sys/bus/i2c/devices/*/name
dmesg | grep -i -E "i2c|dac|gpio|acvm|i2s"
```
- **Init SysEx Port 0-ra (T5):** indítsd az appot `KICK_AKAI_PORT0=0` környezeti változóval (kikapcsolja a Port 0-ra írást: init SysEx és pad RGB); ha a billentyű/pad-duplázódás nem jön vissza és minden működik, a Port 0 írás törölhető.
- **CV/Gate (T19):** ha nincs `/dev/i2c-*`, a CV valószínűleg az STM32-n megy (SysEx/USB-MIDI) – **ne írjunk vakon I2C eszközre**.

## Ha valami elromlik – visszavonás
A motor-érintő munkák külön commitként vannak, egyenként visszavonhatók (`git revert <commit>`; a mellettük levő merge-commit nem kell):
`904aaf52` + `3e981bf3` + `daf8bd22` (bar seek, a namespace-javítása és a tick-seek/event-seek, **együtt**), `ecfeb260` (pitch bend/sustain), `9fd98db4` (RECALL), `03db3663` (arpeggiator), `d1cd2421` (timing correct), `990fdf44` (automation), `490256b6` (touch strip módok; a T5 kapcsoló és a probe ártalmatlan). Visszavonás előtt futtasd újra a natív buildet (`ninja -C build-linux Kick`).

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
