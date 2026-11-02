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
| Touch Strip LED-létra | n/a | Az MPC v3.10 útmutató szerint nincs szoftveres funkciója ("Light LED"); a Kick nem ír rá (a `set_touch_strip_leds` törölve), a sötét sor nem hiba |

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
| Main Volume | ✅ (kód; hardveren nem igazolt) | Nem analóg: SysEx `F0 47 7F 4B 5B 00 01 <v> F7` (v 0x00..0x75, mért) a Port 1-en; a Kick master szintjére kötve (AD1a), a `0x5B` parancsbájt jelentése ismeretlen |

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

- [ ] **T5** (kész, hardveres döntésre vár) `KICK_AKAI_PORT0=0` környezeti változó a Port 0-ra írás (init SysEx + pad RGB) kikapcsolására, alapértelmezés = a mostani viselkedés, a beállítás logolva indulásnál (`31b7d850` után: `490256b6`). Teendő eszközön: kipróbálni `KICK_AKAI_PORT0=0`-val; ha a pad/billentyű-duplázódás nem jön vissza, a Port 0 írás törölhető. Hardveres próba 2026-11-01: `KICK_AKAI_PORT0=0` mellett is minden gomb-LED világít, tehát nem a Port 0 init SysEx az ok; pad/billentyű duplázódás nem mérve (`docs/AKAI_MPC_KEY61_HW_TEST_REPORT.md`). **Végső (2026-11-02):** az alap (Port 0 írás be) mellett a LED-ek és a pad-színek működnek; a `PORT0=0` összehasonlítás nincs mérve.

### P1 – hiányzó funkciók

