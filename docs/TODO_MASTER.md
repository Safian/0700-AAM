# TODO — teljes lista, csoportosítva (2026-11-04)

## A. Egyedül elvégezhető (éjszaka; ebben a sorrendben, az ember nélkül haladó tételek elöl)
1. **Főszál, azonnal:** mobil ág pusholása a forkra (fast-forward); Looper SYNC fázis mérése és AC2 (nyomkövető sorok egy buildben, injektált forgatókönyv); Aura 45 perces soak figyelése.
2. **1. hullám (W1-A..D, 4 agy-worker párhuzamosan):** Preferences-csomag, Grid-hibák, MIDI program, CLAP-FX host + `clap.gui`. Utána: átnézés, merge, **egy** ARM-build + telepítés, MPC-s ellenőrzés (összeomlás-mentesség, napló, injektált forgatókönyvek), H3 tételek (pad "csendes állapot", kerék/pedál-mérő, List Edit dial, modális ablakok).
3. **2. hullám (W2-A, B, C, D):** indítás, főképernyő tabok, fájlböngésző-források (M0 helyi mappa + cache, M1 WebDAV), triázs-frissítés.
4. **VST- és FX-fejlesztés külön repókban, külön sessionökben fut (owner, 2026-11-04; a Kick-fő vonal nem nyúl hozzájuk):**
   - **VST-ág (instrumentek)**: `Safian/SynthyBoyVST` (nyilvános, AGPL-3.0; helyi klón `Projects/Safian/SynthyBoyVST`), session "SynthyBoyVST fejlesztése". A spike (egy kódból VST3+CLAP, JUCE 8.0.17 AGPL-módban + clap-juce-extensions) kész és a main-en (`b86cfa1`, CI Linux+macOS zöld, a Windows újrafuttatása folyamatban). Tartalma: szintetizátor, zongora-plugin.
   - **FX-ág (effektek)**: `Safian/KickEQ` (nyilvános, AGPL-3.0; helyi klón `Projects/Safian/KickEQ`), session "KickEQ fejlesztése": a Kick EQ az első saját FX és a plugin-template pilotja; a spike a `spike/vst3-clap` ágon (`b5096b7d`), a session ellenőrzi és mergeli.
   - A W3-B, W3-C, W3-D itt **nem a mi feladatunk**: nem ideiglenes helyi worktree, a repók nevei eldőltek. Ehhez a két repóhoz a Kick-oldali munka nem nyúl.
   - **Platform-követelmény minden pluginra** (docs/plans/02-03-05): mennie kell az Akai MPC-n (Linux ARMv7) ÉS a telefonos appban (iOS/Android); a hivatalos pluginok az app részei, a tartalmuk (minták/presetek) telepítéskor töltődik le, törölhető és visszaállítható; "letölthető-kész" architektúra (`PluginManifest` + `StaticPluginLoader`/`DynamicPluginLoader`); az áruházi egyeztetés csak a kész produktum után.
   - **A Kick fő vonalán marad** (a host-oldal): W1-D (CLAP-FX az Insert-böngészőben, `clap.gui`), W3-E (`.kickplug` telepítő / plugin-betöltő a `PluginManifest` és a Static/DynamicPluginLoader szerint, mobilra is), W3-A (érintő-zongora a híd után).
5. Ami **nem** megy ember nélkül, az a B. szakaszban van: reggelre összegyűjtve, pontos lépésekkel.

## B. Ember kell (reggelre összegyűjtve)
- Hallgatás: többmintás zongora alul/középen/fent, halk/hangos (a Demo 01 most a zongora); minden új hangzási funkció.
- Fizikai: Controller Mode mérés Mac-en, majd iPhone/iPad; fizikai ujjas strip Track Level; többujjas érintés a kijelzőn.
- Jogi átnézés: Salamander CC BY 3.0 ↔ AGPL, Google API ToS, Splice ToS, libcurl/TLS licenc.
- Döntések: Akai-specifikus réteg tervezése; Serum-szintű funkciólista; backend hosting és kulcsgazda.
- iOS: szimulátor/eszköz indulás és külső MIDI a mobil ágon (nem ellenőrzött).


## Állapot (2026-11-04 03:50) és a képernyő-review backlog (docs/plans/screens/*.md, 22 dokumentum)
**Kész, mergelve és pusholva (`468d65fd`):** W1-A Preferences-csomag, W1-B Grid, W1-C MIDI program, W1-D CLAP-FX host + `clap.gui`, W2-A indítás, W2-B főképernyő tabok, W2-C fájlböngésző-források (M0 helyi + M1 WebDAV; https az MPC-n nincs TLS-könyvtár miatt), Looper SIGFPE javítások (két nullával osztás), crash-jelentés.
**Kész, helyben mergelve, még nem pusholva (integrált ARM-buildre vár):** W3-A érintő-zongora (`83d1f096`).
**Fut (éjszaka):** W3-E `.kickplug`/PluginManifest telepítő (agy), Looper lejátszási fázis gyökérok-elemzés (agy; mérés: felvétel rendben, lejátszás ~1,3 s eltérés), S-B minta/sampler/assign képernyők (agy), S-A mixer/mute képernyők (sonnet), S-C szekvencer-szerkesztők (sonnet).
**Következő egységek a képernyő-review backlogból (fájlkör szerint szétválasztva, a fentiek után):**
| ID | Képernyők (docs/plans/screens/) | Végrehajtó |
|---|---|---|
| S-D | menu, projects, file-browser, system-resources, header-statusbar-popups, sw-update | sonnet |
| S-E | looper (a fázis-javítás után), inserts, xy-fx, knob-edit-qlinks, midi-control | agy |
| S-F | pad-perform, program-edit | sonnet (a W3-A után: a Keyboard/Pad Perform közös skála-forrás) |
Mindegyik egység a dokumentum TODO-sorait csinálja, a hardvert/hangot kérő tételek az "ember kell" listába kerülnek.

