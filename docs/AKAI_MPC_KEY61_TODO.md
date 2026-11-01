# Akai MPC Key 61 – hardverfunkció-audit és TODO

Dátum: 2026-10-31 · Ág: `safian/main` · Vizsgált commit: `d9a48e38`; frissítve a `7d06f546`, `c8db754a`, `9ade01df` commitok után

## 0. Módszer és korlátok (olvasd el először)

- **Csak statikus kódolvasás történt.** Ebben a környezetben nincs MPC hardver, nincs ARMv7 build, nincs futtatás. Semmi nem lett élőben megerősítve vagy cáfolva.
- Összevetett fájlok:
  - `Kick/apps/kick/hardware/adapters/akai_mpc_key61_adapter.h` (adapter)
  - `Kick/apps/kick/kick_hardware_bridge.h` (híd: HW esemény → Kick)
  - `Kick/apps/kick/hardware/hardware_adapter.h` (HAL interfész)
  - `Kick/modules/engine/audio_program/source/mt_audio_iprogram.cpp` (live MIDI feldolgozás)
  - `docs/AKAI_MPC_KEY61_HARDWARE_REVIEW.md` (a korábbi review)
- **A Note/CC-számok forrása a `HARDWARE_REVIEW.md`** (firmware-diszasszemblálás + élő log). Ezeket itt nem tudtam függetlenül ellenőrizni; csak a kóddal való egyezést néztem.
- Jelölések: ✅ működik (a kód alapján) · ⚠️ részleges / gyanús · ❌ hiányzik · ❓ nem ellenőrizhető hardver nélkül.

## 1. Összegzés

| Terület | Állapot |
|---|---|
| 45 fizikai gomb → `HardwareButton` leképezés (Note-ok) | ✅ mind a 45 le van képezve |
| Gombok elsődleges funkciója | ✅ nagyrészt, néhány ⚠️ |
| Gombok `+SHIFT` funkciója | ⚠️ 6 pár hiányzik |
| `LOCATE` kombinációk | ❌ a flag be van állítva, de sehol nincs használva |
| 16 pad (hang, velocity, bank) | ✅ lookup helyes; ⚠️ aftertouch nincs, dupla trigger kockázat |
| 61 billentyű | ✅ note on/off + oktáv; ❌ aftertouch, mod wheel |
| Pitch bend / sustain / expression / FS2 | ⚠️ továbbítva, de a motor valószínűleg eldobja; expression és FS2 ❌ |
| Q-Link forgatás / Data Dial | ✅ |
| Q-Link érintés, Touch Strip mód | ❌ |
| Gomb-LED-ek | ⚠️ 13 LED, csak ki/be |
| 16 RGB pad LED | ✅ kód (projekt pad-színek + villanás), hardveren nem próbált |
| 7" kijelző + multitouch | ✅ (hardkódolt értékekkel) |
| Audio kimenet | ⚠️ működik, de a latency-beállítás ellentmond a logüzenetnek |
| Audio bemenet (INPUT 1/2, +48V) | ❌ |
| CV/Gate 1–8, DIN MIDI, USB-A/B | ❌ / ❓ nincs az adapterben |

**Fontos:** a `HARDWARE_REVIEW.md` §4 és a commit-üzenet több dolgot "megvalósítottként" ír, ami a kódban **nincs meg** (lásd §4). A dokumentum tervként olvasandó, nem állapotként.

## 2. Funkciónkénti állapot és ok

### 2.1 Gombok (Port 1, ch 0)

Az adapter `get_akai_button()` mind a 45 Note-ot leképezi. A viselkedést a `KickHardwareBridge::on_button_press` adja.

