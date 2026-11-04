# Reggeli összefoglaló (2026-11-04 éjszaka → 11-05)

Állapot: **folyamatban**: a végleges verzió akkor lesz kész, amikor a TODO A. szekciója kész vagy a B. szakaszba tett indoklással. Utolsó frissítés: 2026-11-04 ~08:00. Minden itt "kész"-nek mondott tétel: merge-elve `safian/main`-re (`a5f1851d`), ARM- és Mac-build zöld, host-tesztek ASAN/UBSAN-nal, az MPC-n legalább indul/nyílik összeomlás nélkül; ahol hangot, LED-et vagy fizikai vezérlőt kell ellenőrizni, azt a "Ember kell" listában jelzem.

## Kész, az MPC-n ellenőrizve
- **Zongora**: a Demo 01 az MPC-n a zongora (több mintás Keygroup, velocity-rétegek), a Piano projekt betöltése összeomlás nélkül megy.
- **Összeomlások javítva (gyökérokkal)**: projektváltás (hiányzó virtuális destruktor az `MTISample`-ben, ASAN), sáv-sor lógó `pTrack`, a Looper képernyő megnyitása (két nullával osztás: üres alapértelmezett nevező/számláló → ARM SIGFPE), duplikált track/program a W1-C és W2-B ütközése miatt.
- **Aura soak**: 46 perc üresjárat, a Kick él (a régi hiba ~25 perc után jött).
- **Looper SYNC**: gyökérok `MTSequence::play()`: csak a takt nullázódott, a beat/tick a STOP-kori maradt → a STOP utáni PLAY a régi pozícióról indult (általános hiba). Javítva, mért eltérés −1,34 s → −0,14 s.
- **Képernyő-zongora** (Menü → Keyboard): két gyökérok javítva (rejtett legfelső ablak találat-tesztje; fehér billentyű találati cella), **három egyidejű ujj** külön hangot ad, külön lenyomás/felengedés; érintés → motor ≈ 42 ms.
- **Oktáv**: két OCT + billentyű akkord, az oktáv-LED sor (a gyári MPC-ből mért: CC 0x5F–0x65), te megerősítetted.
- **Naplózás**: időbélyeg, szálazonosító, kategóriák (`KICK_LOG`), rotáció, lépés-napló, főhangerő-napló; `/data/kick.crash` pc/lr + veremátfésülés.
- **Új funkciók (mind buildelt, host-tesztelt, az MPC-n nyílnak)**: Preferences-csomag (platform-réteg MPC-s útvonalakkal, Plugins fül), Grid-javítások, MIDI program (stabilitás, tartományok), CLAP-FX az Insert-böngészőben + `clap.gui`, indítóképernyő + állapotsor + biztonságos mód (`kick.boot-count`), főképernyő tabok (időjel, Monitor, törlés-megerősítés, undo), fájlböngésző-források (helyi + WebDAV, http), mixer/mute, szekvencer-szerkesztők (Step Seq, List Edit, Next Sequence, Song, Color Edit), Assign/Sampler/Sample Edit javítások, `.kickplug` manifest + Ed25519 + atomi telepítő + visszagörgetés.
- **Mobil app**: a hardverkinézet kivéve, csak teljes képernyős UI + külső MIDI (`feat/mobile-slideout-ui` pusholva a forkra; iOS nem ellenőrzött).
- **Pendrive-telepítő szkriptek** (offline tesztelt 57/57), a Demo 01 mentése: `/data/demos_backup/Demo01.orig`.

## Folyamatban (éjszaka, sonnet; az agy-kvóta elfogyott, ~2 óra a reset)
- S-D: menü / projects / fájlböngésző / system resources / fejléc / sw-update képernyők.
- S-F: pad-perform / program-edit.
- Következők: S-E (looper UI, inserts, xy-fx, knob-edit, midi-control), R-1 (S-B maradék), R-2 (Looper hullámforma-másolás drift), R-3 (S-C maradék).

## Ember kell (pontos lépésekkel)
1. **Hallgatás**: Demo 01 zongora alul/középen/fent, halk/hangos; **képernyő-zongora** (Menü → Keyboard): glissando, bend, vibrato, sustain-gomb, velocity-módok; **Looper**: Sync be → REC 4 ütem → STOP → PLAY: a loop és a szekvencia együtt jár-e a füleddel.
2. **Fizikai**: strip Track Level ujjal; többujjas érintés a kijelzőn; Q-Link/mixer ellenőrzések a worker-jelentésekből (Pad Mixer Q-Link érintés után, mute-csoport, Clear Solos, bank-szinkron); Controller Mode mérés Mac-en a Key 61-gyel.
3. **Eszközön futtatandó, én nem futtattam (rendszer/hálózat)**: Preferences-ben dátum/idő, Ethernet/WiFi, fényerő, Factory Reset, Firmware Update (a hálózat és a rendszer elronthatná az ssh-t/a gépet).
4. **Döntések (owner)**: mixerek közös skálája (MIDI vs dB), Main Volume nem perzisztens trim, Keygroup Pad Mixer, pad-solo perzisztencia/undo, Mixer-elrendezés / Akai-paritás, Time Division mute, **transzport-duplázás** (PLAY/STOP a képernyőkön és a hídban), Song Export tartalma, CLASSIC 127 vs GRID 100 alap-velocity, NS-04 pad-LED irány, Keygroup/MIDI alapszínek, Sampler Export célja, DSP-könyvtár/licenc a Sample Edit Pitch/Time/Bit-reduce-hoz, AS-15 holt kód törlése, SE-21 PLAY-szemantika, **hivatalos plugin-aláíró kulcs** (a beépített publikus kulcs helykitöltő, privát kulcs nincs: fail-closed), `assets/Default` módosítása (új "Keyboard" csempe-ikon + theme.xml; az eszközön a Default skin fut), a Looper hullámforma-másolás átalakítása.
5. **Jogi átnézés**: Salamander CC BY 3.0 ↔ AGPL, Google API ToS, Splice ToS, libcurl/TLS licenc (https az MPC-n nincs TLS-könyvtár miatt).
6. **iOS**: szimulátor/eszköz indulás és külső MIDI a mobil ágon.

## Hibás / nyitott
- **Looper**: a felvétel elején a loop-óra ~0,3 s-ot veszít és lassú drift (≈ −0,04 s/s): gyanú a hullámforma-pillanatkép spinlock alatti másolása (R-2).
- Nem javított, owner-döntés nélkül: transzport-duplázás; a híd Q-Link oszlopváltása; pad-LED réteg (Color Edit/Next Sequence).
- Ismert korlát: WebDAV https az MPC-n nem működik; a jelszó bevitele nyílt szövegben látszik a képernyő-billentyűzeten.
- `kick_hardware_bridge.h` Linux-kód: a Mac-en csak `-fsyntax-only` (stubolt device_factory), a teljes forgatás az ARM-build.
- A Piano-projekt hangzása és minden hangzási/LED-es funkció hallgatással/szemmel nincs igazolva (lásd Ember kell).
