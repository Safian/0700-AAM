# MPC Key 61 – a hardver gombjainak fizikai működése (tudás-napló)

**Cél:** ami a hardverről egyszer ki lett derítve, itt marad, hogy ne kelljen újra megmérni (pl. háromszor egymás után megnyomni a gombot). Ha egy sor azt mondja, hogy fizikailag mért, akkor **a hardver kizárható**, és a mögötte lévő folyamatot (adapter, híd, UI) a felhasználó nélkül lehet vizsgálni: a rögzített nyers MIDI visszajátszható a `rawmidi` paranccsal (lásd 5. szakasz).

**Szabály:** új tény mindig ide kerül, dátummal, módszerrel és a build azonosítójával. Mért tényt és kódból vett feltevést külön jelölünk. Csak append: egy régi bejegyzést nem törlünk, hanem javítunk és megjelöljük.

Jelölések: **mért** = nyers MIDI vagy a Kick naplója fizikai működtetésből; **felhasználó** = a felhasználó szeme/füle; **kód** = térkép a kódban/review-ban, fizikailag nem újramérve.

## 1. Protokoll (mért, 2026-11-02, Key 61, build `1a5e4543`–`f7e8846d`)
- **RawMIDI portok:** `hw:0,0,0` = Port 0 (Public/billentyű), `hw:0,0,1` = Port 1 (Private/vezérlők), `hw:0,0,2` = DIN. A Kick kizárólagosan foglalja őket (`amidi` közben `Device or resource busy`).
- **Indítás:** a Kick `F0 47 7F 4B 60 00 01 41 F7` ("Mode 0x41 Private") üzenetet ír **Port 0-ra és Port 1-re** is. **Mért:** a Kick leállítása után a vezérlő semmit nem küld (0 üzenet 28 s és 40 s alatt, gombokkal és pad-ekkel). A kézi `amidi` újraindítással (egyenként megnyitott és bezárt port) csak a Main Volume SysEx jött meg; ez a mérés nem meggyőző, mert a Kick a portokat nyitva tartja.
- **Csatornák Port 1-en:** ch1 = gombok, Q-Link, Data Dial, Touch Strip; ch10 = pad-ek; ch16 = billentyű, kerekek, pedál, aftertouch. **Port 0 néma** Private módban (mért: a billentyűk Port 1 ch16-on jönnek, Port 0-n 0 üzenet).
- **LED-protokoll:** `B0 <jegyzet> <érték>` (cc mód) működik, a `90`-es (note) nem (korábbi hardveres mérés). Érték: egyszínű LED 0 / 1 / 127; kétszínű 0..4. A halvány (1) és a teljes (127) fény **szemmel nem különböztethető meg** a Touch Strip gomb LED-jén (felhasználó, 2026-11-02): a hardver gyakorlatilag csak be/ki.
- **Main Volume gomb:** SysEx `F0 47 7F 4B 5B 00 01 <v> F7`, `v` = 0x00..0x75 (mért).
- **Data Dial:** `B0 64 01` (+1) / `B0 64 7F` (−1), CC100 relatív, ch1; nyomás = jegyzet 111 (mért).
- **Q-Link 1–4:** CC16–19 relatív, ch1; érintés = jegyzet 84–87 be/ki (mért).
- **Touch Strip:** pozíció CC33 (MSB) + CC34 (LSB), 14 bit, ch1; ujj érintés/elengedés = jegyzet 78 (mért).
- **Kerekek, pedál, billentyű:** Pitch Bend ch16 (14 bit); Mod Wheel CC1 ch16; szusztain CC64 ch16; billentyű Note On/Off ch16 (vel 75–97 a mintában); Channel Pressure ch16 (mért).
- **Pad-ek:** Note On/Off ch10 + Poly Aftertouch; pad-jegyzetek az `akai_map.h` `get_akai_pad_index` szerint (pad 1 = 37 …) (mért: pad 1 = 37).
- **Main Volume nem MIDI-CC**, hanem SysEx; a hardver analóg hangerejét az Akai OS külön kezeli, a Kickben a master busz szintjét állítja (javítva, `898a122a`).

## 2. A hardver gombjai
Forrás: `Kick/apps/kick/hardware/akai_map.h` és `led_map.h` (generálva, a LED jegyzete megegyezik a gombéval). A "Fizikai bizonyíték" oszlop mondja meg, mi van tényleg megmérve.

