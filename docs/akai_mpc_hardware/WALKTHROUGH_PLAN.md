# Terv: az MPC v3.10 útmutató soronkénti végigvétele, párhuzamos fejlesztéssel

**Szabály:** funkció → keresés a dokumentációban → a leírás alapján fejlesztés. Hipotézis nincs: minden állítás mellett bizonyíték áll (útmutató-oldal, kódsor, napló, snapshot, fotó), vagy "nem vizsgált". Az útmutató: *MPC Standalone OS – User Guide v3.10* (528 oldal, ez érvényes az MPC Key 61-re is), [README.md](README.md).

## 1. A feladat egysége: az állítás-lap (claims register)
Minden munkacsomaghoz (WP) egy külön fájl: `docs/akai_mpc_hardware/walkthrough/WP-<betű>.md` (külön fájl = nincs összeolvadási ütközés). Soronként egy állítás:

| ID | Oldal | Az útmutató állítása (rövid parafrázis, max. egy rövid idézet) | Kick megfelelője (fájl:sor vagy képernyő) | Állapot | Bizonyíték | Teendő |
|---|---|---|---|---|---|---|

- **ID:** `<WP>-<sorszám>`, pl. `C-014`. Állandó, a fejlesztés és a teszt erre hivatkozik.
- **Állapot (csak ezek):** `OK` (bizonyítékkal), `ELTÉR`, `HIÁNYZIK`, `NEM-KICK` (az MPC-nek van, a Kicknek nincs megfelelője, a felhasználó dönt), `NEM VIZSGÁLT`.
- **Bizonyíték:** kötelező az `OK` és `ELTÉR` mellé (napló-sor, snapshot-fájl, fotó, kódsor). "Gyanú" nincs.
- **Teendő:** `fejlesztés`, `döntés`, `teszt`, `nincs`.

## 2. Munkacsomagok (az útmutató fejezetei szerint; oldalak a tartalomjegyzékből)
| WP | Terület | Oldalak | Kick-oldali fő fájlok |
|---|---|---|---|
| **A** | Key 61 hardver (felső/hátsó panel), Hardware beállítások | 365–371, 61 | `hardware/akai_map.h`, `led_map.h`, `kick_hardware_bridge.h` (*kész: [KEY61_DOC_VS_KICK.md](KEY61_DOC_VS_KICK.md); kiegészítendő*) |
| **B** | Idő és transzport: Time Counter/Locate, Timing Correct, Metronome, Next Sequence, Song, Looper | 71–74, 316–318, 326–329, 338–341 | bridge (transport, seek), `mt_audio_*` |
| **C** | Pad és játék: 16 Level, Pad Perform, Arpeggiator, Erase, Keyboard Control, Touch Strip, Pad Color | 77–84, 99–104, 336–337 | `strip_modes.h`, `key_transpose.h`, `pad_colour_map.h`, bridge |
| **D** | Mixer és mute: Track/Pad Mixer, Track/Pad Mute, Track View | 137–147, 256–266 | UI modulok, bridge |
| **E** | Szerkesztő módok: Grid, Step Sequencer, List Edit, Track Edit (drum), CV track | 165–212, 252–255 | UI modulok |
| **F** | Minta módok: Sample Edit, Chop, Pad mód, Sampler, Auto Sampler | 267–303 | UI modulok |
| **G** | Menü, Browser, Sounds, Favorites, Projekt, Preferences, Mentés | 53–70, 133–136, 146–147 | UI modulok, `kick_fileBrowser.cpp` |
| **H** | Q-Link, XYFX, MIDI Learn, MIDI Control, Live Control, Crossfader | 148–150, 304–325, 331–335 | bridge, UI modulok |
| **I** | Automation, Effects (insert, send), Audio Mixdown | 75–76, 85–98 | motor |
| **J** | Main Mode (a teljes képernyő) | 106–132 | UI modulok |
| **K** | **Döntés-csomag:** MPC-specifikus részek (Keygroup/Plugin/MIDI/Audio track, Arrange Mode, KG Stack, Mod Matrix) | 42–52, 151–164, 213–251 | – (`NEM-KICK` besorolás, a felhasználó dönt) |

