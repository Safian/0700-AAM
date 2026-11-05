# Terv 02 — Zongora-CLAP (terjedelmi terv + mintaforrások)

Állapot: terv. Háttér-kutatás: `research/sample-sources.md`, `research/architecture-and-publishing.md` (forrás-URL-ekkel). UI-minta: `assets/piano-ui/`. Kapcsolódó: 03 (plugin-ökoszisztéma: a zongora ennek az első hivatalos pluginja).

## 1. Fő megállapítások
1. **A mostani `KeygroupProgram` nem elég zongorának.** Nincs release-ág, burkológörbe, szűrő, round-robin, voice stealing, loop-crossfade, streaming; a velocity-rétegek kemények (max 4); a generált projekt ONE_SHOT, így a pedál/note-off hatástalan; a minták float32 sztereóban a RAM-ban vannak (>1 GB nem fér). → **Új, önálló zongoramotor** kell (plugin-független engine), nem a keygroup bővítése. A keygroup-ot a `make_piano_project.py`-os MPC-s út használja tovább.
2. **A Kicknek csak CLAP-host oldala van**, plugin-oldala és `clap.gui` sehol (host sem). Saját felülethez a `clap.gui` kiterjesztés kell a pluginban, és a hostban is (jelenleg generikus paramétersorok + 4 makró).
3. **Licenc:** a Kick-eredetű kód AGPL-3.0-or-later; a nulláról írt plugin, amely csak a MIT CLAP-fejléceket használja, szabadon licencelhető, de AGPL-lel is jól megy. JUCE-t csak AGPL/GPL módban szabad használni (kereskedelmi JUCE-licenc AGPL-kóddal egy binárisban nem keverhető).
4. **A minták nem kód:** külön csomagban, külön NOTICE-szal; nem olvasztjuk a bináris AGPL-os műbe.

## 2. Mintaforrás-döntés
Ajánlott út: **A + D** (szabad licencű meglévő + saját felvétel a hiányzóra).
| Szerep | Forrás | Licenc | Megjegyzés |
|---|---|---|---|
| Fő koncertzongora | **Salamander Grand V3** (Yamaha C5, 16 velocity-réteg, 48k/24) | CC BY 3.0, a szerző szerint 2022-03-04 óta public domain | 394 MB (16 bit WAV) / 707 MiB (FLAC) / 1,18 GiB (24 bit). Hammer- és húr-rezonancia release van; pedálzaj van; **pedál-lenyomott (sympathetic) minta nem biztos (NEM ELLENŐRIZVE)**. Kredit akkor is. |
| Alternatíva (jobb hangolás) | **Accurate-Salamander V6.2a** | CC-BY | ~0,8 GB FLAC; minden hang újrahangolva, dinamika kiegyenlítve. |
| Upright | **Upright Piano KW** | CC0 | Csak 2 réteg → rétegesítést ki kell egészíteni, vagy „lo-fi/ tack” karakterhangszerként. |
| Billentyűs extra | **VCSL Keys** | CC0 | 3 grand, 2 upright, csembaló; release és részben pedálminta. |
| Később | saját felvétel (D) | CC0 saját | pedálrezonancia, round-robin, e-zongora (Rhodes/Wurlitzer szabad készlet nincs), una corda. |
Kerülendő: Piano in 162 (nincs licenc, torrent), Maestro Concert Grand (all rights reserved), Amethyst (CC BY-ND = származékmunka tilos), jRhodes3d (NC), LivingRoom Upright (BY-SA), Pianobook-csomagok, és **gyári digitális zongora/szintetizátor mintázása** terjesztésre. Splendid Grand: eredete Akai-forrású, kockázatos.
AI/modellezés: a fő hanghoz **nem életszerű** ma; segédeszköznek jó (zajtalanítás, loop-keresés, hangolás/hangerő-kiegyenlítés a PD Salamanderen). Fizikai modell (FigBug/piano, GPL-3) csak kutatási mellékág, a kiadást nem blokkolja.
Teendők: a V3 szerzői PD-nyilatkozat archiválása; NOTICE + About-kredit; az átalakítás (FLAC, 24→16) jelölése; jogi átnézés nagy terjesztés előtt (CC BY 3.0 ↔ AGPL aggregate).

