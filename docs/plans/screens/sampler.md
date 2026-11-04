# Sampler (felvétel) — review és TODO

Alap: **statikus kódolvasás** (olvasás kezdete: HEAD `40fb2db6`, ág `claude/kick-plans`, 2026-11-04; közben a Kick-kódot csak `168417a1` érintette — Looper-összeomlás-javítás —, az itt idézett fájlokat nem), semmi nem lett futtatva, hardveren nem mértem. Ahol a kód és a régi doksi ellentmond, a kód az igazság. Jelölések: `UI/` = `Kick/modules/kick_ui/audio_components/sampler/`, `REC/` = `AudIO/juce_modules/AudIO_juce/Recorder.{h,cpp}` (a felvevő/looper motor, `mt::Recorder`, `SampleRecorder`, `Looper`), `ENG/` = `Kick/modules/engine/audio_engine/engine/`, `bridge` = `Kick/apps/kick/kick_hardware_bridge.h`. Alap-dokumentumok: `research/screen-inventory.md` §4.10, `research/known-issues-and-roadmap.md` (H17), `docs/AKAI_MPC_KEY61_TODO.md` T15/T32, `docs/AKAI_MPC_KEY61_HW_TEST_CHECKLIST.md` §6-7, `KEY61_DOC_VS_KICK.md` #20, `WP-G.md`.

## 1. Áttekintés (mit csinál, hogyan lehet elérni, hardver-gombok kötése)

**Mit csinál.** Audio-bemenetről mintát vesz fel (mono/sztereó, 6 bemeneti mód, küszöb-indítás, max. hossz, monitor), majd négy üzemmódban dolgozza fel: **Sample** (egy minta), **Slice** (jelölők a felvétel közben: Slice+ gomb vagy pad), **Pad Tap** (padonként váltott start/stop-jelölő), **Pad Hold** (nyomva tartott pad = szelet). Mentés: Save / Edit (→ Sample Edit) a „KEEP OR DISCARD” ablakkal, Play (előhallgatás), Discard, Cancel. A felvétel a motor `SampleRecorder` objektumában él (24 bites WAV mentés, `REC/Recorder.cpp:119-134`).

**Elérés.**
- Hardver: **Shift + SAMPLE EDIT** (`bridge:1293-1300`, LED `bridge:280`); a gombpár a MPC-n mért (RPT:98-99, 234). `HW_BTN_SAMPLER` külön gomb a Key 61 térképen nincs (`akai_map.h`).
- Érintés: Menü csempe #9 (`M/kick_ui/dialogs/kick_AppMenuOptions.cpp:96`).

**Hardver-kötés a képernyőn** (`UI/kick_Sampler.cpp:360-516`, `UI/kick_SamplerRecorder.cpp:244-256`):
| Vezérlő | Hatás | Megjegyzés |
|---|---|---|
| PLAY / PLAY START / STOP | a **szekvencia** transzportja (`:505-515`) | a felvétel hardveres indítása/leállítása **nincs** |
| REC / OVERDUB gomb | a Sampler képernyőn **a szekvencia-felvételt élesíti** (`ae.sequenceRecPressed()`), mert csak a Looper képernyő/felvétel kapja őket: `bridge:241-245, 966-981` | váratlan: sample-felvétel helyett szekvencia-REC |
| Pad (Slice / Pad Tap / Pad Hold) | jelölők (`:362-488`); a pad-LED a programmal színez (`:518-546`) | lásd S1: a Slice/Pad Tap a **felengedésre** fut |
| Q-Link 1 / 2 | max. felvételi idő (nagy/kis lépés) — `kick_SamplerRecorder.cpp:244-256` | igen |
| Q-Link 3 | küszöb-csúszka (a „gain” feliratú vezérlő, lásd S9) — `kick_Sampler.cpp:497-500` | igen |
| Q-Link 5 | bemenet-lista léptetése (`kick_IOComponent.cpp` `knobCommand`) | **nem** — a Key 61 hídja 4 Q-Linket ad (`bridge:1529-1541`) |
| Data Dial | nincs kezelő (CENTER kizárva: `:493`) | a híd általános „utoljára érintett vezérlő” útja működhet — ezen a képernyőn NEM ELLENŐRIZHETŐ KÓDBÓL |