| Gomb | Primer | +SHIFT | Megjegyzés / ok |
|---|---|---|---|
| TAP TEMPO | ✅ tap-BPM | ✅ `EN_MASTER` | Lásd 3.4: rejtett tempo-szerkesztő mód |
| METRONOME | ✅ ki/be | ⚠️ `HomeOptions` | Ugyanaz, mint a MENU; a review `EN_PREF`-et ír. ❓ |
| ARP | ⚠️ `EN_NOTE_REPEAT` | ⚠️ `MenuPadPerform` | Ugyanazt küldi, mint a NOTE REPEAT gomb; arpeggiator nem látszik. ❓ |
| LATCH | ✅ `EN_LATCH` küldve | – | Hogy az UI kezeli-e, nem ellenőriztem ❓ |
| OCT − / + | ✅ ±1 oktáv (−3..+3), ragadásmentes | ❌ félhang-transzpozíció | ❌ A két gomb együtt = reset sincs |
| PAD BANK A–D | ✅ `setBank` | ✅ E–H | ❌ dupla nyomás = E–H nincs |
| TOUCH STRIP (gomb) | ❌ üres `case` | ❌ | Nincs mód-váltás |
| TOUCH FX | ✅ XYFX | ✅ | |
| NOTE REPEAT | ✅ | ✅ LATCH | |
| FULL LEVEL | ✅ | ✅ HALF LEVEL | |
| 16 LEVEL | ✅ | ⚠️ `MenuPadPerform` | Ugyanaz a képernyő, mint KEYBOARD CONTROL primer; a review szerint más kell |
| ERASE | ✅ | – | |
| COPY | ✅ | ⚠️ `EN_ERASE` | A felirat DELETE; `EN_ERASE`-re képezve, szemantika ❓ |
| SHIFT (bal+jobb) | ✅ | – | ⚠️ Egy közös flag: az egyik elengedése törli a Shiftet, ha a másik még le van nyomva |
| MENU | ✅ | ✅ BROWSE | |
| SOUNDS | ✅ File Browser | ❌ FAVORITES | `HW_BTN_SOUNDS` nem nézi a Shiftet |
| MAIN | ✅ ListView | ❌ TRACK | Nem nézi a Shiftet |
| Q-LINK (gomb) | ✅ `EN_LINK` | ✅ KnobEdit | |
| DATA DIAL push, +, − | ✅ | – | |
| EDIT | ✅ ProgramEdit | ✅ XYFX | |
| GRID | ✅ | ✅ StepSeq | |
| MIX | ✅ ChannelMixer | ✅ PadMixer | |
| MUTE | ✅ TrackMute | ✅ PadMute | |
| AUTOMATION | ✅ `EN_AUTOMATION` | ❌ OFF | `EN_ON_OFF` létezik a contractban, nincs bekötve |
| KEYBOARD CONTROL | ✅ | ✅ MIDI CTRL | |
| SAMPLE EDIT | ✅ | ✅ SAMPLER | |
| TIMING CORRECT | ✅ `EN_TC` | ❌ ON/OFF | Ugyanaz, mint fent |
| UNDO | ✅ | ✅ REDO | |
| `<` `>` | ✅ → MINUS/PLUS | ❌ LOCATE+: előző/köv. esemény | |
| LOCATE | ⚠️ csak flag | – | `isLocateDown()` sehol nincs hívva |
| `<<` | ✅ → `EN_BACK` | ❌ LOCATE+: START | |
| `>>` | ❌ nincs kötve (szándékosan: korábban tévesen Enter-t nyomott) | ❌ LOCATE+: END | Kötés kérdéses, lásd §6/1 |
| REC | ✅ | ❌ RECALL | |
| OVER DUB | ✅ | – | |
| STOP | ✅ | ⚠️ Return to start | Megvalósítás (stop→play(false,false)→stop) hatása ❓; dupla nyomás = MIDI panic nincs külön, de minden STOP all-notes-off-ot küld |
| PLAY / PLAY START | ✅ | – | |

### 2.2 Tekerők, érintés, strip