## 3. Terjedelem (mit építünk)
### 3.1 Zongoramotor (plugin-független, C++17)
- **SampleProvider**: RAM-ben előtöltött attack-fej + streamelt farok; FLAC dekódolás (dr_flac, szabad licenc); fájl-I/O külön szálon; 16/24 bit; teljes-RAM mód opció.
- **Voice-pool**: ~256 belső slot (attack + release + rezonancia + zaj), 128 játszott hang; értelmes voice stealing (legrégebbi/ leghalkabb release-ek előbb); a rezonancia-/zajrétegek kikapcsolhatók a polifónia felszabadítására.
- **Velocity**: 16 réteg, réteg-crossfade (nem kemény váltás), állítható veltrack-görbe.
- **Release**: kalapács-zaj, húr-rezonancia release, damper-zaj; release-hang velocitása a note-on-ból.
- **Pedál**: folytonos CC64 helyes held-note logikával (a pedál felengedéséig tartott hang, a release a felengedéskor), pedálzaj be/ki (félpedálnál nem triggerel), sostenuto (CC66), soft pedal (CC67, kezdetben szűrő+hangerő közelítés; valódi una corda minták csak saját felvétellel).
- **Rezonancia**: damper/szimpatikus rezonancia modell (mintaalapú, ha van; másképp saját közelítés, pl. rövid konvolúció/ rezonátor-bank). Az 5 ismert módszer közül választás prototípus-méréssel.
- **Hangolás**: tuning (A4), stretch-tábla, „Natural/Retuned” váltás.
- **Mikrofon-pozíciók** (ha a készlet adja): Close/Player/Room/Far keverés.
- **FX**: terem/reverb (konvolúció hosszú IR-rel CPU-nehéz → algoritmikus alap + opcionális IR), egyszerű EQ/kompresszor.
- **Teljesítmény**: valós idejű szál lock-mentes; MPC ARMv7 cél (2 izolált mag) → mérés és minőségi fokozatok.

### 3.2 CLAP-plugin réteg
- Kötelező kiterjesztések: audio-ports, note-ports (CLAP+MIDI), params, state, gui, latency, thread-check, tail, voice-info, render; hasznos: timer-support, posix-fd-support, preset-discovery/preset-load, note-name, remote-controls. MPE/MIDI2 P2.
- `init` gyors és GUI nélküli; mintát nem tölt ott. Note-id + NOTE_END kötelező.
- **State**: könyvtár-azonosító + verzió + hash (nem abszolút útvonal); hiányzó könyvtárnál „Telepítés/ Keresés” és csendes fallback.
- Validáció: clap-validator, pluginval, TSan/valgrind CI (Floe mintájára).

### 3.3 GUI (saját felület)
- **Kötelező hostoldali munka**: `clap.gui` a pluginban (Cocoa/X11/Win32 szülőhandle; Linuxon timer+fd kiterjesztés; Wayland alatt csak lebegő ablak) ÉS a Kick hostjában (beágyazás; jelenleg nincs).
- Keretrendszer-döntés (mérés/prototípus után): JUCE-AGPL + `clap-juce-extensions` (leggyorsabb, nem hivatalos), vagy DPF (ISC) / iPlug2 (zlib-szerű). A Kick-kinézethez (sötét, borostyán `#FFB608`) a LookAndFeel/színek másolhatók.
- UI-minta: `assets/piano-ui/` (fő nézet sötét/világos, kompakt, első indítási letöltés, About/credits).

### 3.4 Könyvtár-kezelő és csomagolás
- Mintakészlet külön csomagként (FLAC, SHA-256 manifest), letöltés worker szálon, kézi zip is; telepítési hely a felhasználói konfigban; attribúció az About-ban és a letöltőoldalon (CC-BY 3.0 4(b): szerző, cím, URI, licenc).
- Telepítés/ frissítés a 03 terv `.kickplug` mechanizmusán át.

### 3.5 Kiadás
- CLAP macOS (Developer ID, hardened runtime, notarytool, staple), Windows (kódaláírás: SignPath Foundation OSS-nek ingyenes de dual-licencelés nélküli OSI-licenc kell; Azure Artifact Signing egyéninek csak USA/Kanada), Linux (`~/.clap`, `/usr/lib/clap`), ARMv7 (MPC).
- VST3/AU: clap-wrapper (MIT) — P1; AudioUnitSDK Apache-2.0 ↔ AGPL kompatibilitás ellenőrzendő.
- Listák: CLAP Audio Software Database (clapdb.tech, cleveraudio.org űrlap/KVR-fórumszál), KVR-termékfelvétel (ingyenes fejlesztői fiók). A KVR Developer Challenge 2026 lezárult; a következő időpont NEM ELLENŐRIZVE.
- GitHub Releases: 2 GiB/asset határ → plugin és mintakönyvtár külön asset.