**Audio-bemenet.** A motor a JUCE `AudioDeviceManager`-en át 2 bemeneti csatornát kér (`ENG/mt_audio_manager.cpp:86-94`, `AUDIO_NUM_INPUTS 2`, `AUDIO_SAMPLE_RATE 48000`: `AudIO/AudIO/Includes/Constants.h:7,13`; `AudIO/src/juce_adapter.cpp:22`), a felvevő a „default stereo in” buszra csatlakozik (`ENG/mt_audio_recorder.cpp:44`). Hogy a MPC ALSA-eszköze (INPUT 1/2, +48 V) tényleg ad-e bemenetet: **NEM IGAZOLT** (`known-issues` H17; `HW_TEST_CHECKLIST.md:59`; probe-parancsok `:66-69`).

## 2. Funkciók és állapotuk

| Funkció | Állapot | Bizonyíték |
|---|---|---|
| Képernyő-nyitás (Shift+SAMPLE EDIT, menü), LED | KÉSZ (gombpár mért) | `bridge:280, 1293-1300`; RPT:98-99,234 |
| Felvétel indítás/leállítás képernyőgombbal, küszöb-indítás, max. hossz | KÉSZ (kód szerint) | `UI/kick_SamplerRecorder.cpp:30-37, 263-302, 344-364`; `REC/Recorder.cpp:77-92, 175-182` |
| Bemenet-választó (6 mód: L/R/L+R × mono/sztereó) | KÉSZ | `kick_Sampler.cpp:21-32`; `REC/Recorder.cpp:168-173` |
| Bemeneti mérő (RMS/peak) | KÉSZ | `kick_Sampler.cpp:22-24`; `kick_SamplerRecorder.cpp:413`; `REC/Recorder.cpp:191` |
| „Gain” csúszka | KÉSZ, de **küszöb**, nem bemeneti erősítés (a felvétel erősítése fix 1,0) | `kick_Sampler.cpp:34-36`; `kick_SamplerRecorder.cpp:349` |
| Monitor be/ki | KÉSZ | `kick_Sampler.cpp:41-43`; `REC/Recorder.cpp:174` |
| Monitor szint-csúszka | STUB (a Samplerben nincs bekötve; a Looperben igen) | `kick_Sampler.cpp:41-43` vs `looper/kick_LoopRecorder.cpp:78`; a motor `monitorGain` alapértéke 1,0 (`REC/Recorder.h:75`) miközben a csúszka 80-at mutat (`kick_IOComponent.cpp:20`) |
| Inserts panel | STUB (látszik, de nincs `onInsertsClicked`; a motorban nincs Sampler-insert interfész, csak Looper) | `kick_Sampler.cpp` (nincs); `looper/kick_Looper.cpp:75-78`; `ENG/mt_audio_recorder.cpp:79-89` |
| Hullámforma élő rajzolása, idő-kijelző, progress | KÉSZ (teljesítmény-gyanú, lásd S8) | `kick_SamplerRecorder.cpp:399-422` |
| Slice / Slice+ (jelölők időben) | RÉSZLEGES (jelölők nem látszanak a hullámformán; idő-alap: 30 Hz-es időzítő, lásd S3) | `kick_Sampler.cpp:84-89, 372-385`; `kick_SamplerRecorder.cpp:85-104` (nincs jelölő-rajz) |
| Pad Tap / Pad Hold szeletelés | BUGOS GYANÚ (S1, S11) | `kick_Sampler.cpp:386-487` |
| Pad-szín jelzés (piros/zöld váltás a Pad Tapban) | STUB (`if( 0 )` + `//TODO … GD side`) | `kick_Sampler.cpp:411-425` |
| Play (előhallgatás) | RÉSZLEGES (a gomb állapota nem tér vissza a lejátszás végén) | `kick_Sampler.cpp:95-100`; `kick_SamplerRecorder.cpp:51-57` (üres callbackek) |
| Cancel / Discard | KÉSZ | `kick_Sampler.cpp:70-82, 102-105` |
| Save / Edit ablak (KEEP OR DISCARD: név, program, pad, szelet-mentési típus) | RÉSZLEGES/BUGOS (S4-S6) | `kick_Sampler.cpp:195-259`; `kick_KeepDiscardSampleComponent.cpp:1-74` |
| Mentés fájlba (24 bit WAV, az eszköz mintavételi frekvenciáján) | KÉSZ; hiba nem jelenik meg | `REC/Recorder.cpp:119-134`; `kick_Sampler.cpp:226` (`saveRecording` void) |
| Header **Export** | STUB (üres ág; a gomb a felvétel első indításától engedélyezett marad) | `kick_Sampler.cpp:47-62` |
| Header **Clear** | STUB (csak kommentár) | `kick_Sampler.cpp:63-66` |
| Kimenet-választó (`outputChanged`) | n/a — a kimenet-lista el van rejtve (`showOutputs(false)`), a lambda üres | `kick_Sampler.cpp:19, 38-39` |
| Felvétel-hossz | RÉSZLEGES: UI max. 1200 s, a motor 600 s-ra vág; `//TODO` | `kick_SamplerRecorder.cpp:20-23, 344-354`; `REC/Recorder.cpp:66` |
| Mentés után szeletek Sample Edit-be töltése / asset-szeletek | KÉSZ kód szerint, de 44100 Hz-es konstansokkal és csonkítással (S2) | `kick_Sampler.cpp:261-335` |
| Holt kód | `Sampler::saveSample`, `SaveSampleDialog` (+ .h/.cpp) — sehol nem hívott | `kick_Sampler.cpp:183-193`; `kick_SaveSampleDialog.*`; `grep SaveSampleDialog` → csak a saját fájlok |
| Audio INPUT a MPC-n | **NEM IGAZOLT** | `known-issues` H17 |