---
(Részletes egységek és sorrend alább.)

Forrás: `docs/plans/01..05`, `docs/plans/research/known-issues-and-roadmap.md`, a 2026-11-03/04-es munka nyitott tételei. A tervek részletei ott vannak; ez a végrehajtási sorrend. Végrehajtó: **agy-worker** (saját worktree + ág, a kvóta elfogyásakor automatikusan **sonnet-worker** viszi tovább ugyanazt a feladatot a worktree állapotából), a főszál ellenőriz, mergel, és **egy** közös ARM-build/telepítés/MPC-teszt fut hullámonként (egy MPC, egy Docker-build).

Szabályok minden egységre: saját worktree és ág; csak a felsorolt fájlok; nincs push, nincs MPC-hozzáférés a workernek; kapu = host-teszt (ASAN/UBSAN) + `-fsyntax-only`/Mac-build, ahol lehet; nincs "egyelőre így" ág; a híd (`kick_hardware_bridge.h`) közös fájl: minimális, helyi módosítás, vagy a hullámban csak egy egység érintse.

## Már kész (ellenőrzött, pusholva `safian/main`)
Keygroup (egy minta kromatikus + több minta zónák/rétegek), fájlböngésző SD/USB, projektváltás-összeomlások (virtuális destruktor, sáv-sor), crash-fájl (pc/lr/veremátfésülés), logolás (időbélyeg, szál, kategóriák, rotáció, lépés-napló, főhangerő-napló), aszinkron mappaméret, OCT-akkord, oktáv-LED sor (mért), pendrive-telepítő szkriptek (offline tesztelt), Demo 01 = zongora, strip Track Level (injektálva igazolt), mobil hardverkinézet kivétele (helyi `feat/mobile-slideout-ui`, **nincs pusholva**).

## 1. hullám — párhuzamos, szétválasztott fájlkörrel (agy → sonnet tartalék)
| ID | Egység | Terv | Fájlkör | Kapu |
|---|---|---|---|---|
| W1-A | **Preferences-csomag**: `PlatformServices` interfész (desktop/Behringer/Akai MPC valós útvonalakkal: backlight sysfs, `end0`, iwd, udhcpc, `date`/`timedatectl`, factory reset útvonalak, Info verzió); biztos hibák (Ethernet MODE, DNS/gateway, dátum/idő, AUTO SAVE, MIDI-port visszaállítás, WiFi gateway-mező, `KickMidiControl::isValid`, WiFi-jelszó/injekció védelem); fogyasztó nélküli beállítások bekötése vagy elrejtése; CV oldal (kód + teszt, **I2C-re/CV-re nem írunk**); perzisztencia (kilépéskori mentés, sérült fájl, migráció, teljes makeDefaults); UX (állapotsor, háttérszálas WiFi, Factory Reset megerősítés) | 04 §8, `research/preferences-part1/2.md` | `Kick/modules/kick_ui_settings/`, `kick_Hwpref.*`, új platform-szolgáltatás fájlok, `kick_settings*` | host-tesztek (parancs-összeállítás, injekció, perzisztencia), Mac-build |
| W1-B | **Grid-hibák és csonkok**: dupla bank-eltolás, COPY/ERASE kétszer, Magnify (MPC), Q-Link zoom, Split szivárgás; Humanize velocity; stubok megvalósítása vagy elrejtése | 04 §3, `research/screen-inventory.md` | `Kick/modules/kick_ui/**/kick_AppGrid*`, a Grid eszközei | host-teszt a bank-eltolásra és gombállapotra, Mac smoke |
| W1-C | **MIDI program**: kimeneti lista zár/snapshot, port-eltűnés jelzés, "Add MIDI program" track-kel és egyedi névvel, program-szintű note/velocity-tartomány/transzpozíció, csatorna-konzisztencia, CC-címkék, undo/perzisztencia (`valueTreeChildAdded/Removed`) | 04 §5, `research/midi-program-add.md` | `Kick/modules/engine/**/midi_device_manager*`, MIDI program UI/modell | host-tesztek (port/csatorna/PC/bank/undo), Mac smoke |
| W1-D | **Host-oldal a CLAP-effektekhez és `clap.gui`-hoz**: CLAP-FX az Insert-böngészőben, slot-kezelés, állapot a projektben, latency/tail, generikus paraméter-felület; `clap.gui` beágyazás a hostban; a böngésző kereshető/kategorizált az Airwindowsra | 05 §3.1, 03 1. fázis, 02 §3.3 | `Kick/clap_host/**`, `kick_Inserts*`, Insert-modell | clap_host tesztek (MockClapSynth/effect), Mac-build |

