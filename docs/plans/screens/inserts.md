# Inserts — review és TODO

Alap: statikus kódolvasás, 2026-11-04, `claude/kick-plans`. Hardveren nem mértem. Rövidítések: `U` = `Kick/modules/kick_ui/dialogs/kick_Inserts.cpp`, `UH` = `.../kick_Inserts.h`, `E` = `Kick/modules/engine/audio_engine/engine/mt_audio_inserts_types.cpp`, `D` = `AudIO/AudIO/Daw/DawInserts.cpp`, `AX` = `AudIO/AudIO/airwindows/audioeffectx.h`.

## 1. Áttekintés

- **Mit csinál:** négy insert-slot (`DawInserts::MAX_INSERTS=4`, `DawInserts.h:15`). A bal oldali lista soronként: név-gomb, ENABLED/BYPASSED kapcsoló, ADD/DELETE gomb, felül ALL sor (`U:50-255`). A jobb oldalon üres slotnál kategória-böngésző (kategória + effekt-lista, `U:260-291`), kitöltött slotnál az Options panel: név, bypass, legfeljebb 16 forgatógomb (4 sorban, `U:307-409`).
- **Effektek:** Airwindows. 329 forrás-könyvtár van, ebből 327 regisztrált (`GlitchShifter` és `TripleSpread` ki van kommentezve: `airwindows.cpp:1148`, `airwindows2.cpp:973`), 22 kategóriában (a kategória a regisztrációs makró stringje: `audioeffectx.h:16`).
- **Elérés (honnan nyílik):**
  - program-mixer sor (`kick_program_mix.cpp:201-204`),
  - audio track (`kick_audio_track_mix.cpp:187`),
  - Returns, Submix, Output és Master sor (`kick_returns_mix.cpp:175`, `kick_submixes_mix.cpp:175`, `kick_output_track_mix.cpp:127`, `kick_master_route_mix.h:65`),
  - Looper (`kick_Looper.cpp:76-77`),
  - Program Edit: Drum és Keygroup (`kick_DrumInserts.cpp:17`, `kick_PanVelocityKG.cpp:168`, `kick_KeyGroupMaster.cpp:43`).
- **Hardver-kötés:** nincs. Az `Inserts::handleCommand` és `knobCommand` üres (`UH`, `Inserts` osztály), a képernyő nem kér `requiresHandleCommand`-ot. Q-Link, Data Dial, ± nem hat; a Key 61 előlapjáról csak a Mixerből érhető el. Az Automation és a metronóm ikon a fejlécben látszik (`U:18`), az Automation üres (`kick_modeHeader.cpp:54-55`).

## 2. Funkciók és állapotuk

| Funkció | Állapot | Bizonyíték |
|---|---|---|
| Kategória-böngésző, effekt-lista | KÉSZ | `U:260-291`, `mt_audio_interface.cpp:2134-2209` |
| Effekt hozzáadás (ADD) | RÉSZLEGES | `U:98-122` → `E:196-217`; MIDI/CV programra csendben -1 (`E:199`) |
| Effekt törlés | KÉSZ | `U:108-116`, `E:231-241` |
| Sorrend módosítása / áthelyezés | HIÁNYZIK | a slot a pozíció |
| Bypass soronként | KÉSZ | `U:124-131`, `E:183-194`, `D:148-152` |
| ALL ENABLE / ALL DELETE | RÉSZLEGES | `U:146-164`; az ALL gomb állapota nem szinkron, törlés megerősítés nélkül |
| Paraméter-forgatók | RÉSZLEGES | `U:333-368`; normalizált 0..1, nincs egység/kijelzett érték |
| Paraméter-érték szöveg (`displayValue`) | STUB | előáll (`AX:41-52`, `E:92`), a UI sehol nem használja (grep: 0 találat) |
| Mentés/betöltés (ValueTree) | BUGOS GYANÚ | `E:25-80`; név-üres és NC-17 hiba |
| Undo/redo | BUGOS GYANÚ | `E:210` undoval, `E:190`, `E:213` nélkül |
| Program-insertek feldolgozása | KÉSZ | `iSynthObject.cpp:202`, `DawRecorder.h:52-55`, `Recorder.cpp:190` |
| Bus-insertek feldolgozása (Return/Submix/Output/Master) | BUGOS GYANÚ (magas) | lásd INS-B1 |
| Q-Link / Data Dial vezérlés | HIÁNYZIK | `UH`: üres `handleCommand`/`knobCommand` |
| Paraméter-automatizálás | HIÁNYZIK | `mt_automation.h`: csak `Level`, `Pan` |
| CLAP effektek | HIÁNYZIK | `05-fx.md` §1; `Kick/clap_host/README.md:228-229` |
| Keresés, kedvencek, leírások | HIÁNYZIK | `Airwindopedia.txt` (3912 sor) nincs használatban |
| Effekt-presetek | HIÁNYZIK | nincs mentés/betöltés |