- [ ] **T6** (részben kész és beolvasztva) Hátra: 16 LEVEL+Shift (NOTES): döntés szerint marad a `MenuPadPerform` (Kick `EN_NOTES` is ezt nyitja), nincs teendő. TC+Shift és AUTOMATION+Shift: lásd T30. SOUNDS+Shift / MAIN+Shift / REC+Shift: kész (T25, T29, T28).
- [x] **T6b** kész és beolvasztva (`3ac2e6d1`, `53d80738`): minden Shift-függő és megosztott-kódú gomb (COPY, FULL LEVEL, TAP TEMPO, NOTE REPEAT, BANK, ARP, PLUS/MINUS, STEP/CURSOR) a nyomáskor küldött kódot engedi fel, press nélkül nincs release; hardveren nem próbált.
- [x] **T7** kész és beolvasztva (`daf8bd22`): LOCATE+`<`/`>` a kiválasztott track előző/következő note-kezdetére ugrik (`seekToTick`, `findEventTick`, `mt_event_seek.h` + host-teszt); a keresés az üzenetszálon fut. Nincs wrap: előző esemény előtt tick 0, következő nélkül marad. Hardveren nem próbált; az MPC track- vagy szekvencia-szintű-e, nem ellenőrzött.
- [ ] **T7b** (kész és beolvasztva: `904aaf52`, **hardveren nem próbált, a motor lejátszó-magját érinti**) `seekToBar`/`seekBars`/`getSequenceLastBar` a motorban, a `<<`/`>>` kötve. A tick-eltolás a szekvenciában van (`tickShift`), a `CMidiClock` változatlan. Hátra / nyitott: (1) hardveres teszt: `<<`/`>>` lejátszás közben, megállítva, szünetben; hosszú hang vagy ratchet az ugrás fölött; PLAY START parkolt seek után; (2) parkolt seek nem látszik a globális playheaden, amíg semmi nem játszott; (3) song módban a lépés-időzítés nyers óra-tickeket használ; (4) részleges loop és audio-típusú trackek nincsenek elemezve; (5) a Shift+STOP „RETURN” a kijelzett playheadet nem állítja vissza. Hardveres próba 2026-11-01: injektált `BAR_NEXT`/`BAR_PREV` megállítva működik (`1:04:057` → `2:01:000`); játszás közbeni és fizikai próba még hiányzik. `<`/`>` (68/69) helytelenül `EN_MINUS/EN_PLUS`, az Akai leírás szerint egy lépés (riport 3. hiba). **Végső (2026-11-02):** `<<`/`>>` injektálva lejátszás közben és megállítva is működik (megállítva `BAR_NEXT` ×2 → `3:01:000`); a fizikai `<<`/`>>` megállítva újra nincs mérve.
- [x] **T8** kész és beolvasztva (`a49e4f38`): Shift+OCT± = ±1 félhang (korlát ±12, saját feltételezés), OCT−+OCT+ együtt = reset; `key_transpose.h` + host-teszt; hardveren nem próbált. Hátra: az oktáv-LED nem mutatja a félhang-transzpozíciót.
- [x] **T9** kész és beolvasztva (`6ea8d26a`): channel aftertouch, mod wheel (CC1), expression (CC11), FS2 (CC65) a Port 1 ch 15-ről a kiválasztott trackre; a motor ezekből csak a CC1-et fogyasztja, a többi a MIDI-program külső portjára megy. Hátra: a Port 0/2 ágon nincs (duplázódás kockázata).
- [x] **T10** kész és beolvasztva (`6ea8d26a`): pad poly aftertouch (0xA0, ch 9) → DCL `EN_PAD_STATE` 4. mező, csak ha a Hardware beállítások oldala bekapcsolta az aftertouch-ot (különben a csomag új note-ra hatna); hardveren nem próbált, az Akai 0xA0 tartománya nem ellenőrzött.
- [x] **T11** (vizsgálva, kód nem kell) A Q-Link érintésre nulla-delta csomag **káros** lenne: a PadMixer/program-mix mute/solo kapcsolója `mute.setToggleState(value>0)` / `solo.setToggleState(value<=0)`, így nulla delta a mute-ot kikapcsolná, a solo-t bekapcsolná; a számvezérlők `setValue(val_+0)`-t hívnak (semmi), és a UI nem emel ki paramétert. A Kick-nek nincs knob-touch fogalma; az `on_encoder_touch` szándékosan üres.
- [x] **T12** kész és beolvasztva (`490256b6`): TOUCH STRIP gomb módot vált: 0 = Kick slider (régi viselkedés), 1 = pitch bend (CC33+CC34 14 bit, Note 78 elengedésre 8192-re ugrik), 2 = mod wheel (CC1); a mód-LED: ki = slider, halvány = pitch, teli = mod (egyszínű LED, a 3/4 szint nem különböztethető meg). Hardveren nem próbált: a CC34 tényleges küldése, a halvány LED-állapot, a pitch-érzet. Nyitott: mod módból kilépéskor CC1 nullázódjon-e; a pitch mód relatív legyen-e.
- [x] **T13** kész és beolvasztva (`47135cac`): `set_led_level` (0 ki, 1–2 halvány, 3–4 teli A/B), 23 új LED, a mód-LED-ek követik az aktív képernyőt (`onScreenLoaded`), SHIFT/ARP/LATCH/TC LED, BANK E–H a második színnel, módosítás-szűrt (cache) írás; `led_map.h` + host-teszt. **Figyelem:** a kétszínű LED-ek `set_led(true)` most 3-as szintet küldenek (korábban 127), a review 3.5 szerint; hardveren nem próbált. Hátra: jobb Shift LED nincs; az ARP/LATCH/TC LED csak a gombnyomásra frissül (track-váltásra, UI-ból történt változásra elavul); LOCATE, COPY, ERASE, UNDO, TAP_TEMPO, METRONOME, AUTOMATION, TOUCH_STRIP LED nincs hajtva (nincs állapotforrás); MenuPadPerform/MenuKnobEdit LED nem szerepel az eredeti Kick-ben (a gombok ezeket nyitják, ezért világít).
- [ ] **T14** (kész és beolvasztva: `753d60f1`, de hardveren nem próbált) Hátra: a halványítás (0,30 / 0,08) és a hozzá nem rendelt pad megjelenése hangolása eszközön; ellenőrizni, hogy a `KickLEDs` bank-/programváltáskor új pad-szín csomagot küld-e; a Bank A–H pad-színes megkülönböztetése megszűnt (a gomb-LED sem különbözteti meg az E–H-t). Opcionális: 16 pad egy keretben. Hardveres próba 2026-11-01: pad RGB nem tesztelt; a gomb-LED-ek mind világítanak (riport 4. hiba). **Végső (2026-11-02):** a pad-színek bankonként látszanak; hosszabb szín-mérés nincs.
- [x] **T15** (felderítve, kód nem kell) Audio bemenet: a teljes Kick app **nem** az `IAudioAdapter`-t használja, hanem a JUCE `AudioDeviceManager`-t (`mt_audio_manager.cpp:73`: `AudioDevice(AUDIO_NUM_INPUTS=2, ..)`, `AudIO/src/juce_adapter.cpp:22,40-48`), amely az ALSA capture-t is megnyitja; a Looper/Sampler ezt fogyasztja. A `AkaiMpcKey61AudioAdapter` (playback-only) csak az `apps/mpc_gui` önálló programban él. Teendő eszközön: ellenőrizni, hogy a JUCE a `plughw:1,0` capture-t megnyitja-e (lásd T32 próbalista). Az `mpc_gui`-ban nincs felvétel-funkció, ott capture nem szükséges. (Felderítő ágens állítása, kódolvasás; nem ellenőrzött futtatással.)