| Elem | Állapot | Ok |
|---|---|---|
| Q-Link 1–4 forgatás (CC16–19) | ✅ | |
| Q-Link 1–4 érintés (Note 84–87) | ❌ | `on_encoder_touch()` üres a hídban |
| Data Dial forgatás (CC100) | ✅ | CC száma a review-ra támaszkodik ❓ |
| Touch Strip pozíció | ⚠️ | Csak CC33 (MSB) megy át; CC34 (LSB) eldobva, Note 78 (érintés) üres ág; a `CC69` ágnak nincs forrása a dokumentációban |
| Touch Strip LED-létra | ⚠️ | A bejövő érték visszhangzik CC33-ra (`set_touch_strip_leds`); nincs külön LED-logika |

### 2.3 Pad és billentyűzet

| Elem | Állapot | Ok |
|---|---|---|
| 16 pad lookup (`{37,36,42,82,40,38,46,44,48,47,45,43,49,55,51,53}`) | ✅ | Egyezik a review-val |
| Pad velocity | ✅ | |
| Pad poly aftertouch (`0xA9`) | ❌ | A parser átadja, de `handle_midi_message` nem kezeli; a DCL `PAD` csomag aftertouch mezője fixen `0` (`msg.data = {pad, state, vel, 0}`); a UI-ban van Aftertouch csúszka és `padPressed(..., Aftertouch)` |
| Pad dupla trigger | ✅ | A híd már csak a DCL pad-csomagot küldi; a Grid/PadMixer/PadPerform/PadColor/FileBrowser/MainMode képernyők maguk indítják a hangot. Következmény: azokon a képernyőkön, ahol nincs ilyen kezelő, a pad nem szól (az upstream viselkedés) – hardveren ellenőrizendő ❓ |
| 61 billentyű (ch 15) | ✅ | Oktáv-eltolás + `m_key_pitch_map` |
| Channel aftertouch (`0xDF`) | ❌ | Az adapter eldobja |
| Mod wheel (CC1) | ❌ | Az adapter eldobja; a motor kezelné (`modWheel()`, CV program) |
| Pitch bend | ⚠️ | Továbbítva, de `MTIProgram` `isPitchWheel()` ága **üres** (`mt_audio_iprogram.cpp` ~331) → valószínűleg nincs hatása, kivéve ha az al-programok felülírják ❓ |
| Sustain (CC64) | ⚠️ | Továbbítva ch1-re; a CC `switch` csak 1-et és 123-at kezel → valószínűleg nincs hatása ❓ |
| Expression (CC11), FS2 (CC65) | ❌ | Eldobva |

### 2.4 LED-ek

| Elem | Állapot | Ok |
|---|---|---|
| Transport, OCT, FULL/16 LEVEL, NOTE REPEAT, BANK A–D | ⚠️ | 13 LED (`HardwareLed`), csak `127`/`0` |
| Aktív mód-gombok (GRID, MIX, MUTE, EDIT…), SHIFT, LOCATE, TC, AUTOMATION, ARP, LATCH, COPY… | ❌ | Nincs `HardwareLed` enum-érték |
| Halvány (1), kétszínű (1–4) szintek | ❌ | Az API csak bool |
| BANK E–H (gomb-LED) | ⚠️ | `b % 4` → ugyanazt a LED-et gyújtja, mint A–D; a pad-színek már megkülönböztetik (`7d06f546`) |
| OCT LED-létra −3..+3 | ⚠️ | Csak "lejjebb/följebb" bool, a mérték nem látszik |
| 16 RGB pad (SysEx `0x65`) | ✅ kód / ❓ hardver | Bank-szín (`7d06f546`), projekt pad-színek és saját színre visszaállás (`753d60f1`). Hardveren nem próbált, a színek hangolandók |

### 2.5 Kijelző, érintés, hang, egyéb I/O