## 3. Mi nem működik / hibák

- **INS-B1 (P0): a bus-insertek valószínűleg nem szólnak.**
  - A `DawBus` örökli a `DawInserts`-t (`DawBus.h:27-29`), a `MTIBus::setup` ezt köti a UI-hoz (`mt_audio_bus_manager.cpp:88`). A `DawInserts::process` és `prepareToPlay` védettek; hívóik csak: `Recorder.cpp:58/190`, `DawRecorder.h:45/54`, `iSynthObject.cpp:174/202`. A `DawBus.cpp`-ben és a Core-ban (`Bus.cpp`, `Chain.cpp`) nincs hívás, a Core a saját `Chain`-insertjeit kezeli.
  - Következmény: a bus-insertre létrehozott effekt `_rate/_block` 0 marad (`D:20-34`), nincs `prepareToPlay`, és nincs `process`. A Returns/Submix/Output/Master Inserts képernyő tehát adatot tárol, de nem szól. A WP-D D-031/D-033 "OK" ítélete csak a képernyő-nyitásra igaz.
  - Repro: tegyél a Return 1-re egy erős hatású effektet (például `Reverb`), küldj rá. Várt: hallható. Kódból várható: nem változik a hang.
  - Bizonyosság: magas (grep), hallgatásos mérés hiányzik.

- **INS-B2 (P1): a mentett név üres lehet.**
  - Az `AudioEffectX` neve csak az `initializeParams()`-ban áll be (`AX:171-180`), amit a `getParameters()` hív. A `DawInserts::getEffectsNames` addig `""`-t ad (`D:95-101`).
  - Az `addInsert` sorrendje: `state.addChild(.. name=Effect ..)` (`E:210`), majd `insertsSerialize` (`E:213`), ami a gyerek nevét felülírja a motor `""` nevével, és csak ezután hívja a `getInserts()`-t (`E:214`), ami a nevet inicializálja.
  - Ha hozzáadás után nincs további paraméter- vagy bypass-változtatás (amely újra szerializál), a projekt üres nevet ment, és az insert betöltéskor elvész.
  - Az `insertsSyncInfo` (`E:83-94`) a nevet a `getEffectName`-ből olvassa, mielőtt a `getEffectParameters` inicializálna: az első hívás üres nevet ad.
  - Repro: adj hozzá egy insertet, mentsd a projektet, töltsd újra.

- **INS-B3 (P1): `NCSeventeen` nem tölthető vissza.** Regisztrált kulcs "NCSeventeen" (`airwindows2.cpp:197`), a `getEffectName` "NC-17" (`NCSeventeen.cpp:132-134`). A név-inicializálás után a szerializált név "NC-17", a `CREATE_OBJECT("NC-17")` nem létezik, az `isSame` is eltér. A másik 326 effektre a két név egyezik (szkriptes ellenőrzés).

- **INS-B4 (P0): audio-szál verseny.**
  - `InsertEffects::reset()` és `create()` (`D:13-34`) a UI-szálon `bypassed.store(true)` után `pObj.reset()` / értékadás; az audio-szál `process` ciklusa (`D:219-221`) ellenőrzés után még használhatja az objektumot. Nincs zár, épít-és-cserél, késleltetett törlés vagy atomi pointer.
  - A `getEffectsNames` és `getEffectsBypass` (`D:95-109`) szintén olvassa a `pObj`-t egy másik szálból.
  - Hatás: use-after-free esély insert törlésekor vagy cseréjekor lejátszás közben. A hardveres Aura-összeomlás (H1, `known-issues-and-roadmap.md` §2) idle állapotban `sampleFrames=0` hívásnál jött, ezt a `D:214-216` őrzi; a cserés versenyt nem fedi.