### P2 – robusztusság és teljesítmény

- [x] **T16** kész és beolvasztva (`31b7d850`): a RawMIDI olvasó `poll()`-t használ (20 ms), minden írás kimenetenként egy mutexen megy; hardveren nem próbált (valódi rawmidi poll/revents, terhelés). Hátra: a `set_pad_rgb` Port 0-ra írása (T5).
- [x] **T17** kész és beolvasztva (`31b7d850`): `device_probe.h` (kártya, érintő-eszköz felderítés, `EVIOCGABS` tartomány), minden hibánál a régi `hw:0,0,x` / `plughw:1,0` / `event0` / 2048 tartalék. **Figyelem:** ha a valódi érintő-maximum nem 2048, a skálázás szándékosan megváltozik: hardveren ellenőrizendő; az `MPC` kulcsszó az első találatot veszi.
- [ ] **T18** Audio-adapter (`AkaiMpcKey61AudioAdapter`): a felderítés szerint ezt **csak az önálló `apps/mpc_gui` programban** használja a rendszer; a teljes Kick app a JUCE audioeszközt (`AudioDeviceManager`) használja, ezért a `buffer_size` paraméter, a 30 ms latency-kérés és a Phones/Main szétválasztás csak az `mpc_gui`-t érinti. Teendő csak akkor, ha az `mpc_gui` hangútja is cél: tényleges `buffer_size`, latency-cél egyeztetése a loggal, limiter a hard clip helyett. Eszközön mérendő.
- [ ] **T19** CV/Gate: a DAC-kód (`kick_I2c.cpp`: `/dev/i2c-3`, cím `0xC`, `0x30|ch` 3 bájtos írás) csak `KICK_ENABLE_HARDWARE=ON` és `hardwareEnabled()` mellett fut, az Akai-ról nincs dokumentált DAC/busz/cím. **Vakon nem írunk** ismeretlen I2C eszközre. Hardveres próba után dönthető el, hogy Linux-DAC vagy az STM32 (USB-MIDI/SysEx) viszi-e a CV-t. Hardveres próba 2026-11-01: I2C/CV-re nem írtunk; nem tesztelt. **Végső (2026-11-02):** nem tesztelve, szándékosan (az MPC-re I2C/CV-re nem írunk; a próbák `KICK_NO_HARDWARE=1`-gyel futnak).
- [x] **T20** kész és beolvasztva (`53d80738`): a ragadós tempo-szerkesztő mód megszűnt (a Kick UI nem fogyasztja az `EN_TAP_TEMPO`-t); Akai-konvenció szerint a Data Dial csak a TAP TEMPO fizikai lenyomása alatt állítja a BPM-et, a +/− mindig navigál.

### P3 – tesztelés