| Elem | Állapot | Ok |
|---|---|---|
| 7" kijelző, 90° forgatás | ✅ | `/dev/fb0` + CPU-s pixelforgatás; ❓ teljesítmény 30 FPS-en |
| Multitouch (ILI2117) | ⚠️ | Hardkódolt `/dev/input/event0` és `0..2048` tartomány; nem kérdezi le `EVIOCGABS`-szal |
| Audio kimenet Main L/R + Phones L/R | ⚠️ | Mindkét pár ugyanazt kapja; a `buffer_size` paraméter figyelmen kívül marad (fix 256); `snd_pcm_set_params(..., 30000)` = 30 ms latency kérés, miközben a log "5.8ms period"-ot ír |
| Audio kimenet hard clip | ⚠️ | ±0.99 vágás, nincs limiter |
| Audio bemenet INPUT 1/2, +48V | ❌ | `IAudioAdapter`-nek nincs capture ága |
| Outputs 3/4 | ❌ | Nem szerepel; a 2–3. csatornát Phones-ként használja |
| CV/Gate 1–8 | ❓ | A DAC-írás a `modules/hardware/source/kick_I2c.cpp`-ben van (feszültség-skálázás javítva: `9ade01df`). Az Akai adapterben nincs; hogy ezen a hardveren elérhető-e, nem tudom. A mod wheel CV-hez az adapter CC1-et még mindig eldobja (T9) |
| DIN MIDI IN/OUT/THRU (Card 3) | ❌ az adapterben | A review szerint a JUCE MIDI manager viszi ❓ |
| USB-B (gadget MIDI), USB-A | ❌ / ❓ | Nem része az adapternek |
| Main Volume | – | Analóg potméter, nem küld MIDI-t (nem hiba) |

## 3. Hibák és kockázatok (javítandó)

3. **Nem működő/ismeretlen hatású kimenő eseményeknek hiszünk**: pitch bend és sustain a motorban valószínűleg nem hat (§2.3).
4. **Rejtett tempo-szerkesztő mód** – TAP TEMPO után `m_tempo_edit_active` átveszi a `+`/`−`/Data Dial funkciót, amíg ENTER vagy STOP nem érkezik. Nem hardveres viselkedés, felhasználót megzavarhat.
5. **Init SysEx mindkét portra** – `start()` az `m_out0`-ra is küldi a `0x41` módot, miközben a review szerint a Port 0-t némítani kell. A hardveren igazolt build így küldte, ezért nem töröltem; eszközön kell eldönteni, kell-e a Port 0 írás (a `set_pad_rgb` is ír a Port 0-ra).
6. **Port 0 / Port 2 tartalék ág** – a `d0 - 36` pad-fallback és a "minden más billentyű" ág a régi hibás viselkedést tartja életben; Port 2-ről érkező bármi billentyűnek számít.
7. **Szálbiztonság** – `set_led()` (UI szál) és a RawMIDI olvasó szál ugyanazt az `m_out1`-et nem zárolja; a HAL `IHardwareEventListener` hívásai az olvasó szálról jönnek (a `dispatchHardwareInput` ezt `callAsync`-kel kezeli, de a `addLiveMidiNote/Msg` és `setBank` közvetlenül hívódik) ❓.
8. **Polling** – `snd_rawmidi_read` NONBLOCK + `usleep(1000)`: akár 1 ms többletlatency és állandó CPU-használat; `poll()` illene ide.
9. **Hardkódolás** – `hw:0,0,0..2`, `plughw:1,0`, `/dev/input/event0`: kártyasorrend változásakor törik. A `HardwareDeviceFactory` a nem felismert eszközt is MPC-nek veszi (`Defaulting to AkaiMpcKey61Adapter`).

## 4. Dokumentum ↔ kód eltérések

A `HARDWARE_REVIEW.md` (§3–§4) és a `d9a48e38` commit-üzenet szerint megvan, de a kódban **nem találom**:

- LOCATE + `<`/`>`/`<<`/`>>` kombinációk
- OCT mellett Shift = félhang-transzpozíció; OCT− és OCT+ együtt = reset
- CC34 (Touch Strip LSB), Note 78 (strip érintés), poly aftertouch, channel aftertouch, CC1/CC11/CC65
- BANK dupla nyomás = E–H
- `/data/kick_cmd` `rawmidi <port> <hex...>` parancs (a `processCommand` nem ismeri)
- MAIN+Shift TRACK, SOUNDS+Shift FAVORITES, TC+Shift ON/OFF, AUTOMATION+Shift OFF, REC+Shift RECALL

## 5. TODO (prioritás szerint)

### P0 – hibák

- [ ] **T5** (részben kész) Eszközön eldönteni, hogy az init SysEx Port 0-ra kell-e (lásd §3/5). A `snd_rawmidi_open` hibakezelés és a `start()` `false` visszatérés kész.

### P1 – hiányzó funkciók

- [ ] **T6** (részben kész és beolvasztva) Hátra: 16 LEVEL+Shift (NOTES): döntés szerint marad a `MenuPadPerform` (Kick `EN_NOTES` is ezt nyitja), nincs teendő. TC+Shift és AUTOMATION+Shift: lásd T30. SOUNDS+Shift / MAIN+Shift / REC+Shift: kész (T25, T29, T28).
- [ ] **T6b** (kész és beolvasztva: `3ac2e6d1`, hardveren nem próbált) Hátra: tempo-szerkesztő módban a `+`/`−` nyomás nem küld semmit, de a felengedés `EN_PLUS`/`EN_MINUS`-t küld (release press nélkül).
- [ ] **T7** LOCATE kombinációk: LOCATE+`<<` (1. ütem) és LOCATE+`>>` (utolsó ütem eleje) kész a T7b-vel. Hátra: LOCATE+`<`/`>` = előző/következő esemény (ehhez eseménylista-API kell).
- [ ] **T7b** (kész és beolvasztva: `904aaf52`, **hardveren nem próbált, a motor lejátszó-magját érinti**) `seekToBar`/`seekBars`/`getSequenceLastBar` a motorban, a `<<`/`>>` kötve. A tick-eltolás a szekvenciában van (`tickShift`), a `CMidiClock` változatlan. Hátra / nyitott: (1) hardveres teszt: `<<`/`>>` lejátszás közben, megállítva, szünetben; hosszú hang vagy ratchet az ugrás fölött; PLAY START parkolt seek után; (2) parkolt seek nem látszik a globális playheaden, amíg semmi nem játszott; (3) song módban a lépés-időzítés nyers óra-tickeket használ; (4) részleges loop és audio-típusú trackek nincsenek elemezve; (5) a Shift+STOP „RETURN” a kijelzett playheadet nem állítja vissza.
- [x] **T8** kész és beolvasztva (`a49e4f38`): Shift+OCT± = ±1 félhang (korlát ±12, saját feltételezés), OCT−+OCT+ együtt = reset; `key_transpose.h` + host-teszt; hardveren nem próbált. Hátra: az oktáv-LED nem mutatja a félhang-transzpozíciót.
- [x] **T9** kész és beolvasztva (`6ea8d26a`): channel aftertouch, mod wheel (CC1), expression (CC11), FS2 (CC65) a Port 1 ch 15-ről a kiválasztott trackre; a motor ezekből csak a CC1-et fogyasztja, a többi a MIDI-program külső portjára megy. Hátra: a Port 0/2 ágon nincs (duplázódás kockázata).
- [x] **T10** kész és beolvasztva (`6ea8d26a`): pad poly aftertouch (0xA0, ch 9) → DCL `EN_PAD_STATE` 4. mező, csak ha a Hardware beállítások oldala bekapcsolta az aftertouch-ot (különben a csomag új note-ra hatna); hardveren nem próbált, az Akai 0xA0 tartománya nem ellenőrzött.
- [ ] **T11** Q-Link érintés: a Kick DCL-ben nincs knob-touch üzenet (`KnobCmd`: csak ROTARY és QLINK), ezért nem kötöttem be; az `on_encoder_touch` üres marad. Döntés: marad, amíg nincs Kick-analóg.
- [ ] **T12** Touch strip: a Kick csak `EN_SLIDER_TOUCH_INPUT` (0..100) üzenetet ismer, nincs touch-vége üzenet; a CC34 LSB és a Note 78 nem ad új jelentést, a TOUCH STRIP gombnak nincs `BtnLedSequence` kódja. Nyitott (Akai-konvenció): a gomb mód-váltó legyen-e (pitch/mod).
- [x] **T13** kész és beolvasztva (`47135cac`): `set_led_level` (0 ki, 1–2 halvány, 3–4 teli A/B), 23 új LED, a mód-LED-ek követik az aktív képernyőt (`onScreenLoaded`), SHIFT/ARP/LATCH/TC LED, BANK E–H a második színnel, módosítás-szűrt (cache) írás; `led_map.h` + host-teszt. **Figyelem:** a kétszínű LED-ek `set_led(true)` most 3-as szintet küldenek (korábban 127), a review 3.5 szerint; hardveren nem próbált. Hátra: jobb Shift LED nincs; az ARP/LATCH/TC LED csak a gombnyomásra frissül (track-váltásra, UI-ból történt változásra elavul); LOCATE, COPY, ERASE, UNDO, TAP_TEMPO, METRONOME, AUTOMATION, TOUCH_STRIP LED nincs hajtva (nincs állapotforrás); MenuPadPerform/MenuKnobEdit LED nem szerepel az eredeti Kick-ben (a gombok ezeket nyitják, ezért világít).
- [ ] **T14** (kész és beolvasztva: `753d60f1`, de hardveren nem próbált) Hátra: a halványítás (0,30 / 0,08) és a hozzá nem rendelt pad megjelenése hangolása eszközön; ellenőrizni, hogy a `KickLEDs` bank-/programváltáskor új pad-szín csomagot küld-e; a Bank A–H pad-színes megkülönböztetése megszűnt (a gomb-LED sem különbözteti meg az E–H-t). Opcionális: 16 pad egy keretben.
- [x] **T15** (felderítve, kód nem kell) Audio bemenet: a teljes Kick app **nem** az `IAudioAdapter`-t használja, hanem a JUCE `AudioDeviceManager`-t (`mt_audio_manager.cpp:73`: `AudioDevice(AUDIO_NUM_INPUTS=2, ..)`, `AudIO/src/juce_adapter.cpp:22,40-48`), amely az ALSA capture-t is megnyitja; a Looper/Sampler ezt fogyasztja. A `AkaiMpcKey61AudioAdapter` (playback-only) csak az `apps/mpc_gui` önálló programban él. Teendő eszközön: ellenőrizni, hogy a JUCE a `plughw:1,0` capture-t megnyitja-e (lásd T32 próbalista). Az `mpc_gui`-ban nincs felvétel-funkció, ott capture nem szükséges. (Felderítő ágens állítása, kódolvasás; nem ellenőrzött futtatással.)

