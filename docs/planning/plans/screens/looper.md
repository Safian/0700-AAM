# Looper — review és TODO

Alap: **statikus kódolvasás** (olvasás kezdete: HEAD `40fb2db6`; közben az ág `168417a1`-gyel a Looper megnyitási összeomlását javította, lásd §3 L0; a sorszámok az új állapotra vonatkoznak, 2026-11-04), semmi nem lett futtatva, hardveren nem mértem. Ahol a kód és a régi doksi ellentmond, a kód az igazság. Jelölések: `UI/` = `Kick/modules/kick_ui/audio_components/looper/`, `REC/` = `AudIO/juce_modules/AudIO_juce/Recorder.{h,cpp}` (a `mt::Looper` a `mt::Recorder` leszármazottja), `ENG/` = `Kick/modules/engine/audio_engine/engine/`, `bridge` = `Kick/apps/kick/kick_hardware_bridge.h`. Alap-dokumentumok: `research/screen-inventory.md` §4.11, `research/known-issues-and-roadmap.md` (H7, H17), `docs/AKAI_MPC_KEY61_MORNING_DECISIONS.md` (REC/OVERDUB routing + LED), `docs/AKAI_MPC_KEY61_HW_TEST_REPORT.md:215`, `docs/TODO_MASTER.md` H2, `KEY61_DOC_VS_KICK.md`.

## 1. Áttekintés (mit csinál, hogyan lehet elérni, hardver-gombok kötése)

**Mit csinál.** Egy-sávos, ütem-hosszúságú (1–16 ütem: `kick_Constants.h:37`) hurok-felvevő a külső bemenetről: bemeneti mód (6 db L/R/L+R × mono/sztereó), küszöb-indítás, monitor, REC (csere-felvétel), OVERDUB, PLAY/STOP, Reverse, kimeneti Gain/Pan, SEQ-szinkron (a szekvenciával együtt indul), 4 insert (külön képernyő), hullámforma + lejátszófej + ütem-skála, Export (WAV-fájl + opcionális pad-hozzárendelés) és Clear. A hurok-hossz másodpercben a kiválasztott szekvencia tempója szerint számolódik (`UI/kick_LoopRecorder.cpp:220-224`, `MTTempoMgr::getSecondsUpto`).

**Elérés.**
- Érintés: Menü csempe #13 (`M/kick_ui/dialogs/kick_AppMenuOptions.cpp:101`).
- Hardver: **a Key 61-en nincs Looper-gomb** — az Akai-térkép és a hardver-mappa nem tartalmaz Looper-hivatkozást (`grep LOOPER Kick/apps/kick/hardware` → 0 találat); az `EN_LOOPER` csak a desktop billentyű-térképből jön (`M/contracts/utils/keyboard/keyboard_detector.cpp:123`, kezelő: `kick_AppHome.cpp:511`). A MPC-n a képernyő csak a menün át érhető el.

**Hardver-kötés a képernyőn** (`UI/kick_LoopRecorder.cpp:163-192`, `bridge:241-262, 966-990`):
| Vezérlő | Hatás | Megjegyzés |
|---|---|---|
| REC | `record(false)` — a Looper képernyőn (vagy ha a looper épp felvesz) a looper kapja (`recOverdubGoesToLooper`, `bridge:241-245, 974-975`) | más képernyőn a szekvencia-felvételt élesíti |
| OVERDUB | `record(true)` (`:174`) | ugyanez |
| PLAY / PLAY START | `play(false/true)` (`:171-172`) | SYNC esetén a szekvenciát is újraindítja |
| STOP | `stop()` (`:170`) | SYNC esetén `setSequencePause()` |
| Q-Link 3 | a küszöb-csúszka (`options.knobRotated`, `:185-188`) | igen |
| Q-Link 5 | bemeneti mód léptetése | **nem** — a Key 61 hídja 4 Q-Linket ad (`bridge:1529-1541`) |
| Data Dial | nincs kezelő (`knob != EN_KNOB_CENTER` szűrés `:181`) | ezen a képernyőn NEM ELLENŐRIZHETŐ KÓDBÓL |
| REC / OVERDUB LED | a looper állapotát követi, amíg felvesz (`bridge:250-262`; `hardware/led_map.h:157-185`) | lásd L2 |

A hardveres REC-hez tartozó engine-viselkedés (a szekvenciafelvétel és a looper szétválasztása) `MD:75, 90`-ben döntésként rögzítve; a Looper-LED és a REC-gomb útvonala a MPC-n nem lett kamerával ellenőrizve (`MD:90`, `known-issues` §1: „felhasználó szerint jó”).

## 2. Funkciók és állapotuk