## 4. Fázisok (sorrendben; nincs „egyelőre így” ág)
1. **P0 – első kiadás**: motor (16 réteg + crossfade, release, folytonos CC64 helyes logikával, pedálzaj, streaming + FLAC, voice-pool) → CLAP (kötelező kiterjesztések, state) → `clap.gui` + saját UI → könyvtár-kezelő → attribúció → validator/TSan CI → aláírt csomagok → listák.
2. **P1**: sostenuto, soft pedal, retuned/natural + stretch, presetek, konvolúciós terem, VST3/AU, saját sympathetic-rezonancia modell.
3. **P2**: több mikrofon-pozíció, round-robin, MPE/MIDI2, további könyvtárak (Upright, VCSL), saját felvétel (D), Linux-csomagok.
A P0 nem „csonka verzió”: a P0 listán minden elem teljes minőségű; a P1/P2 elemek olyan többlet, amihez külön adat (pl. saját felvétel) vagy külön kutatás kell.

## 5. Kockázatok és nyitott kérdések
- Salamander: van-e pedál-lenyomott minta a V3-ban (különben a rezonancia közelítés vagy saját felvétel).
- MPC ARMv7 CPU/RAM: a zongora (voice-szám, FLAC-dekódolás, rezonancia, reverb) terhelhetősége hardveres mérés nélkül nem ígérhető.
- JUCE-mód (AGPL) és a GUI-keretrendszer végleges kiválasztása.
- CC BY 3.0 ↔ AGPL aggregate értelmezés; jogi átnézés.
- sfizz archiválva (2026-06) → saját motor, nem SFZ-függőség; a Floe (GPL-3) a legközelebbi architektúra-minta.
- Piano-modell hangminőség-mérés: halláspróba szükséges (a kutatások fórum-alapúak, nem hallgatási teszt).

## Repo-szerkezet és formátumok (owner döntés, 2026-11-04)
- **Minden plugin külön repóban él** (vagy a Kick-repóba almodulként bekötve, de akkor is önállóan fejleszthetően): a plugin **a Kick forrása és információi nélkül** épül, tesztelhető és kiadható. A Kick csak a kész, aláírt csomagot (`.kickplug` manifest + bináris, lásd 03) fogyasztja — nincs forrás-függőség a plugin → Kick irányban.
- Ebből következik: a plugin repója önálló CMake-projekt, saját CI (Linux x86_64/ARMv7/AArch64, macOS, Windows), saját licenc (AGPL-3.0-or-later ott, ahol Kick- vagy Vital-eredetű kód van; egyébként szabadon), saját release-ek és saját mintakönyvtár-csomag. Közös részek (plugin-sablon, manifest-séma, build-/aláíró szkriptek, clap-wrapper-beállítás) egy külön **plugin-template / SDK repóban** élnek, amit a pluginok sablonként használnak.
- **Formátumok: minden pluginból egyszerre készül a VST3 és a CLAP** ugyanabból a kódból (CLAP-natív + clap-wrapper a VST3/AU-hoz, vagy JUCE + clap-juce-extensions; a választás a próbaépítés eredménye). A CLAP az elsődleges (a Kick csak ezt tölti be), a VST3 a DAW-os felhasználóknak.
- **Próbaépítés (spike) most:** egy minimális, hangot adó teszt-instrumentből (pl. szinusz/ egyszerű szintetizátor) induló önálló repo, amely egyetlen kódból VST3-at ÉS CLAP-ot épít, CI-vel és clap-validator/pluginval ellenőrzéssel. Ebből dől el a GUI-keretrendszer és a wrapper-út, mielőtt a zongorát/ SynthyBoy-t építjük rá. Nyitott: a repó neve és helye (GitHub-szervezet), a Vital-fork repója külön (GPL-3.0→AGPL, saját név).