### P2 – robusztusság és teljesítmény

- [ ] **T16** `poll()` a `usleep` helyett; zárolás a RawMIDI írásra (a `set_pad_rgb` az olvasó szálról, `on_pad_note_on`-ból is ír, így több szál ír ugyanarra a portra). A `set_pad_rgb` a Port 0-ra is küld, ami a T5 szerint némítandó.
- [ ] **T17** Eszköz- és útvonal-felderítés (`/proc/asound/cards`, `EVIOCGABS`) a hardkódolt `hw:0,0,x` / `event0` / `2048` helyett; a factory ne tippeljen alapértelmezett MPC-t.
- [ ] **T18** Audio: tényleges `buffer_size` használata, latency-cél egyeztetése a loggal, Phones/Main külön szint, limiter a hard clip helyett. **Eszközön mérendő.**
- [ ] **T19** CV/Gate: a DAC-kód (`kick_I2c.cpp`: `/dev/i2c-3`, cím `0xC`, `0x30|ch` 3 bájtos írás) csak `KICK_ENABLE_HARDWARE=ON` és `hardwareEnabled()` mellett fut, az Akai-ról nincs dokumentált DAC/busz/cím. **Vakon nem írunk** ismeretlen I2C eszközre. Hardveres próba után dönthető el, hogy Linux-DAC vagy az STM32 (USB-MIDI/SysEx) viszi-e a CV-t.
- [ ] **T20** Rejtett tempo-szerkesztő mód (3.4) felülvizsgálata.