## 3. Mi nem működik / hibák

**S1 (P0) A Slice és Pad Tap mód a pad FELENGEDÉSÉRE fut.** A Sampler saját enumja `PAD_PRESSED = 0, PAD_RELEASED = 1` (`UI/kick_Sampler.h:38-42`), a DCL szerződés viszont `BTN_RELEASED = 0, BTN_PRESSED = 1` (`Kick/modules/contracts/dcl/dcl_contracts.h:216-217`), és a híd a pad-ütést `BTN_PRESSED`-ként küldi (`bridge:1763`). A `padState == PAD_PRESSED` (`kick_Sampler.cpp:374, 388`) így a felengedésre igaz; a Pad Hold ág a helyes `BTN_*` konstansokat használja (`:467, 478`). Hatás: a jelölők a pad felengedésekor kerülnek be, a pad-lenyomás nem csinál semmit; a Slice/Pad Tap szeletelés időzítése eltolt. Repro: Slice mód → REC → pad nyomva tartva 1 s → a jelölő a felengedésnél.

**S2 (P0) A szelet-határok rossz mintavételi frekvenciával és csonkítva számolódnak.** `loopStart/End = idő × 44100` fix konstanssal (`kick_Sampler.cpp:266-267, 269, 314, 316, 320-321`), miközben az eszköz 48000 Hz-et kér (`Constants.h:7`) és a felvétel az eszköz frekvenciáján készül (`REC/Recorder.cpp:128`); a slice-határok mintákban értendők (`audio_sound.cpp`, `SLICE loopStart/loopEnd` = forrás-minták). Ráadásul `(int)*it * 44100` a **double → int kasztot a szorzás előtt** végzi (`:314, 316, 320`), vagyis a határok egész másodpercre csonkolnak. Hatás: a Slice-módban mentett szeletek ~8,8%-kal korábbra esnek és ≥ 1 s hibával; csak a pad-ágak (`:266-269`) csonkolás nélküliek, de ott is 44100.

**S3 (P0) A jelölő-időalap pontatlan, és a megállítás utáni idő „összemegy”.** A `getCurrentRecordTime()` a 30 Hz-es időzítő által frissített `timerControl` értéke (`kick_SamplerRecorder.cpp:319-322, 408-417`) → ±33 ms jitter + üzenetszál-késés. Továbbá a motor `onRecorderStopped` értesítése az `onRecorderLengthChanged(validFrames/rate)`-et is hívja (`REC/Recorder.cpp:150`), ami az UI-ban `_recordLength`-et a *tényleges* hosszra írja (`kick_SamplerRecorder.cpp:39-41`); az időzítő ezután `timerControl.setTime(_progress * _recordLength)`-et ír (`:416`), ahol `_progress` a *teljes puffer* arányában marad (`REC/Recorder.cpp:192-193`) → megállítás után a kijelzett és a `getCurrentRecordTime()` által adott idő a valódi felvételi idő töredékére esik. Ezt használja az utolsó szelet vége (`kick_Sampler.cpp:269, 321`). Futtatással nem igazoltam → **MPC-n/hosztón ellenőrizendő** (felvétel 5 s-ra elkészítve, megállítás 2 s-nál, majd Save + Slice).