- **INS-B5 (P1): undo eltérés.** `E:210` undoval ad hozzá, a szerializálás (`E:190`, `E:213`, `E:225`, `E:237`, `E:248`, `E:296`) `um = nullptr`-val ír. Nincs fa-figyelő, amely az undo-t a motorra tükrözné. A visszavonás a fát módosítja, a hang marad; a következő szerializálás felülírja.

- **INS-B6 (P1): paraméter-megjelenítés.** A forgatók normalizált 0..1 értéket mutatnak (`U:345`). A `displayValue` (például "-6.00 dB") és az `unit` (`AX:41-52`, `E:92`) nincs használatban.

- **INS-B7 (P2): teljesítmény.** Egy forgatás-esemény: `setParameterValue` → mind a négy slot újraszerializálása + `VectorToJuceVar` allokáció + `getInserts()` újraépítés (`E:291-299`), majd a lista `refresh()` (`U:138-144`) újra `getInserts()`-t és négyszer `getInsertsInfo`-t hív. Az `AudioEffectX::getParameters` minden hívásra mindegyik paraméter display-stringjét formázza.

- **INS-B8 (P2): indexelés.** `setInsertBypass` ellenőrzés nélkül `pImpl->inserts[Position]`-t ír (`E:191`); a vektor csak `getInserts()` után töltött. `getNumInserts()` mindig 4 (`D` `_inserts.size()`), nem a kitöltött slotok száma.

- **INS-B9 (P2): MIDI/CV program.** `dynamic_cast` sikeres (minden program örökli a felületet, `mt_audio_iprogram.h:68`), de a motor-oldali `synth` nullptr (`MTIInsertsInterface::setup` csak audio/clip/clap/drum/keygroup programra hívódik). Az ADD csendben -1-et ad, a UI "DELETE"-re vált, majd a `refresh()` visszaállítja (`U:98-122`). Nincs hibaüzenet.

- **INS-B10 (P2): ALL gombok.** A `allEnable` állapota nem tükrözi a tényleges bypass-t (`U:205-206`). A `allDelete` KickButton (`UH`), `getToggleState()` mindig false, ezért az `U:155-164` feltétel mindig teljesül: megerősítés nélkül töröl mindent.

- **INS-B11 (P2): `removeAllEffects` a mester-bypass-t true-ra hagyja** (`D:88-93`); a `DawBus::reset()` is ezt hívja (`DawBus.cpp:104-117`). Csak `setup` olvassa újra.

- **INS-B12 (P2): `setParameter` hibakezelés.** Az Airwindows `setParameter` ismeretlen indexre `throw;` (`Air.cpp:135`) → `std::terminate`. A `DawInserts::setEffectParameter` (`D:138-143`) nem ellenőriz tartományt. A `deserializeEffect` korlátozza a számot (`D:189`).

- **INS-B13 (P2): UI-detalok.**
  - `InsertsList::pEffects` és `pInfo` nincs inicializálva (`UH:76-77`).
  - Az `Inserts::onInsertChanged/onInsertDelete` visszahívásokat a lista sosem hívja (`U:22-23`).
  - A kategórianevek nyersek, aláhúzással ("Tone_Color", "Lo_Fi", "XYZ_Filter", "Amp_Sims").
  - A lista kulcs szerinti (std::map) sorrendű, nincs keresés.
  - A Pad Mixer sorban nincs Inserts-gomb (WP-D D-064).

- **INS-B14 (NEM ELLENŐRIZHETŐ KÓDBÓL): blokkméret.** Az `AX:166-169` `process` a `blockSize()` értékkel hív, nem a `Buffer.getBlockSize()`-zal; a `D:215` csak a 0 esetet védi. Ha az audio-eszköz callback-mérete eltér a `prepareToPlay`-től, túlolvasás lehetséges. A csatornaszám (stereo feltételezés: `AX:22`, `Object(2,2,..)`) mono buffer esetén ugyanígy nem ellenőrzött. Hardveren mérendő.

## 4. Tervben / hiányzik (Akai MPC Standalone-hoz képest)