## Platform-követelmény (owner, 2026-11-04): a pluginoknak (VST/ instrument és FX) a **telefonos appban** és az **Akai MPC-n (ARMv7)** is menniük kell
- **Célmátrix minden pluginra:** Linux ARMv7 hard-float (Akai MPC Key 61 — kötelező), Linux AArch64, Linux x86_64, macOS (arm64+x86_64), Windows x64, **iOS** és **Android** (a Kick mobilos appja), a VST3/CLAP asztali formátumok mellett.
- **Következmények a kódra:** a DSP-mag platform- és UI-független C++17 könyvtár (statikusan linkelhető, nincs dinamikus külső függőség a magban, nincs futásidejű memóriafoglalás/zár az audioszálon, NEON-barát, a CPU-költség mérve az MPC ARMv7-n és egy közepes telefonon); a GUI külön réteg, a mobilos/ MPC-s felület a Kick saját UI-jára képezhető (a mag paraméter-leírásokat ad, ne csak natív plugin-GUI-t). Egyetlen kódbázis, platformonként CMake-célok; a CI ARMv7-re (QEMU/ keresztfordítás) és iOS/Android build-ellenőrzésre is kiterjed.
- **Fontos terjesztési megkötés (ellenőrizendő, nem hivatalosan igazolt):** iOS-en az App Store-szabályok szerint letöltött **végrehajtható kód** (dinamikusan betöltött `.clap`/`.dylib`) nem engedett; a pluginokat **az appba statikusan beépítve** (vagy iOS-en AUv3-ként) lehet szállítani. Androidon a Play-szabályok ugyanígy korlátozhatják a letöltött natív kódot. Ezért a „linkről telepíthető plugin” (03) **MPC-n és asztalon** teljes értékű; **iOS/Android-on** a hivatalos pluginok (SynthyBoy, KickEQ, zongora) az app részeként készülnek, a harmadik féltől származó, letöltött natív plugin ott csak a platform-szabályok által engedett módon (vagy egyáltalán nem) lehetséges — jogi/ platform-szabály ellenőrzés kell, mielőtt ígérünk bármit. A Kick iOS-buildje most indításkor kilép (`AGENTS.md`, „Known state”), a mobil-út ettől függetlenül nyitott munka.

## DÖNTÉS (owner, 2026-11-04): hivatalos pluginok = az app része; tartalom telepítéskor töltődik le; törölhető és visszaállítható
- **Alapból az app része** a hivatalos plugincsomag (SynthyBoy, KickEQ, zongora, hivatalos FX-ek). A **telepítésnél (első indításkor/ a telepítő lépésében) töltődnek le** a nagy tartalmak (mintakészletek, presetek, hullámtáblák) a hivatalos backendről (03), SHA256-ellenőrzéssel és megszakítható/ folytatható letöltéssel.
- **Törölhető és visszaállítható:** a user a Plugin-kezelőből bármelyik hivatalos plugin tartalmát törölheti (tárhely felszabadítása), és egy gombbal visszaállíthatja (újraletöltés); a törlés nem érinti a projekteket (a projekt csak azonosítót+verziót hivatkozik; hiányzó tartalomnál „Visszaállítás” felugró és csendes fallback, nem összeomlás). A felhasználói presetek/ beállítások külön maradnak.
- **Platformonként:** MPC/asztal — a plugin (`.clap`) és a tartalom is letölthető/törölhető/visszaállítható. **Mobil (iOS/Android) — a plugin KÓDJA mindig az appban van** (statikusan beépítve, lásd a platform-követelményt: letöltött végrehajtható kód nem engedett), tehát a kód nem törölhető, legfeljebb **letiltható/elrejthető**; a **tartalom** (minták, presetek) letölthető, törölhető és visszaállítható. A letöltés adat, nem kód — ez összefér a platformszabályokkal (ellenőrizendő a végleges app-áruházi szabályokkal).
- Megvalósítás-vázlat: közös `ContentPack` fogalom a manifestben (id, verzió, méret, SHA256, licenc/ attribúció), telepítő-sor, tárhely-kijelzés, „telepítve/ hiányzik/ frissíthető” állapot, első indításkor ajánlott csomagok előre kijelölve (a user módosíthatja); a pendrive-os telepítő (`docs/hardware/key61/pendrive-install.md`) ugyanezt a verzió-/hash-elvet használja az MPC-n.