**S4 (P0) A mentett fájl a projekt-mappán KÍVÜL landol, és a név-ütközés kezelése hibás.**
 a) `ae.getProjectDir().getSiblingFile("/newSample.wav")` / `…getSiblingFile(fileName + ".wav")` (`kick_Sampler.cpp:187, 224-225`): a `getProjectDir()` a projekt *könyvtára* (`ENG/mt_audio_manager.cpp:228`; `engine/project/kick_xml_manager.cpp:192-208`), a `getSiblingFile` ennek a **szülőmappájába** ír (pl. `/data/projects/New Sample.wav`, nem a `…/<projekt>/samples/`-be). A projekt-mentő a kívüli mintákat a *mentett másolatba* átmásolja (`kick_xml_manager.cpp:112-130`), az élő fa útvonala viszont abszolút marad (`:43-47` komment).
 b) `Sampler::getValidFileName` Windows-útvonalat épít (`base + "\\" + file`, `kick_Sampler.cpp:287`) → POSIX-on ütközéskor a visszaadott név `projects\New Sample 1.wav` alakú (a szülőkönyvtár neve + visszaper a fájlnévben). A Looper export is ezt hívja (`looper/kick_Looper.cpp:53-54`).
 c) A motor mintagyorsítótára és az asset-térkép **fájlnév-egyezésre is visszaesik** (`audiofile_manager.cpp:57-66` `exists()`; `kick_xml_manager.cpp:43-47` komment) → két azonos nevű felvétel (más mappából) egymást felülírhatja a memóriában.
 Repro: kétszer menteni „New Sample” néven; a fájlrendszeren és a Sample Edit listában ellenőrizni.

**S5 (P0) Hardveren a felvétel nem vezérelhető.** REC → szekvencia-REC élesítés (`bridge:966-981`, mert a Sampler nem „looper”: `bridge:241-245`), STOP → szekvencia-leállítás (`kick_Sampler.cpp:513`). Felvétel indítása/leállítása csak képernyő-gombbal megy; REC-LED és a REC gomb viselkedése a Samplerben nincs definiálva.

**S6 (P1) A mentési folyamat hibái.**
 - Slice mód, „With Non-Destructive Slices” (`SliceSaveType == 1`): létrejön egy **üres Drum program** (`createProgram`), szelet-hozzárendelés csak a „With Pad Parameters” (2) esetén történik (`kick_Sampler.cpp:236-246`).
 - Ha nincs program (`programs.getSize() == 1`) a „CREATE NEW PROGRAM” választó **Sample módban is megjelenik** (`kick_KeepDiscardSampleComponent.cpp:53-58`), de Sample módban a `getSliceSaveTypeIndex()`-et senki nem használja (`kick_Sampler.cpp:230-246`).
 - A program-lista kijelölése már *Apply előtt* átállítja a motor kiválasztott programját (`kick_KeepDiscardSampleComponent.cpp:47-51`), Cancel nem állítja vissza. A „Root Note” választó semmire nem hat.
 - Pad Tap / Pad Hold mentés: `addSlicesToPads` a kiválasztott programba ír program-típus ellenőrzés nélkül (`kick_Sampler.cpp:272`).
 - **Minden** mentés betölti a mintát a Sample Editorba (`kick_Sampler.cpp:229, 297`) → a Sample Editor `setSource` a **teljes projekt undo-történetét törli** (`ENG/mt_sample_editor_interface.cpp:361-369`; SE-13).

**S7 (P1) Audio-bemenet: igazolatlan és hibája láthatatlan.** `AudioDevice::start` az `error` stringbe írja a hibát, és `stopped` állapotba tér (`AudIO/src/juce_adapter.cpp:22-24`), de az UI-nak nincs hibajelzése (04-screen-audit §1: „hangeszköz-hibát semmi nem olvassa”). Ha a bemenet 0 csatornás, a felvevő csendet kap, a mérő nem mozdul, hibaüzenet nincs. A hordozott 48 kHz/2 csatorna feltevést a MPC ALSA-eszköze nem biztos, hogy teljesíti (NEM ELLENŐRIZHETŐ KÓDBÓL).