| Gomb (Kick név) | Jegyzet (bejövő = LED, ch1, Port 1) | LED | Fizikai bizonyíték |
|---|---|---|---|
| QLINK | 0 | egyszínű (0 / 1 / 127) | nincs nyersen mérve (térkép: review/kód) |
| PROG_EDIT | 2 | egyszínű (0 / 1 / 127) | nincs nyersen mérve (térkép: review/kód) |
| TOUCH_STRIP | 4 | egyszínű (0 / 1 / 127) | a gomb jele megérkezett (Kick log); a LED Off módban kialszik (felhasználó, 2026-11-02) |
| SAMPLE_EDIT | 6 | egyszínű (0 / 1 / 127) | nincs nyersen mérve (térkép: review/kód) |
| OCTAVE_DOWN | 7 | egyszínű (0 / 1 / 127) | nincs nyersen mérve (térkép: review/kód) |
| ERASE | 9 | egyszínű (0 / 1 / 127) | hideg indítás után működik (felhasználó, 2026-11-02, build `0947f1f5`), lásd 4. szakasz |
| OCTAVE_UP | 10 | egyszínű (0 / 1 / 127) | nincs nyersen mérve (térkép: review/kód) |
| NOTE_REPEAT | 11 | egyszínű (0 / 1 / 127) | nincs nyersen mérve (térkép: review/kód) |
| TC | 12 | egyszínű (0 / 1 / 127) | nincs nyersen mérve (térkép: review/kód) |
| BANK_A | 35 | kétszínű 0..4 | nincs nyersen mérve (térkép: review/kód) |
| BANK_B | 36 | kétszínű 0..4 | nincs nyersen mérve (térkép: review/kód) |
| BANK_C | 37 | kétszínű 0..4 | nincs nyersen mérve (térkép: review/kód) |
| BANK_D | 38 | kétszínű 0..4 | nincs nyersen mérve (térkép: review/kód) |
| FULL_LEVEL | 39 | egyszínű (0 / 1 / 127) | hideg indítás után működik (felhasználó, 2026-11-02, build `0947f1f5`), lásd 4. szakasz |
| 16_LEVELS | 40 | egyszínű (0 / 1 / 127) | gyári indítási sorozat nélkül Port 0-n jön, a sorozattal Port 1-en (Shift mérve), lásd 4. szakasz |
| TOUCH_FX | 42 | egyszínű (0 / 1 / 127) | nincs nyersen mérve (térkép: review/kód) |
| TRACK_MUTE | 43 | kétszínű 0..4 | nincs nyersen mérve (térkép: review/kód) |
| SHIFT – Right Shift | 49 | nincs LED-leképezés | nincs nyersen mérve (térkép: review/kód) |
| SHIFT – Left Shift | 50 | egyszínű (0 / 1 / 127) | hideg indítás után működik (felhasználó, 2026-11-02, build `0947f1f5`), lásd 4. szakasz |
| MAIN | 52 | egyszínű (0 / 1 / 127) | hideg indítás után működik (felhasználó, 2026-11-02, build `0947f1f5`), lásd 4. szakasz |
| TAP_TEMPO | 53 | egyszínű (0 / 1 / 127) | nincs nyersen mérve (térkép: review/kód) |
| PLUS | 54 | nincs LED-leképezés | nyers MIDI, 2026-11-02 |
| MINUS | 55 | nincs LED-leképezés | nyers MIDI, 2026-11-02 |
| UNDO | 67 | egyszínű (0 / 1 / 127) | nincs nyersen mérve (térkép: review/kód) |
| STEP_PREV – < | 68 | nincs LED-leképezés | nincs nyersen mérve (térkép: review/kód) |
| STEP_NEXT – > | 69 | nincs LED-leképezés | nincs nyersen mérve (térkép: review/kód) |
| LOCATE | 70 | egyszínű (0 / 1 / 127) | nincs nyersen mérve (térkép: review/kód) |
| BAR_PREV – << | 71 | nincs LED-leképezés | nincs nyersen mérve (térkép: review/kód) |
| BAR_NEXT – >> | 72 | nincs LED-leképezés | nincs nyersen mérve (térkép: review/kód) |
| REC | 73 | kétszínű 0..4 | REC: a gomb jele megérkezett 2026-11-02 (Kick log, fizikai) |
| AUTOMATION | 75 | kétszínű 0..4 | nincs nyersen mérve (térkép: review/kód) |
| OVERDUB | 80 | kétszínű 0..4 | OVERDUB: a gomb jele megérkezett 2026-11-02 (Kick log, fizikai) |
| STOP | 81 | egyszínű (0 / 1 / 127) | STOP: a gomb jele megérkezett 2026-11-02 (Kick log, fizikai) |
| PLAY | 82 | kétszínű 0..4 | PLAY: a gomb jele megérkezett 2026-11-02 (Kick log, fizikai) |
| PLAY_START | 83 | nincs LED-leképezés | nincs nyersen mérve (térkép: review/kód) |
| ENTER – Data Dial Push | 111 | nincs LED-leképezés | Data Dial nyomás: nyers MIDI és Kick log, 2026-11-02 |
| METRONOME | 112 | egyszínű (0 / 1 / 127) | nincs nyersen mérve (térkép: review/kód) |
| TRACK_MIX | 116 | kétszínű 0..4 | nincs nyersen mérve (térkép: review/kód) |
| GRID | 117 | egyszínű (0 / 1 / 127) | nincs nyersen mérve (térkép: review/kód) |
| ARP | 118 | egyszínű (0 / 1 / 127) | nincs nyersen mérve (térkép: review/kód) |
| LATCH | 119 | egyszínű (0 / 1 / 127) | nincs nyersen mérve (térkép: review/kód) |
| KEYBOARD_CONTROL | 120 | egyszínű (0 / 1 / 127) | nincs nyersen mérve (térkép: review/kód) |
| SOUNDS | 121 | egyszínű (0 / 1 / 127) | gyári indítási sorozat nélkül Port 0-n jön, a sorozattal Port 1-en (Shift mérve), lásd 4. szakasz |
| COPY | 122 | egyszínű (0 / 1 / 127) | hideg indítás után működik (felhasználó, 2026-11-02, build `0947f1f5`), lásd 4. szakasz |
| MENU | 123 | egyszínű (0 / 1 / 127) | hideg indítás után működik (felhasználó, 2026-11-02, build `0947f1f5`), lásd 4. szakasz |