- **`05-fx.md`:** CLAP-effektek megjelenítése a böngészőben (kategória "CLAP"), állapotmentés (plugin-id + verzió + blob), automatizálás, latency/tail; "Kick EQ" pilot-FX; Airwindows kereshető és csoportosított böngészése, nevek és leírások az `Airwindopedia.txt`-ből; név és szerző megjelölése (MIT).
- **Walkthrough:** WP-D D-032 (nincs Effects fül; az Inserts a sorban nyílik), D-035 (Return/Output insert-helyek eltérő elrendezése), D-064 (pad-szintű Inserts hiányzik), WP-G G-180 (Track FX Rack mentés hiányzik), G-196 (render "Use Insert FX" nincs).
- **Akai-függőség:** a Touch FX plugin az Akai-n szintén egy insert a kijelölt kimeneten (lásd `xy-fx.md`); a Kick Touch FX tervezése erre a képernyőre épül (INS-01).
- **Automation:** csak Level és Pan lane van (`mt_automation.h`); insert-paraméter lane nincs. A fejléc Automation-gomb üres.
- **NEM ELLENŐRIZHETŐ KÓDBÓL:** az Akai 4 insert-slot pontos viselkedése, a Track FX Rack formátuma, a "Send csatorna használata előtt legalább egy effektet be kell tölteni a return mixerben" szabály ellenőrzése (D-031).

## 5. Javaslataim

1. **Bus-insertek élesítése (INS-B1).** Két út: (a) a `DawBus` mixin bekötése a bus feldolgozásába (`prepareToPlay` + `process` a megfelelő ponton, a pre/post-fader sorrend eldöntésével); (b) a `Core::Chain` insert mechanizmusának használata (`Design::createInsert`, `Chain.cpp`), amelyet az engine már feldolgoz. Az (a) a meglévő UI-adatmodellt őrzi. Döntés kell a feldolgozási pontról (pre-gain / post-gain).
2. **RT-biztos slot-csere.** Az új objektum épüljön a UI-szálon, `prepareToPlay`-jel; a csere egyetlen atomi pointer-írás, a régi objektum törlése késleltetve (az audio-szál által már nem használt állapotban, például egy "safe-point" sorral). A bypass váltása kereszt-halványítással (`RampedParameter`), hogy ne kattogjon.
3. **Név-registry.** Egy `EffectCatalog`: regisztrációs kulcs, megjelenített név, kategória (olvasható), leírás (generálva az `Airwindopedia.txt`-ből build-időben), kedvenc és legutóbbi jelölés. A szerializálás mindig a kulcsot mentse (soha a `getEffectName`-et); az `isSame` is kulcsot hasonlítson. Egy regisztráció-teszt minden effektre ellenőrizze a kulcs és a példány azonosságát.
4. **Paraméter-UI.** `displayValue` és `unit` megjelenítése; sorozatos (stepped) paraméterek kezelése; a 4 Q-Link az első 4 paraméterhez, a Data Dial a slotok és paraméter-oldalak között; ± a slotok sorrendjéhez; 16-nál több paraméter görgetése. A forgatás-esemény csak az érintett slotot szerializálja, és a lista frissítése legyen diff-alapú.
5. **Böngésző.** Kereső (a meglévő `ui::layout::keyBoardPopup` felhasználásával), alfabetikus és kategória szerinti lista, kategórianevek olvashatóan; az ADD csak tényleges kijelölés után; egyértelmű hiba, ha a programtípus nem támogat insertet.
6. **Perzisztencia és undo.** Egységes undo: vagy minden írás undo-val és fa-figyelővel (`valueTreeChildAdded/Removed`) a motor szinkronizálására, vagy semelyik; a névmentés javítása (INS-B2).
7. **Effekt-presetek és A/B** (mentés/betöltés a `/data` alatt); másolás track/program között; insert-lánc másolása.
8. **Mérés.** Insert-szintű CPU-mérő az MPC-n (ARMv7), figyelmeztetés xrun előtt; a legnehezebb effektek (`Reverb`, `Chamber2`, `TapeDelay`, `PitchDelay`) tiltólistája/jelzése.
9. **CLAP-effektek** a `05-fx.md` szerint, ugyanabban a böngészőben.
10. **Tesztelhetőség.** Host-teszt az összes effektre a gyártón keresztül (lásd 6. pont).