- [x] **T21** kész és beolvasztva (`31b7d850`): `akai_map.h` + `AkaiMapTests.cpp` (45 gomb-Note, 16 pad bijekció, 49 és 50 is SHIFT).
- [x] **T22** kész és beolvasztva (`31b7d850`): `rawmidi <port> <hex...>` a `/data/kick_cmd` FIFO-ban, ugyanazon a parseren/dispatchen át (`inject_bytes`); a parancsot futtatva nem próbáltam, csak fordul.
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
- [x] **T30** (TC rész kész és beolvasztva: `d1cd2421`, hardveren/UI-n nem próbált) Valódi Timing Correct: `mt_timing_correct.h` (tiszta logika + teszt), `MTAudioInterface::timingCorrect` (egy undo lépés), globális TC be/ki (Shift+TC közvetlenül az API-t kapcsolja, nincs DCL csomag), a List Edit TC-ablak vezérlői bekötve; a RECALL a flag szerint korrigál. Hátra: az „Events: Range” opció nincs támogatva (nincs tartomány-kijelölés); a flag és a paraméterek nem perzisztensek; rövidebb track mint a szekvencia esetén a Start-mód a szekvencia hosszával csavar; nincs TC LED (T13). **AUTOMATION (minimális, valódi):** track LEVEL lane (`mt_automation.h` + host-teszt), globális mód Off/Read/Write (`setAutomationMode`), Read lejátszás a tick szálon, Write felvétel a level-változásból, lane a track ValueTree-ben (`AUTOLANE`), a törlés undo-zható; AUTOMATION gomb: Read<->Write, +Shift Off, LED 3/4/0, DCL csomag nélkül. Hátra: a track PAN-nak nincs fogyasztója a motorban (nem automatizálható), nincs lane-szerkesztő UI, a felvétel nem undo-zható, hardveren/UI-n nem próbált. **AUTOMATION:** kész és beolvasztva (`990fdf44`), lásd T33.
- [x] **T33** Automation kész és beolvasztva (`990fdf44`): globális mód Off/Read/Write (nem perzisztens), **track szintű LEVEL** automation (a ValueTree-ben `AUTOLANE`, projektbe menti, max. 4096 pont); Read a tick-szálon alkalmazza a lane-t (`levelScaler` atomi), Write a `valueTreePropertyChanged(IDs::level)`-en rögzít (mixer, Q-Link, track view), vékonyítással; AUTOMATION gomb: Read↔Write váltás, Shift = Off, LED Read=3, Write=4. `mt_automation.h` + host-teszt; hardveren/hallgatással nem próbált. Hátra: a track-szintű **PAN nem automatizálható, ellenőrzött okból** (ezért a **program**-szintű pan lane készül, T35): a `MTTrack::setPan` / `MTITrackData::pan`-nak nincs hívója, a `getPan()` sosem olvasott, a `MTAudioInterface`-nek nincs track-pan setterje, a mixer pan-gombjai a program (`pProg->pan`) vagy a busz (`pBus->pan`) pan-jét írják; így Write nem tudna mit rögzíteni, Read nem tudna mit lejátszani (látszólag működő, valójában üres funkció lenne). A MIDI-program mixer-gombja (`MTIProgram::pan`) ma nem csinál semmit MIDI programnál (az eredeti Kick hiánya): ha kell, `MTMidiProgram::noteProcess` küldhetne CC10-et, és a lane program-szintű lenne (programot több track osztja, undo-zható állapotot ír a tick-szálról: külön tervezés); audio trackek nincsenek; nincs lane-szerkesztő UI; szekvencia-másolás nem másolja a lane-t (`mt_audio_manager_sequence.cpp`); lane-tickek nem skálázódnak track-hosszváltásra; Read módban a mixer csúszka a tárolt szintet mutatja, nem a lane értékét; a Write felvétel nem undo-zható; Read leállása után a `levelScaler` az utolsó lane-értéken marad. Hardveres próba 2026-11-01: `btn AUTOMATION` ×3 elfogadva, app stabil; a hatás nincs ellenőrizve, a LED nem értékelhető (riport 4. hiba). **Végső (2026-11-02):** az AUTOMATION gomb LED-je igazolt (zöld=Read, piros=Write, sötét=Off); az automation-lane működése hardveren nincs mérve.
- [ ] **T32** Eszközön: `echo probe > /data/kick_cmd` kiír és ment egy csak olvasó rendszerjelentést a `/data/kick_probe.txt`-be (`/proc/asound/cards`, `pcm`, `/proc/bus/input/devices`, `/etc/asound.conf`, `/dev/i2c-*`, `/dev/snd`, `/sys/bus/i2c/devices` nevek, és a `device_probe.h` választásai; nincs shell, ioctl, I2C megnyitás). A kimenet elküldése dönti el a T19-et és az audio-bemenet kérdését. A korábbi kézi parancslista továbbra is érvényes. Hardveres próba 2026-11-01: `probe` lefutott, kimenet: `docs/AKAI_MPC_KEY61_PROBE_RESULT.txt`; touch `range 2048x2048` (EVIOCGABS). **Végső (2026-11-02):** a `probe` lefutott, az ALSA kártya-fa is benne van (card0 = Akai USB, card1 = ACVM kodek); a kodek mixer-elemei nem listázódnak.