**S8 (P1) Hullámforma-frissítés és memória: dropout- és OOM-gyanú.** `setThumbnail()` 30 Hz-en `ae.getRecorderAudioBuffer()`-t hív felvétel közben (`kick_SamplerRecorder.cpp:399-411`), ami a **teljes eddigi felvételt lemásolja** egy `SpinLock` alatt (`REC/Recorder.cpp:107-112`); az audio-callback a zárat csak `TryLock`-kal veszi, és ha nem sikerül, `clearOutputs(); return;` (`:159-160`) → a másolás idejére a bemeneti blokkok **kimaradnak a felvételből**. Hosszú felvételnél (pl. 60 s sztereó ≈ 23 MB / 33 ms) ez hallható kattanás/hiány lehet. Emellett minden indításkor `history.makeCopyOf(audio)` a teljes (előre lefoglalt) pufferről (`:78`); 600 s × 48 kHz × 2 csatorna × 4 B ≈ 230 MB + másolat. A MPC RAM-ja NEM ELLENŐRIZHETŐ KÓDBÓL. Ráadásul a `setupRecorder` a régi mód csatornaszámával foglal, majd az `startRecorder` után újra (`ENG/mt_audio_recorder.cpp:91-101, 147-157`; `REC/Recorder.cpp:86-90`) → dupla allokáció.

**S9 (P1) Félrevezető vezérlők.** A „gain” csúszka valójában a rögzítési **küszöb** (`kick_Sampler.cpp:34-36` → `setRecorderThresholdDB`), bemeneti erősítés nincs; a Monitor-szint csúszka hatástalan (monitor 100%-os, visszacsatolás-kockázat), az Inserts panel halott, az Export/Clear gombok engedélyezve, de üresek (`:47-66`; csak az első indításkor engedélyeződnek, soha nem tiltódnak). Az Export/Clear/Output/Inserts a 04-audit „Sampler Export/Clear” tétele.

**S10 (P1) Preview állapot és hosszú-távú élet.** A Play gomb a lejátszás végén nem áll vissza (`onRecorderPreviewStopped` üres: `kick_SamplerRecorder.cpp:55-57`). A képernyő elhagyása felvétel közben nem állítja le a felvételt (`kick_SamplerRecorder.cpp:380-398` csak az időzítőt és a monitort), visszatéréskor `recorder.setRecordMode(mode)` az `armed` (`recordModeRecord`) állapotban **újraindítja** a felvételt (`kick_Sampler.cpp:578` → `kick_SamplerRecorder.cpp:277-283`).

**S11 (P2) Pad Tap/Hold korlátok.** Bankváltás közben a megkezdett jelölő vége elveszik (`kick_Sampler.cpp:141-151, 438-445` → `endSamplePoints` hiányzik → `operator[]` 0 → `loopEnd = 0` az `addSlicesToPads`-ban `:261-275`); Pad Hold-nál a felvétel leállítása a pad felengedése előtt ugyanígy 0 végpontot ad; a szelet-sorszám pad-sorrend szerinti (`std::map`), nem időrend; a Pad Hold ág nem ellenőrzi a pad-tartományt (`:462-487`).

**S12 (P2) Egyéb.** `MTAudioRecorderManager::setup` a Sampler-fa csomópontot a `looperNode` alá hozza létre (`ENG/mt_audio_recorder.cpp:86-88`) — felesleges `SAMPLER` ág a projektben. `Sampler::lastPressedPad` statikus tag (több példány nem lehet).

## 4. Tervben / hiányzik (az Akai MPC Standalone-hoz képest, a docs-ból ismert tervek)

- **A v3.10 Sampler/Auto Sampler fejezet („WP-F”, p. 267–303) áttekintése NINCS elvégezve** (`WALKTHROUGH_PLAN.md:24`; `known-issues` §3.1). Az Akai-paritási lista ezért **NEM ELLENŐRIZHETŐ a repóból**; WP-F = SE-30.
- Dokumentált, nyitott: **audio bemenet** (INPUT 1/2, +48 V) a Samplerben igazolatlan — `known-issues` H17; `AKAI_MPC_KEY61_TODO.md` T15 („felderítve, kód nem kell… eszközön ellenőrizni”), T32 (probe: `echo probe > /data/kick_cmd` → `/data/kick_probe.txt`); `HW_TEST_CHECKLIST.md` §6, §7 (`arecord -D plughw:1,0 …`). A +48 V vezérlésének útja a repóban nincs (NEM ELLENŐRIZHETŐ).
- **WP-G G-186**: a Sampler Save/Save ablakai ELTÉRnek az útmutatótól (`WP-G.md:194`). A mentés-hely választó (Storage: Internal/USB/SD) nincs a mentő-ablakban (G-197, `WP-G.md:205`).
- `screen-inventory` §6/7: Sampler Export/Clear/Output STUB; pad-szín `if(0)`; felvétel-hossz `//TODO` — ld. a fenti hibákat.
- A Kick **saját UI-ja** már jelzi a tervezett funkciókat: Export/Clear, Output-választó, pad-szín-váltás (GD oldali színek), felvétel-hossz paraméterezés, Sampler-insert.
- Nem találtam tervet: Auto Sampler, felvétel előtti puffer (pre-roll), belső busz felvétele (resample), count-in, normalizálás/vágás mentéskor (WP-F ellenőrzendő).