A pad-ek, a Q-Link gomb, a Data Dial és a strip külön szakaszban (1.) szerepelnek.

## 3. Mért viselkedés (összefoglaló)
- **Shift:** csak nyomva tartva él (az útmutató szerint), két gomb (49 jobb, 50 bal) ugyanazt az állapotot adja. Hideg indítás után a jobb Shift (49) Port 1-en jön, a LED felvillan (mért, 4. szakasz).
- **Touch Strip gomb LED:** Off módban kialszik (felhasználó). A strip melletti piros LED-pöttyök az útmutató szerint nem vezérelhetők (a MIDI Control Mode "Light LED" leírása szerint nincs funkciója); normál módban az útmutató nem ír róluk.
- **Transzport blokk (PLAY 82, STOP 81, REC 73, OVERDUB 80):** a jelük megérkezik a Kick-first indulásban is.

## 4. Kick-first indulás (2026-11-02): MEGOLDVA a gyári indítási sorozattal
**Mért ok (nyers MIDI, `captures/key61_sweep_kick_first_20261102.txt`):** a pendrive-os indulás után (a Kick a gyári MPC-alkalmazás nélkül indul) a vezérlő **nincs Private módban**: a gombok többsége **Port 0-n (Public), ch1 Note On/Off** jön, **ugyanazokkal a jegyzetszámokkal, mint a térképben**, és nem Port 1-en:

| Gomb | Port 0 üzenet |
|---|---|
| Shift (jobb) | `90 31` (49) |
| Full Level | `90 27` (39) |
| 16 Levels | `90 28` (40) |
| Erase | `90 09` (9) |
| Copy | `90 7A` (122) |
| Menu | `90 7B` (123) |
| Sounds | `90 79` (121) |
| Main | `90 34` (52) |
| Undo | `90 43` (67) |
| Tap Tempo | `90 35` (53) |
| Play | Port 0 `B0 76 7F/00` **és** Port 1 `90 52 7F/00` (82) |
| Stop | Port 0 `B0 75 7F/00` **és** Port 1 `90 51 7F/00` (81) |