### P3 – tesztelés

- [ ] **T21** (részben kész: a parsernek van hoston futó tesztje, `Kick/tests/platform/MidiParserTests.cpp`) Hátra: egységteszt a `get_akai_button` / `get_akai_pad_index` táblákra (mind a 45 gomb + 16 pad); ehhez a táblákat ALSA-mentes headerbe kell kiemelni.
- [ ] **T22** `kick_cmd` `rawmidi` parancs a review szerint, hogy hardver nélkül is lehessen bájtszinten tesztelni.
- [x] **T23** Hardveres próbalista elkészült: `docs/AKAI_MPC_KEY61_HW_TEST_CHECKLIST.md` (gombok, Shift-párok, pad/billentyű, transport/seek, LED-ek, hang, döntést hozó próbák, visszavonási commitok).

## 6. Döntések (2026-11-01)

**Elv:** először a Behringer (Kick) megoldása; ha ott nincs, az Akai (MPC) szokása; ha ott sincs, az általános (raw MIDI/DAW) konvenció.

| # | Kérdés | Döntés | Alap |
|---|---|---|---|
| 1 | `>>` kötése | Ütemugrás (kész: T7b) | Akai |
| 2 | Upstream-hatókör | Az adapter/bridge/motor-seek helyi réteg marad; upstream-re csak platformfüggetlen, tesztelt darabok (pl. MIDI parser + teszt) | felhasználó |
| 3 | Pitch bend | Keygroup **és** dobok, fix ±2 félhang | felhasználó |
| 4 | ARP | Valódi arpeggiator | felhasználó / Akai |
| 5 | SOUNDS+Shift | Fájlböngésző kedvenc-nézet (új UI-belépési pont) | felhasználó |
| 6 | METRONOME+Shift | `EN_PREF` (Config) | felhasználó / Akai |
| 7 | REC+Shift (RECALL) | Új motor-funkció (retro-capture) | felhasználó / Akai |
| 8 | MAIN+Shift (TRACK) | UI-oldali Track-fül választó | felhasználó / Akai |
| 9 | TC+Shift, AUTOMATION+Shift (`EN_ON_OFF`) | UI-fogyasztó kell | felhasználó / Akai |
| 10 | 16 LEVEL+Shift (NOTES) | Marad a mostani `MenuPadPerform` | Kick: az `EN_NOTES` is ezt nyitja |
| 11 | Audio bemenet, CV/Gate | Igen, kell (T15, T19); előbb hardveres felderítés | Kick: van audio-bemenet és CV |
| 12 | Shift+STOP „RETURN” | A kijelzett playhead is álljon vissza az elejére (a parkolt seek ezt már tudja: `seekToBar(seq, 0)`) | Akai |
| 13 | Init SysEx Port 0-ra | Nem döntés, hardveres próba kell (T5) | – |

### Új tételek a döntésekből