## 5. Javaslataim (improvementek, indoklással)

1. **Mintapontos jelölők a motorból.** A felvevő tegyen közzé atomi írási pozíciót (kereteket) és egy lock-free jelölő-FIFO-t; a pad/Slice+ esemény időbélyegét a jelölő-FIFO-ba kapja a **keret-pozícióval**, az UI a felvétel *tényleges* mintavételi frekvenciájával számoljon. Ez megszünteti S2/S3 gyökerét (30 Hz timer, 44100 konstans, csonkítás) és a megállítás utáni „összemenést”.
2. **Hullámforma-gyorsítótár a felvevő oldalán** (min/max csúcs-lista, pl. 256 keretenként), amit az UI olvas — nincs teljes puffer-másolat, nincs zár-versengés (S8); a `history` mentés csak overdubnál/undo-nál kell, ne minden indításnál.
3. **Pre-roll gyűrűpuffer** (pl. 100–500 ms) küszöb-indításhoz: ma a felvétel a küszöböt átlépő mintától indul (`REC/Recorder.cpp:176-181`), a tranziens eleje elveszhet. Szokásos sampler-viselkedés; mérni kell a memória/CPU-költséget a MPC-n.
4. **Bemeneti út láthatósága**: eszköz-állapot csík (nyitott bemeneti csatornák, mintavételi frekvencia, hiba-szöveg az `AudioDevice::error`-ból), „NO INPUT” jelzés a mérőn; **bemeneti trim** külön a küszöbtől; clip-jelzés; a Monitor szintje bekötve + alapértelmezésben halk/ki a visszacsatolás ellen.
5. **Egységes mentő-folyamat**: egy ablak (név, cél-program/pad, szelet-mód), írás a `<projekt>/samples/`-be egyedi névvel (`juce::File::getNonexistentSibling`), mentés-hiba megjelenítése (`Recorder::lastError()` ma senkihez nem jut el), progress hosszú fájlnál; **Export** = fájl a tárolóra (USB/SD), **Clear** = megerősített elvetés.
6. **Hardver-kötés**: a Sampler képernyőn REC = felvétel armed/start, STOP = stop, PLAY = előhallgatás; a Q-Link 1–4: max. hossz, küszöb, bemeneti trim, monitor; a REC-LED a Sampler állapotát követi. A híd `recOverdubGoesToLooper()` helyett képernyőnkénti irányító-tábla.
7. **Jelölők láthatóvá tétele** a hullámforma fölött (szám + törlés/undo utolsó), érintésre mozgatható; pad-szín (piros=felvétel alatt, zöld=kész) a már létező `setPadDataMessage` hívásokkal (`kick_Sampler.cpp:414-425`).
8. **Tesztelhetőség**: a szelet-számítás, a fájlnév-képzés és a pad-állapotgép legyen GUI-mentes függvény (`kick_sampler_slices.h`-szerű) → host-teszt; a felvevő köré stressz-teszt (párhuzamos `getAudioBuffer` + `process`) a dropout-mérésre.

## 6. Hardveres és unit-tesztek

**Ma fedi:** a felvevő motor host-tesztje az `Kick/tests/platform/AudioTests.cpp:32-126`-ban: rögzítés pontos puffer-határig, sztereó/mono-downmix, küszöb-indítás, overdub/undo/redo, ellenkező irány, mentés (tranzakciós, hiba utáni felépülés), mintavételi-frekvencia-váltás, allokáció-mentesség (`forbidAllocation`). **Nem fedi:** a Sampler UI-logikáját (pad-állapot, szelet-számítás, `getValidFileName`, mentési folyamat), a `getAudioBuffer`/`process` zár-versengést, a hardver-útvonalat. Hardveren csak a gombpár nyitása igazolt (RPT:98-99, 234); a Sampler-felvétel, a bemenet, a pad-szeletelés **nem mért**.