## 3. Fázisok
**0. Előfeltételek (blokkolók, mielőtt a párhuzamos munka kezdődik)** – ezek nélkül az ellenőrzés nem megbízható:
1. Érintés működjön (a kézbesítés hibája, riport 8. kör).
2. Megállított seek (11. hiba).
3. A "melyik gomb ér el az alkalmazásig" napló: minden leképezett gomb `[AKAI-BTN]` sora.
4. A bridge szétbontása területi fejlécekre (pl. `bridge_transport.h`, `bridge_pads.h`, `bridge_strip.h`, `bridge_leds.h`), egyetlen felelőssel, **előbb**, különben mindenki ugyanazt az egy nagy fájlt írja át.

**1. Dokumentum-bejárás (csak olvasás, párhuzamosan)** – WP-nként egy olvasó (sonnet-explorer/worker): a fejezet oldalai → állítás-lap sorai (állítás + a Kick megfelelőjének megkeresése `grep`-pel). Az eredmény egy PR-nyi fájl, kódváltozás nincs. A szöveg-kinyerés: `pdf2txt <pdf> <ki.txt> --pages a-b` (PDFKit; az eszköz oldaltartományra bővítendő). Egyszerre egy olvasó-hullám, a költség miatt.

**2. Döntés (felhasználó)** – WP-nként egy rövid lista: mi az `ELTÉR` / `HIÁNYZIK` / `NEM-KICK` tétel, amit meg kell csinálni, mit hagyunk ki. Itt dől el, hogy a Kick hol tér el szándékosan.

**3. Párhuzamos fejlesztés** – szabályok az ütközés ellen:
- **WP = ág + worktree:** `akai/wp-<betű>`, egy fejlesztő/session csomagonként.
- **Fájl-tulajdon:** minden csomag csak a saját fájljait írja; a közös fájlokhoz (`akai_map.h`, `led_map.h`, `Main.cpp`, motor) **egy sor kérés a postaládába**, az integrátor végzi.
- **Integrátor:** egyetlen session (a cloud session) olvaszt `safian/main`-be, kicsi, gyakori lépésekben; a csomag kész, ha az állítás-lap sorai `OK`.
- **Kapu csomagonként:** tiszta logika host-teszttel (`Kick/tests/platform`), `-fsyntax-only`, majd egy közös ARMv7 `-g` build. Teljes suite csak push előtt.

**4. Hardveres ellenőrzés (közös eszköz: egy MPC)** – az MPC-t egyszerre egy session használja; a többiek a buildjét kérik. Minden csomaghoz egy **teszt-forgatókönyv** a fájl végén: `btn`/`touch`/`strip`/`snapshot` parancsok, a várt képernyő/LED, és mi igényel fizikai nyomást vagy fotót (azt a felhasználó végzi, soronként, párokban, mint eddig). A forgatókönyv újrafuttatható regressziós teszt.

## 4. Szerepek
| Szerep | Ki | Mit |
|---|---|---|
| Dokumentum-olvasó | hardveres session (olvasó-hullám) | állítás-lap sorai, bizonyíték |
| Fejlesztő + integrátor | cloud session | kód, host-teszt, merge |
| Hardveres ellenőr | hardveres session | build, MPC-teszt, kamera/snapshot |
| Döntő és fizikai tesztelő | felhasználó | scope-döntés, fotók, LED/gomb-szemrevétel |

Csatorna: `docs/hw-mailbox/` (TO_CLOUD.md / FROM_CLOUD.md), kérdés `KÉRDÉS:`, válasz `VÁLASZ:`.

## 5. Sorrend (javaslat)
1. **A** (már kész táblázat) kiegészítése a hiányzó sorokkal + a 0. fázis blokkolói.
2. **B, C, D** párhuzamosan olvasva: ezek érintik a hardver gombokat, a legtöbb ismert hibát.
3. **E, F, G, H, I, J** olvasása; közben a B–D fejlesztése.
4. **K** döntés-csomag a végén, a felhasználóval.

## 6. Döntés kell a felhasználótól
1. A **scope**: a Kick a Behringer KICK funkcióit követi (dob + szekvencer + CV), az MPC-specifikus részek (Keygroup, Plugin track, Arrange, Mod Matrix) `NEM-KICK`, hacsak nem kéri másként.
2. A **bridge szétbontása** (0/4.) engedélyezett-e: ez a párhuzamosság előfeltétele.
3. **Egy integrátor** (a cloud session) olvaszt-e `safian/main`-be, a többi ágról.