Csak a transzport gombok (Play, Stop, Rec, Overdub) jönnek Port 1-en is, ezért ezek működtek. A Kick a Port 0 jegyzeteit **billentyűnek** veszi, ezért szólalt meg az Erase (jegyzet 9) sample-t. A LED-ek is kimaradtak a vezérlő állapota miatt (a Shift és a Full Level LED sötét maradt).
**Következmény:** a Kick által küldött `F0 47 7F 4B 60 00 01 41 F7` ("Mode 0x41 Private: Port 1 Private be, Port 0 Public ki") ebben az állapotban **nem hat** (a Port 0 továbbra is él, a gombok nem a Port 1-re kerülnek). A Port 0 jegyzeteit nem lehet egyszerűen gombnak venni, mert a billentyűk jegyzettartományával ütköznek (pl. 39, 40).
**Korábban:** ugyanezek a gombok működtek, amikor a Kick-et a gyári MPC-alkalmazás leállítása után kézzel indítottam (a vezérlőt előtte az MPC-alkalmazás inicializálta).
**Megoldás (mért):** a gyári MPC-alkalmazás induláskor a Port 0-ra (kimenet) egy **MIDI Identity Requestet és konfigurációs SysEx-eket** küld (21 üzenet, `captures/key61_stock_mpc_startup_20261102.txt`, rögzítve az `LD_PRELOAD`-os `tools/mpcsnoop.c`-vel). A Kick adaptere most ugyanezt küldi el ugyanabban a sorrendben (a második csoport 1 s után). **Hideg indítás után (kikapcsolás, bekapcsolás, pendrive) a Shift már Port 1-en jön (`90 31`, `[AKAI-BTN] note=49`), és a Shift LED felvillan (piros, felhasználó).** A hipotézis tehát beigazolódott: a vezérlő addig Public módban marad, amíg a gyári kézfogást nem kapja meg, és a Kick `4B 60 00 01 41` módparancsa önmagában nem elég.
**Nem tudjuk:** a 21 üzenet közül melyik a lényeges (az Identity Request, a `4B 62/63/64/41/5B/5A` konfiguráció, vagy mind). Nem szűkítettük, mert a teljes sorozat a gyári viselkedés, és mind beküldése biztonságos.
**Következtetés:** a pendrive-os és bármely más Kick-first indítás ugyanazt a bináris kódot használja, a hardver-előkészítés az adapterben él, az indító réteg csak a szkript.

## 5. Mérési módszerek (hogy ne kelljen a felhasználót kérni)
1. **Nyers MIDI a Kick alatt:** az adapter ideiglenes `[AKAI-RAW] port=<n> <status> <d0> <d1>` naplósora (az első 600 üzenet). A felhasználó egyetlen sorrendben végigmegy a gombokon, a naplót elmentjük ide.
2. **Visszajátszás:** a rögzített üzenetek a `kick_cmd` FIFO-ba írt `rawmidi <port> <hex bájtok>` paranccsal a Kick feldolgozóján mennek át (pl. `rawmidi 1 90 52 7F`). Ezzel a felhasználó nélkül vizsgálható az adapter és a híd.
3. **Gomb-injektálás:** `btn <NÉV>` (nyomás+elengedés egyszerre; a Shift-kombináció így nem injektálható).
4. **LED-ellenőrzés:** `btn`-nel kiváltott LED-írás, kamerával (Photo Booth, nem Continuity Camera) vagy a felhasználó szemével.
5. **Képernyő:** `snapshot <fájl>`, a PPM-et PNG-re alakítva.