| Funkció | Állapot | Bizonyíték |
|---|---|---|
| Képernyő megnyitása menüből | KÉSZ (az összeomlás javítva: 168417a1; MPC-n a javított build ellenőrzése a commit szerint következő lépés) | `kick_AppMenuOptions.cpp:101`; `UI/kick_LoopRecorder.cpp:276-282` |
| Hardveres megnyitás | HIÁNYZIK (nincs Key 61 gomb) | `keyboard_detector.cpp:123`; `grep LOOPER hardware` |
| Bemeneti mód (6) | KÉSZ (mellékhatás: csatornaszám-váltás a következő indításkor törli a hurkot) | `UI/kick_LoopRecorder.cpp:75`; `REC/Recorder.cpp:86-90, 168-173` |
| Hossz (ütem) | BUGOS GYANÚ (L3) | `UI/kick_LoopRecorder.cpp:85-89, 220-224, 429-436` |
| REC (csere-felvétel) | RÉSZLEGES (L2: a sticky overdub-flag miatt overdubként viselkedhet; körbeér és felülír, nincs egymenetes mód) | `UI/kick_LoopRecorder.cpp:94-98, 138-161`; `REC/Recorder.cpp:77-85, 175-182` |
| OVERDUB | **BUGOS** (L1, L2) | `UI/kick_LoopRecorder.cpp:100-103, 144-146`; `REC/Recorder.cpp:175-187` |
| PLAY / STOP | KÉSZ | `UI/kick_LoopRecorder.cpp:105-136`; `REC/Recorder.cpp:93-102` |
| SEQ-szinkron | RÉSZLEGES (fázis-mérés hiányzik: H7/Z2b; AC2) | `UI/kick_LoopRecorder.cpp:115-127, 133-134, 150-157`; `known-issues` H7; `TODO_MASTER.md:62` |
| Reverse | RÉSZLEGES (csak lejátszás közben engedett; felvételkor a kód FORWARD-ra állít, a gomb állapota marad; az állapot-visszatükrözés elírt) | `UI/kick_LoopRecorder.cpp:91, 159, 367-370` |
| Gain / Pan (kimenet) | **BUGOS** (L4: kétszer alkalmazódik) | `UI/kick_LoopRecorder.cpp:81-82`; `REC/Recorder.cpp:163-172, 184` |
| „Gain” csúszka (opciók) | KÉSZ, de **küszöb**, nem erősítés | `UI/kick_LoopRecorder.cpp:76`; `REC/Recorder.cpp:176` |
| Monitor + szint | KÉSZ | `UI/kick_LoopRecorder.cpp:77-78, 424-428`; `REC/Recorder.cpp:174` |
| Inserts (4) | KÉSZ | `UI/kick_Looper.cpp:75-78, 119-123`; `ENG/mt_audio_recorder.cpp:79-84` |
| Hullámforma + lejátszófej | KÉSZ (teljesítmény-gyanú: L5) | `UI/kick_LoopRecorder.cpp:194-198, 387-390, 450-465` |
| Ütem-skála | RÉSZLEGES (a Project Defaults számláló/nevező fogyasztója, nem a szekvencia taktusa; fix 4 beat/ütem a szélesség-számításban) | `UI/kick_LoopRecorder.cpp:244-246, 260-281`; `04-screen-audit.md` §8 |
| Export (név + program/pad) | RÉSZLEGES/BUGOS (L6) | `UI/kick_Looper.cpp:47-67`; `UI/kick_LoopRecorder.cpp:208-212`; `REC/Recorder.cpp:119-134` |
| Clear | KÉSZ (megerősítés nélkül) | `UI/kick_Looper.cpp:69-73`; `UI/kick_LoopRecorder.cpp:200-206` |
| Export/Clear engedélyezés | BUGOS GYANÚ (L7) | `UI/kick_Looper.cpp:41-44`; `UI/kick_LoopRecorder.cpp:97, 111, 163-176` |
| Undo / Redo | STUB (engine kész és tesztelt; UI/hardver hívó nincs) | `ENG/mt_audio_interface_looper.cpp:79-80`; `REC/Recorder.cpp:113-118`; `grep looperUndo` → 0 hívó |
| Egymenetes felvétel N ütemig | HIÁNYZIK (a felvétel körbeér és felülír) | `REC/Recorder.cpp:181`; `AudioTests.cpp:99-102` („records continuously across wrap”) |
| Looping ki/be (`setLooperLooping`) | n/a — nincs UI-hívó, mindig hurok | `grep setLooperLooping` |
| Hang a képernyőn kívül | tervezési korlát: a looper a képernyő elhagyásakor leáll és letiltódik | `UI/kick_LoopRecorder.cpp:435-446` |
| REC/OVERDUB LED | RÉSZLEGES (L2 mellékhatás) | `bridge:250-262`; `hardware/led_map.h:157-185` |
| Audio INPUT a MPC-n | **NEM IGAZOLT** | `known-issues` H17 |

## 3. Mi nem működik / hibák