## 7. Képernyőkép-segéd (kész)

Headless képernyőképek a smoke-teszttel (Linux, `xvfb-run`; a projekt nincs betöltve, a képernyők üresek):

```bash
KICK_DATA_ROOT=/tmp/kd KICK_RESOURCE_ROOT=$PWD/Kick KICK_NO_AUDIO_DEVICE=1 KICK_NO_SERIAL_DEVICE=1 KICK_NO_HARDWARE=1 \
KICK_SMOKE_SHOTS_DIR=/tmp/shots KICK_SMOKE_SCREENS="HomeListView,HomeGridView,MenuPadMixer,MenuListEdit,HomePreferencesView" \
xvfb-run -a -s "-screen 0 1280x800x24" build-linux/apps/kick/Kick_artefacts/Release/Kick --smoke-test
```

Minden névhez egy `<név>.png` készül a `KICK_SMOKE_SHOTS_DIR`-ben (1,2 s várakozás képernyőnként). A név a `kick_ui::Screen` neve (HomeOptions, HomeListView, HomeGridView, HomeSequencerView, HomeXYView, HomePreferencesView, MenuSampleEdit, MenuProgramEdit, MenuSampler, MenuLooper, MenuPadMute, MenuTrackMute, MenuPadMixer, MenuChannelMixer, MenuKnobEdit, MenuListEdit, MenuPadPerform, MenuMidiControl, MenuFileBrowser, MenuNextSequence, MenuSong, MenuColorEdit, RecentProject, SystemResources); ismeretlen név esetén a visszatérési kód 2. A `KICK_SMOKE_SNAPSHOT` az induló képet menti, mint eddig. Kód: `Kick/apps/kick/Main.cpp` (`smokeScreenShots`).

## 8. MIDI program pan → CC10 (kész)

- [x] **T34** kész és beolvasztva (`def27ac2`): a MIDI program mixer PAN gombja CC10-et küld a program MIDI kimenetére (`MTMidiProgram::valueTreePropertyChanged`, `IDs::pan` ág; `panToCC10`: -1→0, 0→64, +1→127; azonos érték nem megy ki kétszer; `mt_midi_pan.h` + `MidiPanTests.cpp`). A natív build és 16 host-teszt rendben; hardveren nem próbált. **Ellenőrizendő:** (1) a mixer gomb valóban eléri a kezelőt, (2) projektbetöltéskor nem megy ki felesleges CC10. **Nem készült:** kezdő-CC10 lejátszáskor/betöltéskor (a `MTMidiProgram` a többi CC/Bank/ProgCH állapotot sem küldi újra); a pan-automation lane program-szintű: **folyamatban** (`pan-lane-automation` ág, T35). A track-pan automatizálás elhagyása NEM a felhasználó döntése volt (ő a CC10-et választotta); semmi nem maradhat ki.