## 6. Mérési napló (append-only)
| Dátum | Módszer | Eredmény |
|---|---|---|
| 2026-11-02 | `amidi -d -t 100` Port 1, Kick leállítva, a felhasználó végigment a vezérlőkön | 1609 üzenet: Data Dial, +/−, Q-Link, strip, kerekek, pedál, billentyű (ch16), pad, Main Volume SysEx (1. szakasz) |
| 2026-11-02 | `amidi` Port 1, Kick leállítva, indító SysEx nélkül | 0 üzenet |
| 2026-11-02 | `amidi` Port 1, indító SysEx kézzel, 40 s | csak Main Volume SysEx (19 db); nem meggyőző |
| 2026-11-02 | Kick-napló a pendrive-os indulás után, felhasználó nyomkodása | csak 82/81/73/80 érkezik Port 1-en, a többi gomb Port 0-n (Public mód); Erase = sample (4. szakasz) |
| 2026-11-02 | hideg indítás után a felhasználó kipróbálta a Full Level gombot (LED világít, a pad 127), a Shift + Full Level-t (Half Level, 64), az Erase, Copy, Menu és Main gombot | mind működik (felhasználó); a középső blokk Port 1-en jön, a LED-ek működnek (build `0947f1f5`) |
| 2026-11-02 | `LD_PRELOAD`-os figyelő (`tools/mpcsnoop.c`) a gyári MPC-alkalmazáson, indulás | 21 indító üzenet Port 0-ra, köztük Identity Request; a Kick módparancsa nincs benne (`captures/key61_stock_mpc_startup_20261102.txt`) |
| 2026-11-02 | hideg indítás (kikapcsolás, bekapcsolás) a gyári sorozatot küldő Kick-kel, a felhasználó egyszer megnyomta a Shift-et | Shift Port 1-en jön, a LED felvillan: a Port 0 / Public állapot megszűnt (4. szakasz) |
| 2026-11-02 | `[AKAI-RAW]` napló, a felhasználó egyszer végigment: Shift, Full Level, 16 Levels, Erase, Copy, Menu, Sounds, Main, Undo, Tap Tempo, Play, Stop | a gombok Port 0 ch1 Note On, azonos jegyzetszámokkal; elmentve: `captures/key61_sweep_kick_first_20261102.txt` |
| 2026-11-03 | `/proc/mounts`, a Kick fájlböngészője | SD-kártya: `/sdcard`; USB-pendrive: `/media/PenDrive` (a Kick ezekből találja meg a külső tárolókat); a mappaméret-számolás exponenciális volt, int64-es, `RangedDirectoryIterator`-os változattal javítva (build `688245fb`, `580620c3`) |
| 2026-11-03 | Keygroup-lejátszás injektált `key` parancsokkal és hallgatás, build `e1448d49`, `b101e5b8` | egyetlen minta: kromatikus, gyökér C3 (60); több minta: zóna (minNote..maxNote), különben a legközelebbi, gyökér a minta keyNote-ja; velocity-rétegek `layerPlay==VELOCITY`-nél a velStart..velEnd szerint; a felhasználó megerősítette a hangmagasságot |
| 2026-11-03 | projektváltás összeomlás (minden projektnél), ASAN-nal Macen reprodukálva | gyökérok: az `MTISample`-nek nem volt virtuális destruktora (a programok `unique_ptr<MTISample>`-ben tartják az `MTIDrum`/`MTIKeys`/`MTIClip` példányokat), a ValueTree-figyelők megmaradtak; javítva (`8ea4af11`) |
| 2026-11-04 | `/data/kick.crash` (pc, lr, veremátfésülés) a Piano projekt betöltésénél | az `AppTrackDisplay::refreshSoloMute` a lezárt projektre mutató `pTrack`-ot használta (a sávlista kisebb lett); a sor most újra lekéri a sávot; a Piano projekt betöltése azóta összeomlás nélkül megy (`8ea4af11`) |
| 2026-11-04 | a felhasználó játszott, majd nem szólt a zongora | a motor és az ALSA futott, a sáv szintmérője kilengett; ok: a **Main Volume gombot lehúzták** (SysEx `F0 47 7F 4B 5B 00 01 <v> F7` → `output(0)->gain`); injektált SysEx (`rawmidi 1 F0 47 7F 4B 5B 00 01 50 F7`) után szólt. A főhangerő-változás azóta naplózódik (`Main volume N/127 -> gain G`) |
| 2026-11-04 | `LD_PRELOAD`-os figyelő (`tools/mpcsnoop.c`) a gyári MPC-alkalmazáson, a felhasználó az OCT−/OCT+ gombokkal −3..+3 között járt, közben a gombok alatti fényeket nézte (`captures/key61_octave_leds_20261104.txt`) | az **oktáv-sor LED-jei** Port 1 (Control Change, csatorna 0) írásai: **CC 0x5F, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65 = −3, −2, −1, 0, +1, +2, +3**, érték **0x7F = világít, 0 = nem**. A 0-tól az aktuális oktávig minden LED világít, a 0 (0x62) mindig (−3-nál −3, −2, −1 és 0; +2-nél 0, +1, +2); egy OCT-nyomás egy LED-et kapcsol. Az indító blokkban `B0 61 7F B0 62 7F` áll (a gyári alkalmazás az előző oktávját, −1-et őrizte meg, ez a fotón látott "két piros pont"). A gombok saját LED-je (CC 7 = OCT−, CC 10 = OCT+) 3 = világít, 1 = nem: az a oldal világít, amelyik irányban az oktáv 0-tól eltér, a nyomás közben is. A Kickben (`led_map.h`, `HW_LED_OCTAVE_M3..P3`) bekötve |
| 2026-11-04 | strip Track Level: `btn TOUCH_STRIP` ×4 (mode 4), `strip 20`, `strip 110`, `strip 100`, snapshotok (Track képernyő, LEVEL tárcsa), build `ac657b91`-előtti `main` | a sáv helyzete 1:1 a kijelölt sáv Track Level-je (0..127): a LEVEL tárcsa 100 → 20 → 110, a tárcsa elfordul; a mód léptetése a naplóban `[STRIP] mode N`. Fizikai ujj nélkül, injektált értékkel igazolva; a sáv érintésének nyers útja korábban mért (Data Dial/strip) |
| 2026-11-04 | Looper SYNC fázis (Z2b): ideiglenes `[LOOP]` nyomkövetés (szekvencia BBT + looper `progress` 1/3 s-onként, 96 tick/ütem, 120 BPM, 4 ütemes loop), Sync be, `btn REC` 11 s, `btn STOP`, `btn PLAY` 9 s; build az `ac657b91`-et követő `main` + a Looper-javítások | **Felvétel (REC, Sync be):** a looper és a szekvencia együtt indul, a ciklushatárok egyszerre ugranak, az eltérés (loop − szekvencia) medián −0,15 s, ±0,2 s-os mintavételi szórással. **Lejátszás (STOP után PLAY, Sync be):** a szekvencia **~1,3 s-mal előrébb** jár a loopnál végig (medián −1,34 s, −1,17…−1,52); a ciklushatárok is ennyivel csúsznak. Az újraindítás kikényszerítése (`setSequencePlay(sel, true, true)` a `looperPlayback()` előtt) NEM változtatott rajta (−1,42 → −1,34 s) → nem az indítás sorrendje; a looper lejátszási oldalát kell vizsgálni (`Recorder::startPreview` / `progress` az olvasó pozícióból). A kézikönyv szerint Sync mellett a Looper a transzporttal lépésben marad. Hallgatással nem ellenőrizve (üres bemenet). **HIBA, nyitott**. A nyomkövető patch: nincs commitolva |
| 2026-11-04 | Looper SYNC fázis újramérése a javítás után (`MTSequence::play()` bar+beat+tick nullázás, `agy/looperphase` `4949d88f`), ugyanaz a forgatókönyv és `[LOOP]` nyomkövetés (Sync be, `btn REC` 11 s, `btn STOP`, `btn PLAY` 9 s) | **Gyökérok:** `MTSequence::play()` csak a `playhead.bnb.bar`-t nullázta, a beat és a tick a STOP-kori maradt, a rákövetkező `seek(bnb)` onnan indította a szekvenciát (a mért 1,17 s = 2,34 beat × 0,5 s). **Javítás igazolva:** a PLAY utáni első minta `bbt=1:1:48` (előtte `1:3:9x`), a lejátszási eltérés (loop − szekvencia) medián **−0,14 s** (előtte −1,34 s), egyezik a felvételi fázissal (−0,20 s). A hiba általános volt (bármely sima STOP után a PLAY az ütemen belüli régi pozícióról indult), nem csak Looper SYNC-nél. Hallgatással nem ellenőrizve. **Nyitott, másodlagos:** a loop-óra a felvétel elején ~0,3 s-ot veszít, és lassú drift (≈ −0,04 s/s): gyanú a `Recorder::getAudioBuffer()` spinlock alatti másolása a 30 Hz-es hullámforma-rajzolásból (a `process()` try-lock bukik, a blokk kimarad) |
| 2026-11-04 | Képernyő-zongora (Menü "Keyboard", `MenuPiano`) injektált többujjas érintéssel (`latency 1`, `touch_down <slot> X Y`, `touch_up <slot>`), ARM-build a `07e0e072` merge-ből | **Két gyökérok javítva:** (1) a widget saját találat-tesztje `reallyContains()` volt, ami a legfelső (rejtett) ablakot kérdezi → minden érintés a −1-es billentyűre esett (`getComponentAt` nullptr, ha a top-level nem látható); (2) a fehér billentyűk találati cellája 1 egység széles volt, a megrajzolt billentyű 1,5–2 egység → a jobb 33–50% "lyuk". **Igazolva:** három egyidejű ujj (slot 0/1/2 → key 8/14/19) három külön `note-on`, külön `note-off`, a billentyű kiemelve, nincs összeomlás. **Mért késleltetés:** az érintés beolvasásától az `addLiveMidiNote` hívásig ≈ 42 ms (a hívás 51–57 µs); ez az üzenetszál 30 Hz-es feldolgozása, a hallható késleltetés ehhez adja a hangpuffert (nem mérhető a Kick-alkalmazásban). Hangzás/glissando/bend/sustain hallgatással nem ellenőrizve |