**L0 (javítva, MPC-igazolás hátra) A Looper megnyitása összeomlott friss `/data`-n.** `ppqResolution = 960 / (denominator / 2)` osztott nullával (üres Project Defaults → nevező 0), ARM-on SIGFPE; a Mac-smoke nem mutatta. Javítva: `168417a1` (`UI/kick_LoopRecorder.cpp:281`, `jmax(1, …)`); a commit szerint a javított build MPC-s ellenőrzése a következő lépés. Tanulság: a skála a Preferences-ből olvas (L9).

**L1 (P0) Overdub közben a meglévő hurok NEM szól.** A `Recorder::process` felvétel és lejátszás ágai kizárják egymást: `if (recording …) {…} else if (preview …) {…}` (`REC/Recorder.cpp:175-187`); felvétel/overdub alatt a hurok tartalma nem kerül a kimenetre, csak a monitor (`:174`). A host-teszt csak a puffer-tartalmat ellenőrzi („loop overdub retains the previous cycle”, `AudioTests.cpp:106-108`), a hallhatóságot nem. Hatás: az OVERDUB használhatatlan — a játékos nem hallja, mire játszik rá. Repro: REC egy ütemet → OVERDUB → hurok némán fut felvétel közben.

**L2 (P0) Az overdub-flag soha nem törlődik.** Az UI csak `setLooperOverdub(true)`-t hív (`UI/kick_LoopRecorder.cpp:145`); `false`-t sehol (grep). A `replaceB` kikapcsolása csak `setLooperPlay()`-t hív (`:100-103`). Következmények: (a) az első OVERDUB után a sima REC is overdubként ír (`REC/Recorder.cpp:178`), (b) a REPLACE gomb a következő időzítő-tickben visszaáll aktívra (`:372`), (c) a **szekvenciafelvétel elindítása letiltódik**: `looperBusy = isRecording() || isOverdubbing()` (`ENG/mt_audio_manager.cpp:625`) → az élesített szekvencia „Armed” marad, take nem indul (`mt_seq_record.h:65-72`) a program újraindításáig, (d) a REC-LED a looper-REC alatt overdub-színt mutat (`hardware/led_map.h:161-162`). Repro: Looper → OVERDUB → kilépés → szekvencia REC + PLAY → nem kezd take-et.

**L3 (P0) Képernyőre lépéskor a hurok törlődhet.** `visibilityChanged`: `if (!approximatelyEqual<double>(getRecordingLength(numBars), ae.getLooperLength())) setRecordingLength(numBars)` (`UI/kick_LoopRecorder.cpp:433-436`); a `getLooperLength()` **float** (`REC/Recorder.h:41`), a másik oldal double. Nem float-pontos hosszoknál (pl. 100 BPM, 4 ütem = 9,6 s) az összehasonlítás hamis → `setLength` → `audio.clear(); validFrames = 0` (`REC/Recorder.cpp:64-76`): a felvett hurok minden képernyő-belépéskor elvész. 120 BPM-nél (2 s/ütem) a hossz float-pontos, ott nem jelentkezik. Ugyanez történik a LENGTH (ütem) szám állításakor, megerősítés nélkül (`:85-89`), és ha `tempoMgr` még null (1 s hossz: `:221-223`). Futtatással nem igazoltam → **ellenőrizendő** nem-120 BPM-es projekten.

**L4 (P0) A Gain és Pan kétszer hat.** A `gains` (gain × pán) a **felvételi** jelútban (`REC/Recorder.cpp:163, 169-173`) és a **lejátszásnál** (`:184`) is szorzódik: gain 0,5 → felvétel ×0,5, lejátszás ×0,5 = ×0,25; a pán is kétszer csillapít; az exportált WAV a már „beégetett” értéket tartalmazza. Monitor-útvonal: ugyancsak `recorded`-ból (`:174`). A Looper `outGain/outPan` csúszkáinak nincs más jelentése (`UI/kick_LoopRecorder.cpp:81-82`).

**L5 (P1) Hullámforma-frissítés: dropout-gyanú.** Futás alatt 30 Hz-en `setThumbnail()` → `ae.getLooperSampleBuffer()` → `getAudioBuffer()` a teljes eddigi felvételt másolja `SpinLock` alatt (`UI/kick_LoopRecorder.cpp:194-198, 387-390`; `REC/Recorder.cpp:107-112`), az audio-szál `TryLock` bukásakor `clearOutputs(); return;` (`:159-160`) → kihagyott blokkok a hurok kimenetében és bemenetében. Egy 16 ütemes, 60 BPM-es hurok ≈ 64 s ≈ 24 MB másolat 33 ms-onként. Futtatással nem igazoltam. Közös a Sampler S8-cal (SA-07).