## 6. Hardveres és unit-tesztek

**Ami fedi most:** semmi nincs a `DawInserts`-re, az `MTIInsertsInterface`-re vagy a UI-ra. Az `AudioTests.cpp` az engine indítását és az eszköz-blokkokat nézi, effektet nem. A `StripModesTests` és a `RelativeCcTests` rokon, de nem érinti. WP-D D-031/D-033 "OK" csak a képernyő-nyitásra vonatkozik.

**Hiányzik (host, a `AudioTests.cpp` mintájára AudIO_juce-szal):**
- minden regisztrált effekt létrehozása a gyártón át: kulcs = `getName()` (NCSeventeen-t fogja), `numParams ≤ 16`, paraméter get/set round-trip a 0..1 tartományban, érvénytelen index nem terminál;
- feldolgozás szinusz/csend bemenetre: nincs NaN/Inf, 0 frame és változó blokkméret nem ront;
- `MTIInsertsInterface` szerializálás round-trip: hozzáadás → mentés → újratöltés (INS-B2);
- bus-insert hallható (RMS-változás) egy minimális `DawEngine` buszon (INS-B1);
- szálteszt: törlés/csere futó `process` mellett, TSAN/ASAN.

**Az MPC-n ellenőrzendők:**
1. Insert hozzáadás és törlés lejátszás közben (összeomlás-teszt, ismételt).
2. Return/Submix/Output/Master insert hallható-e.
3. Projekt mentés és betöltés insertekkel; NC-17.
4. CPU/xrun mérés effektenként ARMv7-n (különösen a hosszú idejű effektek).
5. Aura-hosszú-futás újrateszt insert élő állapotában (H1).
6. Q-Link/Data Dial vezérlés (miután megvan), érintési pontosság a forgatókon.
7. Back gomb, Mixer → Inserts → vissza a Key 61 előlapjáról.

## 7. TODO

Hibák:
- [x] **INS-01** (P0, L) Bus-insertek feldolgozása (`prepareToPlay` + `process`), pre/post-fader döntés — `DawBus.h:27-29`, `DawBus.cpp`, `mt_audio_bus_manager.cpp:88`. Függőség: az XY-02 / XY-08 (Touch FX cél) rá épül.
- [x] **INS-02** (P0, M) RT-biztos slot-csere (atomi pointer, késleltetett törlés), bypass kereszt-halványítással — `D:13-34`, `D:59-93`, `D:219-221`.
- [x] **INS-03** (P1, S) Névmentés javítása: a szerializálás a regisztrációs kulcsot menti, a név az `AudioEffectX` konstruktorában vagy a gyártóban áll be — `E:25-56`, `E:196-217`, `AX:171-180`.
- [x] **INS-04** (P1, S) `NCSeventeen` név/kulcs egyezés; `isSame` kulcs szerint — `NCSeventeen.cpp:132-134`, `D:36-41`.
- [x] **INS-05** (P1, M) Egységes undo + fa-figyelő a motor szinkronjához — `E:190`, `E:210`, `E:213`.
- [x] **INS-06** (P1, S) MIDI/CV program: letiltott Inserts-gomb vagy értelmes jelzés; az ADD hibakezelése — `E:199`, `U:98-122`.
- [x] **INS-07** (P2, S) Indexelés/inicializálás: `E:191` ellenőrzés, `UH:76-77` inicializálás, `setEffectParameter` tartomány-ellenőrzés — `D:138-143`.
- [x] **INS-08** (P2, S) ALL gombok: állapot-szinkron, törlés megerősítéssel/undo-toast-tal — `U:146-164`, `U:205-206`.
- [x] **INS-09** (P2, S) `removeAllEffects` bypass-állapot — `D:88-93`.