**A MPC-n ellenőrzendők:**
- [ ] `echo probe > /data/kick_cmd` → `/data/kick_probe.txt`, `arecord -l`, `arecord -D plughw:1,0 --dump-hw-params` (T32): van-e capture-eszköz, hány csatorna, milyen frekvencia(k).
- [ ] INPUT 1/2 jelzés eléri-e a mérőt (L, R külön); +48 V; hallható monitor késleltetés; monitor-visszacsatolás.
- [ ] Mintavételi frekvencia-egyezés (48000?) — mentett WAV fejléc vs. szeletek helye (S2).
- [ ] Mind a 6 bemeneti mód; mono/sztereó fájl-csatornaszám; küszöb-indítás különböző küszöbökkel.
- [ ] 20 s / 5 perc / 10 perc felvétel: dropout (hallható hiány a mentett fájlban), RAM-használat, UI-reakció, hullámforma-frissítés (S8).
- [ ] Slice / Pad Tap / Pad Hold: pad-lenyomás vs. -felengedés (S1); jelölők helye a mentett szeleteken (S2/S3).
- [ ] Save/Edit: hova kerül a fájl (S4); ismételt mentés azonos névvel; a mentés utáni undo-történet (S6).
- [ ] Hardveres REC/STOP/PLAY a Sampler képernyőn (S5).
- [ ] Képernyő elhagyása felvétel közben (S10).
- [ ] Hosszú (> 5 s) felvétel padhoz rendelve — teljes hosszban szól-e (5 s-os cache-korlát: SE-12).

## 7. TODO