**L6 (P1) Export.** (a) `Sampler::getValidFileName` Windows-szeparátort épít (`kick_Sampler.cpp:287`), ütközéskor hibás név; a fájl a projekt-mappa *szülőjébe* kerül (`UI/kick_Looper.cpp:53-54`; SA-03). (b) Az export a `validFrames` hosszát írja (`REC/Recorder.cpp:109-110, 119-134`), nem az ütem-hosszt: ha a felvétel rövidebb, a fájl nem ütem-pontos. (c) A mentési hibát senki nem olvassa (`looperSave` void; `lastError()` nem jut el az UI-ig) — utána az `addAsset` csendben elbukhat (`UI/kick_Looper.cpp:55-63`). (d) A mintaalapú cache a >5 s hosszú WAV-ot 5 s-nál vágja (SE-12). (e) 24 bites WAV az eszköz frekvenciáján (`REC/Recorder.cpp:128`); overdub-összeg ±1 fölé mehet és exportkor vágódik (nincs limiter).

**L7 (P1) Export/Clear gombok állapota.** Az engedélyezés a `recordingStarted` jelre épül, amit csak a képernyő-gombok kattintása süt el (`UI/kick_LoopRecorder.cpp:97, 111`); a hardveres REC/OVERDUB/STOP (`record()/stop()`, `:138-176`) nem. Induláskor mindkettő tiltott (`UI/kick_Looper.cpp:33-34`) → **hardverrel felvett hurkot nem lehet exportálni/törölni**, amíg a képernyőn rá nem kattintanak a REC/STOP gombokra. Ráadásul a felvétel-gombra az engedélyezés *kikapcsolt* értékkel fut (`recordingStarted(!toggle)`).

**L8 (P1) Reverse / állapot-visszatükrözés.** A `timerCallback` `playB.setToggleState(ae.isLooperReverse())`-t ír, majd rögtön felülírja `playB`-vel (`:368, 370`) — feltehetően `reverseB` kellene. `reverseB` csak lejátszás közben engedélyezett (`:367`); a `record()` mindig előre állít (`:159`) a gomb állapotának módosítása nélkül.

**L9 (P1) Tempó- és taktus-kapcsolat.** A hurok hossza másodpercben fix a beállításkor; a tempó-változás (vagy másik szekvencia) a hurkot nem igazítja (csak képernyő-belépéskor/ütemszám-váltáskor, L3). A `tempoMgr` nyers mutató (`UI/kick_LoopRecorder.cpp:67-70`, `kick_LoopRecorder.h`) — projekt-váltás/szekvencia-törlés után lógó mutató lehet (NEM ELLENŐRIZHETŐ KÓDBÓL, hívódik-e a listener minden cserénél). A skála a Project Defaults számláló/nevezőjét használja (`:260, 268`), a hossz a szekvencia taktusát — különböző taktusnál a skála nem egyezik a hurokkal.

**L10 (P1) A Looper a képernyő elhagyásakor leáll.** `visibilityChanged(false)` → `stop()` (SYNC-nél `setSequencePause()`) és `setLooperEnable(false)` (`:420-446`). Hatás: a hurok nem játszható háttérként más képernyőn; SYNC mellett a képernyő elhagyása **megállítja a szekvenciát**, amit a felhasználó nem a looperből indított. Tervezési döntés kell.

**L11 (P2) Egyéb.**
 - Dupla indítás: a REC egyszerre hívja `setLooperRecord(true)`-t (→ `startRecording(false)`) és `looperStart(false, sync)`-et (`UI/kick_LoopRecorder.cpp:147, 160`; `REC/Recorder.cpp:51, 77-91`) — a `history.makeCopyOf(audio)` kétszer fut, az undo-pont a másodikra mutat.
 - A „SEQ” szinkron `skipThreshold = sync` — küszöb nélkül indul (`:160`).
 - A mérő `else if (1 <= values.size())` ága (`:380-381`) `values[1]`-et olvas — halott ág (a mérő mindig 2 elemű: `REC/Recorder.cpp:135-138`), de hibás.
 - Csatornaszám-váltás (mono↔sztereó) a következő indításkor törli a hurkot (`REC/Recorder.cpp:86-90`), figyelmeztetés nélkül.
 - `RECORD` nem törli a meglévő hurkot (`fromBeginning = false`): a „csere” a körbeéréskor felülír, nem tisztít.
 - A hullámforma-forrás nem tulajdonos pointer (`WaveRenderer.setSource` nyers `getArrayOfReadPointers`, `:197`) a `view` puffer újraméretezésekor érvénytelenedhet (NEM ELLENŐRIZHETŐ KÓDBÓL, mikor rajzol).

## 4. Tervben / hiányzik (az Akai MPC Standalone-hoz képest, a docs-ból ismert tervek)

