# TODO — teljes lista, csoportosítva (2026-11-04)

## A. Egyedül elvégezhető — ÁLLAPOT (2026-11-04 ~19:00): minden tétel kész és ellenőrzött
Merge-elve `safian/main`-re (`0c15cf9a`), Mac-build tiszta, ARM-build 0 hiba, ctest 102/102, a smoke-ok PASS, az MPC-n telepítve és igazolva (indulás, képernyők, összeomlás-mentesség, napló). Az egységek (W1-A..D, W2-A..D, W3-A/E, S-A..F, R-1..R-12) mind bent vannak; az agy- és sonnet-worker ágak merge-elve és törölve. A VST/FX-fejlesztés külön repóban/sessionben fut (SynthyBoyVST, KickEQ), ide nem tartozik; a host-oldal (CLAP-FX, `.kickplug`) bent van.
Az, ami ember vagy hardveres hallgatás nélkül nem igazolható, a B. szakaszban van, tételenként indokolva.

## B. Ember kell — a pontos lépések a `docs/MORNING.md` "Ember kell" listájában és a `docs/AKAI_MPC_KEY61_HW_TEST_CHECKLIST.md` 20–51. pontjaiban
- **Hallgatás** (indok: üres bemenet, nincs hangszóró-visszacsatolás a gépnek): Demo 01 zongora minden tartományban, képernyő-zongora (glissando/bend/vibrato/sustain), Looper Sync füllel, busz-insertek, XY FX presetek, Pad Perform/poly aftertouch, Sample Edit, Sampler felvétel, az `adsr.h` változás hatása a samplerre.
- **Fizikai** (indok: ujj/gomb kell): strip Track Level ujjal, többujjas érintés, Q-Link/mixer, hardveres REC/OVERDUB/STOP/PLAY, **a bekapcsoló gomb rövid megnyomása és a tényleges Shut Down/Restart (R-12)**, Controller Mode mérés Mac-en.
- **Eszközön, visszafordíthatatlan vagy külső eszközt igényel** (indok: adatvesztés/hálózat): Format/Eject próba-pendrive-on, 300 MB másolás, dátum/idő, Ethernet/WiFi, Factory Reset, Firmware Update, pendrive-os telepítés/rollback.
- **Owner-döntések** (indok: termékdöntés, nem műszaki): lista a lenti "Owner-döntések" sorban és a MORNING.md 4. pontjában (köztük a transzport-duplázás, a hivatalos plugin-aláíró kulcs, `assets/Default`, SW-00).
- **Jogi átnézés**: Salamander CC BY 3.0 ↔ AGPL, Google API ToS, Splice ToS, libcurl/TLS licenc.
- **iOS**: szimulátor/eszköz indulás és külső MIDI a mobil ágon (nem ellenőrzött).
- A Looper loop-óra drift az MPC-n újramérve és megszűnt (KEY61_HW_BEHAVIOUR.md); csak a fülre hallgatás marad a Hallgatás pontban.


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


## Állapot (2026-11-04 08:00) — frissítés
**Pusholva (`3c0b29bf`) és MPC-n ellenőrizve:** minden korábbi + W3-A érintő-zongora (három egyidejű ujj igazolva, két gyökérok javítva), S-A mixer/mute, S-C szekvencer-szerkesztők, W3-E `.kickplug`-telepítő (Plugins fül nyílik), Looper SYNC gyökérok (`MTSequence::play()` stale beat/tick: mért −1,34 s → −0,14 s), duplikált track/program javítás (W1-C + W2-B ütközés), Aura 46 perces soak.
**Helyben mergelve, ARM+Mac+smoke fut, még nincs pusholva:** S-B minta/sampler/assign képernyők.
**Maradék tételek (külön egységek, nem félmegoldás — mindegyik pontos oka a worker-jelentésekben):**
| ID | Tartalom | Végrehajtó / előfeltétel |
|---|---|---|
| R-1 | S-B maradék: AS-03 egész-pad törlés megerősítéssel, AS-10 réteg-jelölők a rácson, AS-11/13/14, SA-04 hardver REC/STOP/PLAY képernyő-kötés a hídban, SA-07 + SA-02 lock-free hullámforma-cache és marker-FIFO (közös a Looperrel: `AudIO/Recorder`), SE-05/SE-23 Chop→asset slice + Assign, SE-07 touch hit-test, SE-11, SE-13, SE-24 stb. | sonnet; a híd és a Recorder érintése miatt egyedül fusson |
| R-2 | Looper másodlagos: a hullámforma-pillanatkép a `gate` spinlock alatt másol (30 Hz) → eldobott blokkok, ~−0,04 s/s drift; dupla puffer / változott szakasz másolása | sonnet + MPC-s mérés |
| R-3 | S-C maradék: Q-Link oszlopváltás és pad-LED réteg a hídban (SS-07, NS-04, SS-22, LE-24), LE-07 sor-virtualizálás, LE-22 többes kijelölés, LE-32 Go-to-playhead | sonnet, a híd egyetlen gazdájaként |
| R-4 | S-A maradék: PM-10/11 + MU-13 automation (T35 előfeltétel), PM-14 ARM-mérés, MU-12 grid-szinkron mute (hallható ellenőrzés), a transzport-duplázás (PM-20, CM-19, MU-09: owner-döntés) | döntés után |
| R-5 | S-D (menu, projects, file-browser, system-resources, header, sw-update), S-E (looper UI, inserts, xy-fx, knob-edit, midi-control), S-F (pad-perform, program-edit) | sonnet, fájlkör szerint egyenként |
**Owner-döntések (új, a jelentésekből):** mixerek közös skálája (MIDI vs dB), Main Volume nem perzisztens trim, Keygroup Pad Mixer, pad-solo perzisztencia, Mixer-elrendezés/Akai-paritás, Time Division mute, transzport-duplázás helye, Song Export tartalma, CLASSIC 127 vs GRID 100 alap-velocity, NS-04 pad-LED irány, Keygroup/MIDI alapszínek, Sampler Export célja, DSP-könyvtár/licenc a Sample Edit Pitch/Time/Bit-reduce-hoz, AS-15 holt kód törlése, SE-21 PLAY-szemantika, a hivatalos plugin-aláíró kulcs (a beépített publikus kulcs helykitöltő, privát kulcs nincs: fail-closed), `assets/Default` módosítása (a "Keyboard" menü-csempe ikonja + theme.xml sor; az eszközön a Default skin fut).


## Állapot (2026-11-04 09:30) — új egység (owner kérése)
| ID | Tartalom | Állapot |
|---|---|---|
| R-12 | **Rendes leállítás a Kickben, mint a gyári MPC-ben**: a bekapcsoló gomb rövid megnyomására "Power" ablak (Shut Down / Restart / Cancel), Menü "Power" csempe, mentetlen-változás kérdés, transzport/felvétel leállítása, projekt-lezárás, `sync`, majd `PlatformServices::powerOff()`; a gomb figyelése (input-eszköz felderítése `/proc/bus/input/devices`-ból, `KEY_POWER`); a logind kettős kezelése ellen a bootstrap `systemd-inhibit --what=handle-power-key` változata | **Kész, merge-elve, telepítve.** MPC-n igazolva: a gomb `gpio-keys` (`/dev/input/event1`, `KEY_POWER`), logind inaktív, nincs `systemd-inhibit`; a Menü 2. oldal "Power" csempéje megnyitja az ablakot (Shut Down / Restart / Cancel), a MAIN gomb zárja, a Kick stabil. A tényleges Shut Down/Restart és a gomb rövid megnyomása a tulajdonos próbája (B) |

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