Hibák (sorrendben):
- [ ] **SA-01** (P0, S) A Sampler pad-állapot enumja helyett a DCL `BTN_PRESSED/BTN_RELEASED` használata (Slice, Pad Tap); a pad-állapotgép GUI-mentes függvénybe — `UI/kick_Sampler.h:38-42`; `kick_Sampler.cpp:374, 388`.
- [ ] **SA-02** (P0, M) Mintapontos jelölők: motoroldali atomi írási pozíció + jelölő-FIFO, az UI a felvétel tényleges mintavételi frekvenciájával számol, a csonkítás megszűnik; `_recordLength`/`onRecorderLengthChanged` mellékhatás javítása — `kick_Sampler.cpp:261-335`; `kick_SamplerRecorder.cpp:39-41, 319-322, 408-417`; `REC/Recorder.cpp:143-157, 192-193`. Függ: SA-01.
- [ ] **SA-03** (P0, M) Mentési hely és egyedi fájlnév: `<projekt>/samples/`, `getNonexistentSibling`; `getValidFileName` eltávolítása; ugyanez a Looper exportra (LO-07 hivatkozik rá) — `kick_Sampler.cpp:187, 224-225, 277-291`; `looper/kick_Looper.cpp:53-54`; a cache fájlnév-visszaesés kezelése (SE-12 mellett): `audiofile_manager.cpp:57-66`.
- [ ] **SA-04** (P0, M) Hardveres REC/STOP/PLAY a Sampler képernyőn, képernyőnkénti gomb-irányító tábla a hídban; REC-LED — `kick_Sampler.cpp:505-515`; `bridge:241-245, 966-981`. Függ: SA-01.
- [ ] **SA-05** (P1, M) Mentési folyamat javítása: „With Non-Destructive Slices” ne üres programot hozzon létre; a „Create new program” választó csak Slice módban; a kiválasztott program ne változzon Apply előtt; Root Note bekötése vagy törlése; program-típus ellenőrzés Pad Tap/Hold mentésnél — `kick_Sampler.cpp:222-259`; `kick_KeepDiscardSampleComponent.cpp:47-59`.
- [ ] **SA-06** (P1, M) Audio-bemenet út: eszköz-állapot/hiba megjelenítése az UI-n (`AudioDevice::error`, nyitott csatornák, mintavételi frekvencia), „NO INPUT” jelzés; a hardveres igazolás (T32 probe + mérések) eredményének bevezetése a dokumentumokba — `AudIO/src/juce_adapter.cpp:9-28`; `known-issues` H17. **Hardver-függő.**
- [ ] **SA-07** (P1, L) Felvevő motor: lock-free hullámforma-gyorsítótár (min/max), a teljes puffer-másolat megszüntetése a UI-ciklusban, `history` csak overdub/undo-hoz, az `audio.setSize` dupla allokáció megszüntetése — `REC/Recorder.cpp:64-90, 107-112, 159-160`; `kick_SamplerRecorder.cpp:399-411`. Közös a Looperrel (LO-06).
- [ ] **SA-08** (P1, S) Max. felvételi hossz: UI (1200 s) és motor (600 s) összehangolása, RAM-keretből számolt korlát, a lefoglalt memória kijelzése — `kick_SamplerRecorder.cpp:20-23`; `REC/Recorder.cpp:66`.
- [ ] **SA-09** (P1, M) Header Export (fájl a tárolóra) és Clear (megerősített elvetés) implementálása, engedélyezési logika (ma csak bekapcsol) — `kick_Sampler.cpp:47-66`.
- [ ] **SA-10** (P1, S) Play-gomb állapot visszaállítása (`onRecorderPreviewStopped`), Monitor szint bekötése és biztonságos alapérték, Inserts panel bekötése vagy elrejtése (a motorban Sampler-insert interfész kell), bemeneti trim a küszöb mellé; a képernyő-elhagyás/visszatérés ne indítsa újra a felvételt — `kick_Sampler.cpp:41-43, 95-100, 547-579`; `kick_SamplerRecorder.cpp:51-57, 277-283, 380-398`.
- [ ] **SA-11** (P1, S) Mentési hibák megjelenítése (`Recorder::lastError`, `saveRecording` visszatérés), progress — `REC/Recorder.cpp:119-134`; `kick_Sampler.cpp:226`. Közös a Looperrel (LO-07).
- [ ] **SA-12** (P1, S) A Sampler mentés ne töltse feleslegesen a mintát a Sample Editorba (csak Edit / Slice esetén), és ne törölje az undo-t — `kick_Sampler.cpp:229, 297`; függ: SE-13.
- [ ] **SA-13** (P2, M) Pad Tap/Hold robusztusság: bankváltás/hiányzó végpont/időrend szerinti szelet-sorszám, a pad-szín váltás (`if(0)` ág) — `kick_Sampler.cpp:386-487, 261-275`. Függ: SA-02.
- [ ] **SA-14** (P2, S) Holt kód: `Sampler::saveSample`, `SaveSampleDialog`; a stray `SAMPLER` fa-csomópont — `kick_Sampler.cpp:183-193`; `ENG/mt_audio_recorder.cpp:86-88`.

Hiányzó funkciók:
- [ ] **SA-15** (P2, M) Pre-roll gyűrűpuffer a küszöb-indításhoz (mérés a MPC memóriájával) — `REC/Recorder.cpp:175-182`. Függ: SA-07.
- [ ] **SA-16** (P2, M) Jelölők a hullámformán + utolsó jelölő törlése + érintéses mozgatás — `kick_SamplerRecorder.cpp:85-104`. Függ: SA-02.
- [ ] **SA-17** (P2, L) Auto Sampler / resample (belső busz) / további bemenet-források — **függ: SE-30 (WP-F)**.
- [ ] **SA-18** (P2, S) Key 61 4-Q-Link kiosztás (max. hossz, küszöb, bemeneti trim, monitor); a 5-ös Q-Link hivatkozás kivezetése — `kick_Sampler.cpp:489-503`; `kick_IOComponent.cpp` `knobCommand`.

Tesztek:
- [ ] **SA-19** (P1, M) Host-teszt: szelet-számítás (frekvencia-független, nincs csonkítás), fájlnév-képzés (ütközés), pad-állapotgép (`BTN_*`) — új `Kick/tests/platform/SamplerLogicTests.cpp`. Függ: SA-01…SA-03.
- [ ] **SA-20** (P1, M) Stressz-teszt a felvevőn: párhuzamos `getAudioBuffer` + `process` (kimaradt blokkok számlálása) — `Kick/tests/platform/AudioTests.cpp` bővítése. Függ: SA-07.
- [ ] **SA-21** (P1, M) MPC-s ellenőrző-lista (§6) felvétele a `HW_TEST_CHECKLIST.md`-be és lefuttatása; az eredmény a `known-issues` H17 frissítésével.