- A Looper az Akai-bejárás **WP-B** hatókörében van (`WALKTHROUGH_PLAN.md:20`: „…Song, Looper”), de a `walkthrough/WP-B.raw.json` 80 állítása között **Looper-állítás nincs** (grep 0 találat), `WP-B.md` nem készült el. Az Akai-paritási lista ezért **NEM ELLENŐRIZHETŐ a repóból**; teendő: LO-19.
- Dokumentált nyitott tételek: **H7** Looper SYNC fázis (Z2b) mérése hiányzik, AC2 (a Looper szinkronos REC elindíthat szekvencia-take-et; „javítás úton”: `a2cf876a`, `0193d9dd`, `cd9de309`) — `known-issues` H7; `TODO_MASTER.md:4, 62` (H2: „nyomkövető sorok egy buildben, injektált forgatókönyv”). **H17** audio bemenet (INPUT 1/2, +48 V) igazolatlan.
- A REC/OVERDUB gombok routingja és LED-je felhasználói döntés (`MD:75, 90`); a Looper-LED „kamerás ellenőrzése hiányzik”.
- Terv-hiány (a kódból látható szándék, de nincs kész): Undo/Redo (`looperUndo/Redo` API, tesztelt), hurok-ki/be (`setLooperLooping`), egymenetes felvétel.
- 04-screen-audit §6: a Looper-re külön tétel nincs; az 5. pontban a Preferences Project Defaults NUMERATOR/DENOMINATOR „csak a Looper-felvevőre hat” (`04-screen-audit.md` §8.2).

## 5. Javaslataim (improvementek, indoklással)

1. **Állapotgép egy helyen.** A looper állapota (idle / armed / recording / overdubbing / playing / stopped) legyen a motorban egyetlen forrás, az UI és a hardver csak parancsokat küld és állapotot olvas (a mostani `recordB/replaceB/stopB` toggle-ok és `recordingStarted` jel szétszórt állapotot hordoznak: L2, L7, L8). Ez megoldja a hardver-vezérelt felvétel UI-állapotát és a LED-et is.
2. **Hallható overdub**: lejátszás + felvétel egyszerre (a meglévő réteg a kimenetre, az új réteg hozzáadódik), headroom/limiter az összegzésre, undo-réteg (Undo/Redo gomb + hardver Shift+UNDO a Looper képernyőn).
3. **Egymenetes „N ütem” mód** (REC → N ütem után automatikus átváltás lejátszásra/overdubra) és a szinkron-indítás szekvencia-ütemhatáron; a jelenlegi körbeérő-felülíró viselkedés opcióként marad.
4. **Gain/Pan a lejátszási úton** (ne a felvételin), külön bemeneti trim a küszöb mellett; a „gain” csúszka átnevezése „threshold”-ra.
5. **Biztonságos hossz-kezelés**: a hossz ütem-egységben tárolva (nem másodpercben), tempó-váltásra újraszámolva/újramintázva vagy kifejezett figyelmeztetéssel; ütemszám-váltás törlése előtt megerősítés; lebegőpontos összehasonlítás helyett egész mintaszám.
6. **Háttérben szóló looper** (opció): a looper a képernyő elhagyása után is fusson, a státuszsávban jelezve; a SYNC-hez kötött szekvencia-leállítás megszüntetése.
7. **Hardver**: Looper-képernyő megnyitása a Key 61-ről (a gomb-kiosztás owner-döntés); REC/OVERDUB/PLAY/STOP/UNDO a képernyőn már van, a Q-Link 1–4 kiosztás: küszöb, monitor, gain, pan (az 5–10 hivatkozások kivezetése).
8. **Teljesítmény**: hullámforma-gyorsítótár (min/max) a motor oldalán, nincs teljes puffer-másolat (SA-07).
9. **Tesztelhetőség**: a hossz-összehasonlítás, az overdub-hallhatóság, a gain/pan-egyszeri-alkalmazás és az állapotgép host-teszt (a `Recorder` már GUI-mentes: `AudioTests.cpp:32-126`); SYNC-fázis: injektált forgatókönyv-teszt.

## 6. Hardveres és unit-tesztek

**Ma fedi:** `Kick/tests/platform/AudioTests.cpp:32-126` — a `mt::Recorder`/`Looper` motor: pontos puffer-határig rögzítés, overdub tartalma, undo/redo, ellenkező irány, mono-downmix, küszöb, mentés (tranzakciós), frekvencia-váltás, **körbeérő looper allokáció nélkül** (`:99-114`). A szekvenciafelvétel/looper szétválasztás host-tesztje: `SeqRecordTests.cpp` (`onPlayStartedUnlessLooper`, `looperBusy`). **Nem fedi:** az overdub-hallhatóságot (L1), a flag-törlést (L2), a gain/pan egyszeri alkalmazását (L4), a hossz-összehasonlítást (L3), az UI-t (Export/Clear állapot, Reverse), az exportot és a hardver-utat. Hardveren: a REC/OVERDUB gomb → Looper-routing és LED „jó” a felhasználó szerint, kamerás ellenőrzés nincs (`MD:90`); a Looper-felvétel hangja, a SYNC-fázis és az AC2 **nem mért** (H7); a Looper megnyitása a javítás előtt a MPC-n összeomlott (168417a1).