- [x] **T27** kész és beolvasztva (`735276a0`): METRONOME+Shift `EN_PREF` (a HomeAppban már volt fogyasztója: HomePreferencesView). **T31** kész (`5cfd562d`): Shift+STOP után a playhead az 1. ütemre áll (`seekToBar(seq,0)`). A motor-seek fordítási hibája javítva (`3e981bf3`).
- [x] **T24** kész és beolvasztva (`03db3663`): valódi arpeggiator (Up, Down, UpDown, Random; 1/4–1/32 + triolák; 1–4 oktáv; gate; latch) per track, ARP gomb ki/be, LATCH az arp latch-ét kapcsolja ha az arp be van; `mt_arpeggiator.h` + host-teszt. A Kick-ben nincs NOTE REPEAT-motor (az `EN_NOTE_REPEAT`-nek nincs fogyasztója). Hátra: az arp csak futó transporton lép (megállított transportnál az élő hang átmegy); nincs UI/Q-Link a mód/ráta/oktáv/gate állításához (a setterek készek); a kiválasztott track váltásakor a régi track tovább arpegiál; nincs ARP LED (T13 folyamatban); hardveren nem próbált.
- [x] **T25** kész és beolvasztva (`57050685`): SOUNDS+Shift a fájlböngésző következő kiosztott kedvenc-helyére ugrik (a Kick-ben nincs külön kedvenc-lista nézet, 5 kedvenc-gomb van); hardveren nem próbált.
- [x] **T26** kész és beolvasztva (`ecfeb260`): pitch bend ±2 félhang keygroup és dob programokon, sustain CC64 (ONE_SHOT-ot kihagyva); hardveren nem hallgatott. Hátra: bend simítás (blokkonként lép), pedál-állapot nem nullázódik programváltásra/stopra, sustain NOTE_OFF típusú mintán nincs hatás.
- [x] **T28** kész és beolvasztva (`9fd98db4`): REC+Shift = RECALL a kiválasztott trackre (30 s, 2048 esemény), egy undo lépés; hardveren nem próbált. Hátra: nincs UI/LED visszajelzés; futó szekvenciánál a note-list zár hiánya miatti lehetséges verseny (`MTNoteList::addNote`), tartott hangok árvaként elvesznek.
- [x] **T29** kész és beolvasztva (`57050685`): MAIN+Shift a HomeListView Track fülére lép (`AppMainmode::selectTab`); hardveren nem próbált.
- [x] **T30** (TC rész kész és beolvasztva: `d1cd2421`, hardveren/UI-n nem próbált) Valódi Timing Correct: `mt_timing_correct.h` (tiszta logika + teszt), `MTAudioInterface::timingCorrect` (egy undo lépés), globális TC be/ki (Shift+TC közvetlenül az API-t kapcsolja, nincs DCL csomag), a List Edit TC-ablak vezérlői bekötve; a RECALL a flag szerint korrigál. Hátra: az „Events: Range” opció nincs támogatva (nincs tartomány-kijelölés); a flag és a paraméterek nem perzisztensek; rövidebb track mint a szekvencia esetén a Start-mód a szekvencia hosszával csavar; nincs TC LED (T13). **AUTOMATION:** nincs motor-fogalom (nincs paraméter-lane tároló, lejátszás, felvétel, undo-adatmodell, szerkesztő UI) – ehhez külön, nagy tervezés kell; az AUTOMATION(+Shift) gombok addig nem csinálnak semmit (EN_AUTOMATION / EN_ON_OFF fogyasztó nélkül).
- [ ] **T32** Eszközön futtatandó próbalista (MPC, root): `cat /proc/asound/cards; cat /proc/asound/pcm; arecord -l; aplay -l`; `arecord -D plughw:1,0 --dump-hw-params -d 1 /dev/null`; `arecord -D plughw:1,0 -f S32_LE -c 2 -r 44100 -d 5 /tmp/in.wav`; `amixer -c 1 scontrols; amixer -c 1 contents`; `cat /etc/asound.conf`; `i2cdetect -l; ls -l /dev/i2c-*`; `ls /sys/bus/i2c/devices; cat /sys/bus/i2c/devices/*/name`; `dmesg | grep -i -E "i2c|dac|gpio|acvm|i2s"`. Ha nincs `/dev/i2c-*`, a CV valószínűleg az STM32-n megy (firmware/SysEx kérdés). A +48V fantomtáp vezérlése valószínűleg az STM32-n át megy (nem ellenőrzött).
