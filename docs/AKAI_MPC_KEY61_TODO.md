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
| 16 RGB pad LED | ⚠️ bank-szín + találati villanás van; projekt pad-színek nincsenek |
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
| 16 RGB pad (SysEx `0x65`) | ⚠️ | Kész (`7d06f546`): bank-alapszín, ütéskor fehér villanás. Hiányzik: a projekt pad-színei (`kick_PadColor`) nem jutnak a hardverre; a visszaállás a bank színére megy, nem a pad saját színére |

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

- [ ] **T4** (elemezve, nincs kód) Pitch bend és sustain **nincs hatással**: `MTIProgram::noteProcess` (`mt_audio_iprogram.cpp` ~261–336) `isPitchWheel()`/aftertouch ága üres, a CC `switch` csak CC1-et és CC123-at kezel; más felülírás nincs (a `MTMidiProgram` mindent kiküld a külső MIDI portra, ott működik). Javasolt terv: `std::atomic<float> bendRatio` az `MTIAtomicProgramMaster`-ben, szorzás a `master.pitchRatio`-val a voice-ban (`DrumVoice` már olvassa: `drum_voice.cpp` ~164), sustain: atomi pedál-flag + voice-onkénti „held” flag, ONE_SHOT-ot kihagyva. **Döntés kell** (§6/6), és hardveres hallgatásos ellenőrzés.
- [ ] **T5** (részben kész) Eszközön eldönteni, hogy az init SysEx Port 0-ra kell-e (lásd §3/5). A `snd_rawmidi_open` hibakezelés és a `start()` `false` visszatérés kész.

### P1 – hiányzó funkciók

- [ ] **T6** Shift-párok: SOUNDS→FAVORITES, MAIN→TRACK, TC→ON/OFF (`EN_ON_OFF`), AUTOMATION→OFF, REC→RECALL, 16 LEVEL→NOTES külön képernyő (ha létezik).
- [ ] **T7** LOCATE kombinációk (`isLocateDown()` használata): `<<`→START, `>>`→END, `<`/`>`→előző/következő esemény.
- [ ] **T7b** `>>` (bar +1) DCL-kötése: döntés kell (§6/1); jelenleg nincs kötve.
- [ ] **T8** OCT: Shift+OCT félhang-transzpozíció, OCT−+OCT+ reset; LED-létra a mérték szerint.
- [ ] **T9** Billentyűzet/kerekek: channel aftertouch (`0xDF`), mod wheel (CC1), expression (CC11), FS2 (CC65) bekötése az `IHardwareEventListener`-be és a hídba.
- [ ] **T10** Pad aftertouch (`0xA9`) → DCL `PAD` csomag 4. mező.
- [ ] **T11** Q-Link érintés (`on_encoder_touch`) → pl. paraméter-kijelzés / DCL knob-touch.
- [ ] **T12** Touch Strip: gomb (mód-váltás), CC34 LSB, Note 78 érintés.
- [ ] **T13** LED-bővítés: `HardwareLed` + intenzitás/kétszínű API; aktív mód, SHIFT, LOCATE, ARP, LATCH, COPY… LED; BANK E–H megkülönböztetése.
- [ ] **T14** (részben kész, `7d06f546`) Pad RGB: a projekt pad-színeit (`kick_PadColor`) használni bank-szín helyett; ütés utáni visszaállás a pad saját színére. Opcionális: egy keretben frissíteni a 16 padot (most padonként 4 SysEx-írás/port, villanásnál és visszaálláskor is).
- [ ] **T15** Audio bemenet (INPUT 1/2) a `IAudioAdapter` capture-ággal, ha a Looper/Sampler felvételhez kell.

### P2 – robusztusság és teljesítmény

- [ ] **T16** `poll()` a `usleep` helyett; zárolás a RawMIDI írásra (a `set_pad_rgb` az olvasó szálról, `on_pad_note_on`-ból is ír, így több szál ír ugyanarra a portra). A `set_pad_rgb` a Port 0-ra is küld, ami a T5 szerint némítandó.
- [ ] **T17** Eszköz- és útvonal-felderítés (`/proc/asound/cards`, `EVIOCGABS`) a hardkódolt `hw:0,0,x` / `event0` / `2048` helyett; a factory ne tippeljen alapértelmezett MPC-t.
- [ ] **T18** Audio: tényleges `buffer_size` használata, latency-cél egyeztetése a loggal, Phones/Main külön szint, limiter a hard clip helyett. **Eszközön mérendő.**
- [ ] **T19** CV/Gate 1–8, DIN MIDI, USB-B: döntés, hogy az adapter vagy a JUCE MIDI-réteg felel-e értük.
- [ ] **T20** Rejtett tempo-szerkesztő mód (3.4) felülvizsgálata.

### P3 – tesztelés

- [ ] **T21** (részben kész: a parsernek van hoston futó tesztje, `Kick/tests/platform/MidiParserTests.cpp`) Hátra: egységteszt a `get_akai_button` / `get_akai_pad_index` táblákra (mind a 45 gomb + 16 pad); ehhez a táblákat ALSA-mentes headerbe kell kiemelni.
- [ ] **T22** `kick_cmd` `rawmidi` parancs a review szerint, hogy hardver nélkül is lehessen bájtszinten tesztelni.
- [ ] **T23** Hardveres smoke lista (kézi): minden gomb + Shift-pár, 16 pad, billentyű/kerekek/pedálok, LED-ek, kijelző/érintés, audio-peak.

## 6. Nyitott kérdések (döntés kell)

1. Mit kell a `>>` gombnak tennie (a review szerint "1 ütem előre")? Van-e erre DCL-szekvencia (`EN_BACK` párja)?
2. Az ARP gomb valódi arpeggiatort akar, vagy elég a NOTE REPEAT alias?
3. A METRONOME+Shift `EN_PREF` (Config) legyen, vagy marad `HomeOptions`?
4. A `Kick` upstream-barát marad (AGENTS.md), vagy ez az adapter helyi, hardver-specifikus réteg? Ez dönti el, mi mehet upstream PR-ba (valószínűleg csak a platformfüggetlen parser/teszt).
5. Szükséges-e az audio bemenet és a CV/Gate ebben a körben?
6. A pitch bend csak a keygroup/kromatikus programokra hasson, vagy a dobokra is? Fix ±2 félhang elég, vagy programonkénti beállítás kell?