**A MPC-n ellenőrzendők:**
- [ ] A Looper megnyitása menüből friss `/data`-n és meglévő prefs mellett (168417a1 igazolása); a képernyő rajzolása több tempón.
- [ ] INPUT 1/2 jel a mérőn; mind a 6 bemeneti mód; monitor-késleltetés; +48 V (H17).
- [ ] REC → STOP → PLAY hallhatóság; **OVERDUB közben szól-e a meglévő hurok** (L1); OVERDUB után a sima REC viselkedése (L2).
- [ ] Szekvencia REC+PLAY a Looper használata után (L2/c).
- [ ] 100 BPM-es projekten Looper-felvétel → képernyő elhagyása és újra megnyitása: megmarad-e a hurok (L3).
- [ ] Gain/Pan állítása felvétel után és a felvétel előtt: a hang szintje egyszer vagy kétszer csökken (L4).
- [ ] Hardveres REC/STOP után Export/Clear gomb állapota (L7).
- [ ] SYNC be/ki: a hurok és a szekvencia fázisa (H7/Z2b, AC2); ütemhatár-pontosság.
- [ ] Export: fájl helye, név-ütközés, hossz, fájl lejátszása padon (5 s-os cache-korlát!).
- [ ] 16 ütemes hurok 60 BPM-en: dropout-mentesség és UI-folyamatosság (L5).
- [ ] Képernyő elhagyása felvétel/lejátszás közben (L10).

## 7. TODO

Hibák (sorrendben):
- [x] **LO-01** (P0, S) Overdub közben a meglévő hurok lejátszása (lejátszás + felvétel egyszerre), összegzési headroom — `REC/Recorder.cpp:175-187`; teszt: LO-17.
- [x] **LO-02** (P0, S) Overdub-flag kezelése: a REC mindig csere-mód, az OVERDUB-gomb kikapcsolása töröl; a `looperBusy` ne az időtlen flagre épüljön (`isRecording() || (isOverdubbing() && isRecording())`); LED-javítás — `UI/kick_LoopRecorder.cpp:94-103, 138-148, 372`; `ENG/mt_audio_manager.cpp:625`; `hardware/led_map.h:157-185`.
- [x] **LO-03** (P0, S) A hossz-összehasonlítás egész mintaszámra/ütemre; a hurok ne törlődjön képernyő-belépéskor; ütemszám-váltás megerősítéssel — `UI/kick_LoopRecorder.cpp:85-89, 429-436`; `REC/Recorder.h:41`.
- [x] **LO-04** (P0, S) Gain/Pan csak a lejátszási úton (vagy csak a felvételin), egyszer; a mentett WAV ne égesse be — `REC/Recorder.cpp:163-184`.
- [x] **LO-05** (P1, S) Export/Clear engedélyezése az engine állapotából (hardveres felvétel is), nem a UI-jelből — `UI/kick_Looper.cpp:33-44`; `UI/kick_LoopRecorder.cpp:94-112, 163-176`.
- [ ] **LO-06** (P1, L) Hullámforma-gyorsítótár és a teljes puffer-másolat megszüntetése — közös tétel: **SA-07** (`REC/Recorder.cpp:107-112, 159-160`; `UI/kick_LoopRecorder.cpp:194-198, 387-390`).
- [x] **LO-07** (P1, M) Export: a közös mentő-folyamat (SA-03/SA-11): projekt `samples/` mappa, egyedi név, hiba megjelenítése, **ütem-hosszra kerekített** export, `addAsset` hibaellenőrzés — `UI/kick_Looper.cpp:47-67`; `REC/Recorder.cpp:119-134`. Függ: SA-03, SA-11, SE-12.
- [x] **LO-08** (P1, S) Reverse állapot-visszatükrözés javítása (`reverseB`), a `record()` ne módosítsa a irányt a gomb nélkül, dupla indítás kiküszöbölése — `UI/kick_LoopRecorder.cpp:159-160, 367-370`; `REC/Recorder.cpp:51, 77-91`.
- [x] **LO-09** (P1, M) Tempó/taktus-kapcsolat: a hurok ütem-egységben, a skála a szekvencia taktusát használja (nem a Project Defaults-ot), a `tempoMgr` életciklusa (nem nyers mutató), tempóváltás-kezelés — `UI/kick_LoopRecorder.cpp:67-70, 220-224, 244-281`.
- [ ] **LO-10** (P1, S) Képernyő-elhagyás viselkedése: a looper háttérben is szóljon (opció), a SYNC ne állítsa meg a szekvenciát — owner-döntés — `UI/kick_LoopRecorder.cpp:420-446`.
- [x] **LO-11** (P1, M) Egymenetes „N ütem” felvételi mód + szinkron-indítás ütemhatáron (a körbeérő felülírás opció) — `REC/Recorder.cpp:175-182`; `UI/kick_LoopRecorder.cpp:138-161`.