Hiányzó funkciók:
- [ ] **INS-10** (P1, M) Paraméter-UI: `displayValue`, `unit`, stepped paraméterek — `U:320-331`, `E:92`.
- [x] **INS-11** (P1, M) Q-Link (első 4 paraméter) és Data Dial/± bekötés a Key 61-hez — `UH` `handleCommand`/`knobCommand`.
- [ ] **INS-12** (P1, M) `EffectCatalog`: olvasható kategória/név, leírás az `Airwindopedia.txt`-ből, kedvencek — `AudIO_airwindows.*`, `05-fx.md` 3/5.
- [x] **INS-13** (P1, M) Keresés a böngészőben (billentyűzet-popup) — `U:260-291`.
- [ ] **INS-14** (P2, L) Insert-paraméter automation (új `Param` azonosítók), fejléc Automation-ikon sorsa — `mt_automation.h`, `kick_modeHeader.cpp:54-55`.
- [ ] **INS-15** (P2, M) Effekt-presetek, A/B, lánc-másolás.
- [ ] **INS-16** (P2, M) Pad Mixer Inserts-gomb (WP-D D-064).
- [x] **INS-17** (P2, L) CLAP-effektek a böngészőben (05-fx) — függ: INS-01, INS-03.

Javaslatok:
- [x] **INS-18** (P1, S) Forgatás-esemény optimalizálása: csak az érintett slot szerializálása, diff-frissítés — `E:291-299`, `U:138-144`.
- [ ] **INS-19** (P2, M) Insert-szintű CPU-mérő és figyelmeztetés ARMv7-n.
- [x] **INS-20** (P2, S) Blokkméret és csatornaszám kezelése az `AudioEffectX::process`-ben a tényleges bufferhez igazítva — `AX:166-169`, `D:215`.
- [ ] **INS-21** (P2, S) A 329 vs 327 számolás javítása a `05-fx.md`-ben (GlitchShifter, TripleSpread).

Tesztek:
- [ ] **INS-22** (P1, M) Host-teszt: minden effekt gyártás, paraméter round-trip, feldolgozás (6. pont) — `Kick/tests/platform`.
- [ ] **INS-23** (P1, M) Host-teszt: szerializálás round-trip és bus-insert hallhatóság — függ: INS-01, INS-03.
- [x] **INS-24** (P2, M) TSAN/ASAN szálteszt a slot-cserére — függ: INS-02.
- [ ] **INS-25** (P1, S) MPC-ellenőrzőlista (6. pont) a `HW_TEST_CHECKLIST`-be.

## 8. Állapot (S-E kör, 2026-11-04)

Kész (host-teszt: `DawInsertsTests` ASan alatt, `BusInsertsTests` végpontig a Core `Design`-on, Mac build + smoke): **INS-01** (a busz-insertek egy `DawInsertsHost`-on át a busz láncának insertjeként futnak, pre-fader; a hallhatóság a Core-on mérve, az MPC-s meghallgatás hátra), **INS-02** (élő mutató + audio-epoch várakozás, törlés csak ha az audio-szál elhagyta az objektumot), **INS-03/04** (a tárolt név a regisztrációs kulcs; `isSame` kulcs szerint, NC-17), **INS-05** (a fa undo nélkül íródik, egy forrás), **INS-06** (a MIDI/CV program jelzi, hogy nincs insert; sikertelen ADD hibaüzenettel), **INS-07**, **INS-08** (ALL ENABLE valódi állapot, ALL DELETE megerősítéssel), **INS-09**, **INS-11** (Q-Link 1-4 az első négy paraméter, Data Dial a slotok között), **INS-13/17** a W1-D munkája (keresés, CLAP), **INS-18** (csak az érintett slot szerializálódik), **INS-20** (blokkméret-védelem; csatornaszám-védelem nincs), **INS-24** (az audio-szál versenyét a `DawInsertsTests` futtatja).

Részben: **INS-10** (érték-szöveg és mértékegység kész; léptetett paraméterek nincsenek), **INS-14** (a fejléc Automation-ikon elrejtve; insert-paraméter lane NYITOTT: új `Param` azonosítók), **INS-22/23** (nincs „mind a 327 effekt" teszt és ValueTree-körút teszt).

Nyitott: **INS-12** (EffectCatalog / Airwindopedia leírások: a W1-D keresése él, a leírás-tár nem), **INS-15** (effekt-presetek, A/B, lánc-másolás), **INS-16** (Pad Mixer Inserts-gomb: a mixer-képernyők másik worker fájljai), **INS-19** (CPU-mérő ARMv7-n: ember), **INS-21** (05-fx.md számolás), **INS-25** (MPC-lista: checklist 38-39.).