## 2. hullám (az 1. után; a híd egyetlen egységé)
| ID | Egység | Terv | Megjegyzés |
|---|---|---|---|
| W2-A | **Indítás**: indítóképernyő lépés-állapottal, egyetlen betöltési útvonal (dupla `loadProject` megszüntetése), hibák láthatóvá tétele, állapot-csík, biztonságos mód két összeomlás után | 04 §1 | a híd indítás-része; ezzel egy időben ne legyen más híd-egység |
| W2-B | **Főképernyő tabok**: Sequence Time Signature, Track Monitor, törlés-megerősítés, program/track undo, Bounce/Convert stubok | 04 §2 | `kick_AppMainMode*`, `kick_AppSequenceOptions*`, `kick_AppTrack*` |
| W2-C | **Fájlböngésző-források**: `FileBrowserSource` interfész, helyi mappa + gyorsítótár (M0), WebDAV (M1); Drive/Splice **csak a jogi átnézés után** | 04 §7, `research/cloud-sources.md` | jogi átnézés: ember |
| W2-D | **Triázs frissítése**: `UPSTREAM_TRIAGE.md` újraosztása a mobil-változás és az új commitok után, hash-ek ellenőrzése `git log`-ból | — | csak doksi |

## 3. hullám
| ID | Egység | Terv | Előfeltétel |
|---|---|---|---|
| W3-A | **Érintő-zongora**: per-ujj touch a hídban, zongora-képernyő (`KeyboardComponent`), glissando, velocity-módok, pitch bend, sustain, késleltetés-mérés | 04 §4 | W2-A (híd) |
| W3-B (SynthyBoyVST repó, másik session) | **Zongoramotor** (plugin-független, C++17: SampleProvider/FLAC, voice-pool, 16 velocity-réteg crossfade, release, folytonos CC64, pedálzaj, streaming) + host-tesztek | 02 §3.1 | másik session (SynthyBoyVST) |
| W3-C (KÉSZ: SynthyBoyVST `b86cfa1`; KickEQ `spike/vst3-clap`) | **Spike**: minimális hangadó instrument egy kódból VST3 + CLAP, CI, clap-validator/pluginval; ebből dől el a GUI-keretrendszer és a wrapper | 02/03 repo-szerkezet | KÉSZ |
| W3-D (KickEQ repó, másik session) | **Kick EQ** (az első saját FX, a template pilotja) | 05 | W3-C |
| W3-E | `.kickplug` manifest + helyi telepítő a Kickben (aláírás-ellenőrzés kötelező) | 03 1.–3. fázis | W1-D |
| W3-F | Backend/index, build-farm, publikálói regisztráció, sandbox | 03 4.–6. fázis | terv-szintű döntések (hosting, kulcsgazda) |

## Főszál (hardveres igazolás, egy MPC, sorosan)
| ID | Tétel | Állapot |
|---|---|---|
| H1 | Aura SIGSEGV hosszú futás (45 perc üresjárat, `scratchpad/soak.log`) | FUT |
| H2 | Looper SYNC fázis (Z2b) + AC2: nyomkövető sorok egy buildben, forgatókönyv injektálva | KÖVETKEZŐ |
| H3 | Pad "csendes állapot", kerék/pedál → 1. track mérő, List Edit Data Dial, modális ablakok hardveres zárása, RECALL | 1. hullám buildje után |
| H4 | Strip Track Level: injektálva igazolt; fizikai ujjal ember | injektálva KÉSZ |
| H5 | Mobil ág pusholása a forkra (`fork/feat/mobile-slideout-ui`); iOS-szimulátor/eszköz indulás és külső MIDI **nem ellenőrzött** | pusholás: most; iOS: külön |

## Ember kell — részletek
- Többmintás zongora hallgatás alul/középen/fent, halk/hangos (a Demo 01 most a zongora).
- Controller Mode mérés Mac-en (a Key 61-et Controller Módba kell kapcsolni), majd iPhone/iPad.
- Jogi átnézés: Salamander CC BY 3.0 ↔ AGPL, Google API ToS, Splice ToS, libcurl/TLS licenc.
- Döntések: Akai-specifikus réteg (Locate, Sounds Mode, Keyboard Control menü) tervezése (a mód még nincs kitalálva); Serum-szintű funkciólista.

## Nem ebben a körben
Akai-specifikus réteg (Locate ablak, Sounds Mode, Keyboard Control menü, Menu tartalma, Track View): az owner 2026-11-02-i döntése szerint a Kick-funkciók után, az Akai rétegben, tervezve. WALKTHROUGH WP-A,C,E,F,H,I,J,K: nincs elkészítve.