Hiányzó funkciók / hardver:
- [x] **LO-12** (P1, M) Undo/Redo kivezetése a UI-ra és a hardverre (Undo a Looper képernyőn a looper-szintű visszavonásra); Clear megerősítése — `ENG/mt_audio_interface_looper.cpp:79-80`; `REC/Recorder.cpp:113-118`.
- [ ] **LO-13** (P1, S) Looper megnyitása a Key 61-ről: gomb-kiosztás **owner-döntés** (nincs szabad Looper-gomb a térképen); LED a képernyőhöz — `Kick/apps/kick/hardware/akai_map.h`; `led_map.h`; `bridge:1293-1300` mintára.
- [x] **LO-14** (P2, S) Key 61 4-Q-Link kiosztás (küszöb, monitor, gain, pan); az 5-ös Q-Link hivatkozás kivezetése — `UI/kick_LoopRecorder.cpp:177-191`; `kick_IOComponent.cpp`.
- [ ] **LO-15** (P1, M) SYNC-fázis (H7/Z2b) és AC2 mérése MPC-n, javítás, az eredmény rögzítése a `known-issues`-ban — `UI/kick_LoopRecorder.cpp:115-127, 138-161`; `ENG/mt_audio_manager.cpp:620-640`; `TODO_MASTER.md:62`.
- [ ] **LO-16** (P2, S) A „gain” csúszka átnevezése küszöbre, bemeneti trim, clip-jelzés, halott mérő-ág törlése — `UI/kick_LoopRecorder.cpp:76, 375-384`. Közös: SA-10.

Tesztek:
- [x] **LO-17** (P1, S) `AudioTests.cpp` bővítés: overdub közbeni kimenet (L1), flag-törlés (L2), gain/pan egyszeri alkalmazása (L4), hossz-összehasonlítás float/double (L3) — `Kick/tests/platform/AudioTests.cpp:99-114` mellé. Függ: LO-01…LO-04.
- [ ] **LO-18** (P2, M) A Looper UI-állapotgép GUI-mentes kivonása és host-teszt (Export/Clear állapot, Reverse, hardveres parancsok). Függ: LO-05, LO-08.
- [ ] **LO-19** (P1, M) WP-B Looper-szakasz (v3.10: 338–341. oldal körül, ellenőrzendő) végigvétele → Akai-paritási TODO-k; a MPC-s ellenőrző-lista (§6) felvétele a `HW_TEST_CHECKLIST.md`-be és lefuttatása.

## 8. Állapot (S-E kör, 2026-11-04)

Kész (kód + host-teszt `LooperLogicTests`, Mac build + smoke): **LO-02** (az overdub-flag minden kilépésnél törlődik, `looperBusy = isRecording()`; a duplikált indítás megszűnt: csak `looperStart`), **LO-03** (hossz-összehasonlítás egész mintaszámra, nincs törlés ismeretlen tempónál, megerősítés ütemszám-váltás előtt), **LO-05** (Export/Clear az engine állapotából, hardveres felvételre is), **LO-08** (Reverse tükrözés javítva, a felvétel a gombot előre állítja), **LO-12** (UNDO gomb, a második lenyomás redo; Clear megerősítéssel), **LO-14** (Q-Link 1-4: küszöb, monitor, gain, pan; a küszöb a motorba is megy).

Részben: **LO-07** (UI-oldal kész: `<project>/samples`, egyedi név, lemezen ellenőrzött mentés, hibaüzenet a motor `lastError()`-jából, `addAsset` ellenőrzés, a program a dialógus választása szerint; az ütem-pontos export a `Recorder::save`-ben van: R-2), **LO-09** (a tempó lekérése minden használatkor friss, nincs nyers mutató; ütem-egységes hossz és a szekvencia taktusát követő skála NYITOTT), **LO-10** (a képernyő elhagyása már nem állítja meg a szekvenciát; a háttérben szóló looper owner-döntés), **LO-16** (halott mérő-ág törölve; a „gain" -> „threshold" átnevezés a közös `KickIOComponent`-ben van, SA-10), **LO-17/18** (`LooperLogicTests`; a Recorder-motor tesztjei R-2).

Nem érintve (R-2 vagy `AudIO/Recorder`): **LO-01** (overdub alatt a hurok némán fut: `Recorder::process` ágai), **LO-04** (gain/pan kétszer), **LO-06** (hullámforma-pillanatkép spinlock alatt, ~-0,04 s/s drift: mérést kér, R-2), **LO-11** (egymenetes felvétel). Owner-döntés / ember: **LO-13** (Key 61 gomb-kiosztás), **LO-15** (SYNC fázis és AC2 mérése MPC-n), **LO-19** (WP-B Looper-szakasz; MPC-s lista: `AKAI_MPC_KEY61_HW_TEST_CHECKLIST.md` 36-37.).

## 9. Állapot (R-11 befejező kör, 2026-11-04)

Kész: **LO-01, LO-04, LO-11** (R-2: `Recorder`, `AudioTests.cpp` 107-141: overdub-kimenet, gain/pan egyszer, egymenetes felvétel), **LO-07** UI-oldala (a Bars mező a motor `setLooperBars`-ába megy a hossz-váltáskor és a képernyő megnyitásakor: az export a hurok egész ütemeire kerekít; `<project>/samples`, egyedi név, ellenőrzött mentés, hibaüzenet: korábbi kör), **LO-09** (a skála a *szekvencia* taktusát követi, a tempó minden használatkor friss; `LooperLogicTests`), **LO-17** (L1 és L4: `AudioTests`; L3 hossz-összehasonlítás és a hurok-hossz/ütem számítás: `LooperLogicTests`; L2 flag-törlés: a motor-oldali `looperBusy = isRecording()` és a UI `stop()`), **LO-19** (WP-B Looper-szakasz átvétele a v3.10 Standalone OS Guide 338-341. oldalával, lent; MPC-s lista a checklistben).

Nyitott / döntés / blokkolt:
- **LO-10** (háttérben szóló looper, owner-döntés). A v3.10 kézikönyv nem mondja ki, hogy a Looper más módban is szól. Javaslat: opt-in beállítás ("Looper plays in the background"), alapértelmezés *ki* (mai viselkedés: elhagyáskor megáll); bekapcsolva a `setLooperEnable( visible )` helyett `setLooperEnable( true )` és a `stop( false )` elmarad. A SYNC már nem állítja meg a szekvenciát.
- **LO-13** (Key 61 gomb, owner-döntés). A hardver felirata a SAMPLER gombon "LOOP": javaslat: *Shift+SAMPLER = Looper* (ma a híd mindkét esetben a Samplert nyitja, `kick_hardware_bridge.h:1442`), LED-del a képernyőhöz. A híd módosítása R-3 terület.
- **LO-16** (gain -> threshold átnevezés, bemeneti trim, clip-jelzés): a csúszka a közös `KickIOComponent`-ben (`kick_IOComponent.h:93-95,139`) van, a Sampler is ezt használja (SA-10, másik worker). Javaslat: a komponens kap címke-API-t (`setGainTitle`), a Looper "THRESHOLD"-ot, a Sampler szintén; a trim és a clip-jelzés a `levelMeter` mellé.
- **LO-15** (SYNC-fázis és AC2 mérése): MPC, ember.

### Akai-paritás: a v3.10 Standalone OS Guide Looper-fejezete (338-341. oldal) és a Kick

| Akai (v3.10) | Kick |
|---|---|
| Input Source: Input 1,2 / Input 1-2 / Resample L / R / L+R | megvan (a hat bemeneti mód, `options.setInputSelection`) |
| Mono / Stereo mező | a bemeneti módba olvad (`ae.getLooperRecordingMode`) |
| Inserts (legfeljebb négy), a felvételre hatnak | az Inserts képernyő a Looper gombról megvan; hogy a felvett hangba égnek-e, nem ellenőrzött (MPC-s lista) |
| Monitor In / Off, csak bemenet esetén (Resample-nél nem) | megvan; a "csak bemenetnél" korlát nincs ellenőrizve (TODO a kijelző-oldalon) |
| Threshold csúszka, csak Sync ki állapotban; Knob 1 | megvan (`looperStart( false, sync )`: Sync bekapcsolva a küszöb kimarad; Q-Link 1) |
| Peak hold törlése a mérőre koppintva | nincs (kicsi UI-feladat a `levelMeter`-ben, LO-16 mellé) |
| Bars | megvan (LENGTH) |
| Sync | megvan |
| Record To: Play / Overdub | nincs mint választó; a REC / OVERDUB gombok és a küszöb adják a viselkedést (eltérés: nincs "felvétel-élesítés, aztán a transzport indít" mód) |
| Output Gain | megvan (+ Pan: extra) |
| Replace / Overdub gombok, leállítás a /Play gombbal | megvan |
| Reverse: Sync be -> a hurok végén fordul, Sync ki -> azonnal | **eltérés**: Kick mindig azonnal fordít (`setLooperDirection`) |
| Clear | megvan (megerősítéssel) |
| Export to Track / Shift+Export to Pad, "Keep or Discard Sample", Add Event | részleges: Kick programot és padot kér; az Add Event, az Assign track és a Root Note nincs |
| Looper megnyitása: Menu, vagy Shift+Sampler/Looper | Menu megvan; hardver-gomb LO-13 |

Akai-paritási teendők ebből: Reverse a hurok végén Sync alatt (motor), Record To választó, Peak hold törlés, Export-ablak (Add Event / Root Note), Monitor-korlát Resample-nél.
