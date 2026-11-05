# Terv 04 — Képernyő-audit és fejlesztési tervek (indítóképernyő, főképernyő tabjai, Grid, érintő-zongora, MIDI program hozzáadása)

Állapot: terv. Alap: **statikus kódolvasás** (build/futtatás/hardver nélkül) — a „KÉSZ” azt jelenti: van bekötött handler az engine-ig, nem azt, hogy hardveren igazolt. A hardveres igazolás a `research/known-issues-and-roadmap.md`-ben. Részletes bizonyítékok (fájl:sor): `research/screen-inventory.md`, `research/midi-program-add.md`, `research/grid-and-piano-view.md`. Ahol a régi `docs/hardware/key61/*.md` és a kód ellentmond, a **kód az igazság**.

## 0. Prioritások (összefoglaló)
1. Indítás: nincs indítóképernyő; két gyanús versenyhelyzet (dupla betöltés, üres képernyő); a hardver-/hang-init hibák láthatatlanok.
2. Grid: dupla bank-eltolás (csak az A bank jó), COPY/ERASE kétszer fut, a Magnify eszköz az MPC-n halott, Q-Link zoom gyakorlatilag nem működik.
3. Főképernyő: Track „Monitor” gomb, Sequence „Time Signature” stub; a program-/track-létrehozás undo-ja nem követi a motor állapotát.
4. MIDI program: működik, de nincs program-szintű note/velocity-tartomány/transzpozíció, adatverseny a MIDI-kimenet listán, eltűnő port jelzés nélkül.
5. Érintő-zongora: nincs használatban lévő zongora-nézet (a `KeyboardComponent` nincs bekötve); a touch az MPC-n egyujjas.

---
## 1. Indítóképernyő
> **ÁLLAPOT (2026-11-04, ág `sonnet/w2a-startup`, Mac-buildig és host-tesztig ellenőrizve, MPC-n még nem):** a lenti 1–6 javaslat megvalósult. Közös állapot-forrás: `Kick/modules/common/source/kick_startup_state.h` (lépések, hiba-szövegek ok+teendő, build-azonosító a `/data/Kick.version`-ből); biztonságos mód: `kick_boot_guard.h` (`/data/kick.boot-count`, a 30 s stabil futás törli; 2 egymás utáni instabil indulás + `/data/kick.crash` → projekt nélküli indulás, az ok és a crash-fájl útja felugró üzenetben); állapot-csík, betöltési sor és az egyetlen indulási projekt-döntés: `kick_status_strip.h`. UI: `kick_ui/dialogs/kick_StartupScreen.*`, betöltés: `recent_project/kick_RecentProject_Utils.*` (`LoadProjectAndAnimation` = az egyetlen, sorba állított útvonal). A híd (`kick_hardware_bridge.h`) már nem tölt és nem kényszerít képernyőt; az init-hibákat az állapot-forrásba és a `kick_log("error", ...)`-ba jelenti. Host-teszt: `Kick/tests/platform/StartupTests.cpp`.
**Most:** nincs splash/logó/engine-betöltés kijelzés. Az első nézet a Projects lista (demók, sablonok, legutóbbiak, új projekt, Auto Load). MPC-n a híd ~1 s (30. képkocka) után kényszeríti a HomeListView-t, és ha van `/data/demos/Demo 01/Demo 01.kickPrj`, azt szinkron tölti be az üzenetszálon, animáció és `isProjectLoading` nélkül (`kick_hardware_bridge.h:674-691`). Projekt nélkül az előlapi gombok holtak (`kick_AppHome.cpp:487`).
**Hibák/gyanúk:** dupla `loadProject` (auto-load + híd egyszerre); a lusta képernyő-betöltés miatt a kényszerített váltás előtt a régi nézet maradhat (üres képernyő); projektbetöltési hiba nem jelenik meg (csak `stopAnimation()`); hardver-init hiba (framebuffer, touch, MIDI, ALSA) csak `perror`, a híd eldobja a visszatérési értéket; a hangeszköz-hibát semmi nem olvassa; sablon-másolás felülír azonos nevű projektet, `sourceFiles[0]` nincs ellenőrizve.
**Javaslataim (sorrendben):**
1. **Valódi indítóképernyő** márkával, verzióval/build-hash-sel, és lépésenkénti állapottal („Audio engine… MIDI… Kijelző… Projekt”), egy közös állapot-forrásból (nincs hardveres időzítés-hack).
2. **Egyetlen betöltési útvonal**: a híd ne töltsön külön; az auto-load kérés egy sorba megy, `isProjectLoading` mindig áll, a betöltés a háttérszálon, progress-szel.
3. **Hibák láthatóvá tétele**: hangeszköz/ MIDI/ touch/ framebuffer init hibák és projekt-betöltési hibák felugró üzenettel (ok + teendő), nem csendes fallback.
4. **Állapot-csík**: csatlakoztatott MIDI-eszközök, hangeszköz, tárhely (`/data` szabad hely), a pendrive-os telepítés/frissítés állapota (lásd 01-terv: „telepítve / frissítve A→B”).
5. **Gyors újraindulás**: utolsó projekt egy érintéssel, a „Projekt nélkül halott gombok” helyett legyen értelmes alapviselkedés (pl. gombok a Projects listában navigálnak).
6. **Vészhelyzet**: ha két egymást követő indítás összeomlik, biztonságos mód (projekt nélkül indul, jelzi az okot, csatolja a crash-fájlt).

## 2. Főképernyő — Sequence / Track / Program tabok
Közös: fül- és eszköztár (Add/Delete/Undo/Redo), egy globális UndoManager; a Delete **megerősítés nélkül** törli a sort (`kick_AppMainMode.cpp:202-220`); `handleAsyncUpdate` üres.
### 2.1 Sequence tab
- **KÉSZ:** lista (név, BPM, loop, SEQ/GLO, transzponálás), Half/Double length, Insert bars, Copy events/bars/sequence, Clear (`kick_AppSequenceOptions.cpp:355-440`).
- **NEM MEGY:** Time Signature opció üres (`:366`, a csempe látszik); Bounce kikommentezve.
- **Hiányzik / terv (Akai-eltérés, `doc-vs-kick.md`, WP-D/G):** sequence-szintű beállítások, count-in, SEQ REC ARM, Next Sequence mód finomságai; Song konvertálás (`Convert >SEQ` stub).
- **Javaslat:** működő Time Signature (kiinduló: taktusméret-váltás a sequence hosszához és a metronómhoz igazítva), törlés-megerősítés vagy „Undo-t kínáló toast”, Sequence-duplikálás/ átrendezés húzással, vizuális loop/ tempó állapot.
### 2.2 Track tab
- **KÉSZ:** név, program-választó, hossz, transzponálás, szint, mute, solo, rec-arm, I/P MIDI port+csatorna, mérő; Half/Double speed, Explode, Pitch quantize, Humanize, Split, Copy, Random, Erase (`kick_AppTrackOptions.cpp:172-300`).
- **NEM MEGY:** **Monitor** gomb (nincs handler, `kick_AppTrackComponent.cpp:26,136,272`); Bounce kikommentezve; Split ablak megszakításkor szivárog (kisebb).
- **Hiányzik / terv (Akai):** Monitor Off/In/Auto/Merge, Output mező, Key Range/Learn, Velocity Range, MIDI Filter/CC Filter, Track Duplicate, Track as MIDI export (WP-D/G hivatkozások a `research/midi-program-add.md` 6. pontjában).
- **Javaslat:** Monitor megvalósítása (a MIDI-trackeknél a bemenet átengedése/ elnyelése), track-szintű tartomány- és szűrő-beállítások (a program-szintű hiány helyett itt kezdve), track-szín/ csoportosítás, gyors mute/solo-csoportok.
### 2.3 Program tab
- **KÉSZ:** név, route, assign, edit, sampler, MIDI csatorna/bank/port, CV portok; Copy pads, Duplicate, Duplicate to track, Pad colour. A szerkesztő típus szerint (Drum → Sample Edit; Keygroup/Midi/Cv → Pad Perform; Clip/Audio → Program Edit; Plugin → CLAP modal).
- **NEM MEGY:** a **Keygroup szűrő/burkoló/LFO/Layer Mix** oldalak a modellre kötöttek, de az engine figyelmen kívül hagyja (a zongora-terv, 02, külön motort ír le); a CV szerkesztő csak olvasható.
- **Javaslat:** a Keygroup-oldalakat vagy bekötni (valódi ADSR/szűrő/LFO a hangban) vagy elrejteni, amíg nem működnek (félrevezető kezelőszervek); a Plugin-szerkesztő `clap.gui`-val (lásd 02/03).

## 3. Grid
**Fontos pontosítás:** a Kick (és az Akai MPC Standalone) Grid-je **eseményszerkesztő** (piano roll: jegyzetek, velocity), **nem clip-indító** — clip/scene indító nincs, és az Akai-ban sincs. A „Next Sequence/ Song/ Pad Mute/ Track Mute” a legközelebbi indító-jellegű funkció.
- **KÉSZ:** Drum és Keys nézet; ceruza/radír/kijelölés; cut/copy/paste; Edit Start/End, Transpose, Nudge, Don't Snap, TIME DIV; zoom/görgetés, playhead, Auto-Scroll; Snap Mode; Velocity/Probability/Ratchet sáv; Clear, Double Length, Half/Double Speed, Pitch Quantize, Random, Split.
- **RÉSZLEGES:** Humanize-ból hiányzik a Velocity (a motor tudja, `mt_audio_note.cpp:1660`); jegyzet-némítás csak asztali billentyűre; Q-Link csak az 1–4 jut el (1–2 görget, 3–4 zoomol; BBT/metronóm 5–10 nem).
- **STUB:** Trim To Length, Convert To Progression, Bounce To Sample, Bounce To Audio Track; a `GridCopyTool` elérhetetlen (nincs gomb); hardveres Data Dial/knob ág `///TBD`; `TimeSigSetting` bekötetlen.
- **BUG (kód alapján):**
  1. dupla bank-eltolás (`kick_AppGrid.cpp:542` és `:558`) → a nem-A bankokon rossz sor; ez magyarázhatja a „Grid csak az A bankot mutatja” megfigyelést;
  2. COPY és ERASE kétszer fut (press+release is ér el `:503-513`);
  3. Magnify eszköz az MPC-n halott (`isTouch()`-ra épül, a manuális touch-út nem touch-forrás);
  4. Q-Link zoom-lépés `value/100` (a Key 61 deltája ±1 → nem zoomol);
  5. Split popup szivárog.
- **Akai-hiányok:** audio-track Grid, Select All, Duplicate, Solo, Clear Auto, TC ablak a Gridből, Q-Link Playhead Increment és MIDI Rec Erase, automation „Add New”, aktív terület-zoom.
- **Javaslat:** (a) a 4 bug javítása + teszt (a bank-eltolás és a gombnyomás-állapot tiszta egységteszt), (b) a Magnify eszköz a per-ujj touch-forráshoz kötve (lásd 4. pont), (c) stubok megvalósítása vagy a menüből eltüntetése, (d) Velocity a Humanize-ban, (e) klip-/jegyzet-szintű vizuális visszajelzés (kijelölt hangok száma, snap-érték).

## 4. Érintő-zongora („zongora nézet”, a kijelzőn is játszható)
**Most:** nincs használatban lévő zongora-nézet. A `ui::KeyboardComponent` (`kick_ui_components/source/kick_KeyboardComponent.*`) lefordul, de sehol nincs példányosítva; tud oktáv-gombokat, hely-alapú velocity-t, ujjanként követett hangokat; **nem** tud glissandót, pitch bendet, sustaint, skála-jelölést, külső hang rajzolását. A Pad Perform képernyő-pad érintése nem szólaltat meg hangot. A régi `apps/mpc_gui` prototípus (61 billentyű, 10 ujj, csúszás-váltás) kódja minta, de nincs a buildben. Az MPC Standalone-ban nincs képernyő-zongora.
**Érintő-bemenet:** a hardver ILI2117, 10 MT-slot; a híd „peer” úton per-ujj forrást ad, de a „manual” úton (ez fut MPC-n) egyetlen egér-forrás van, minden ujj `index==0` → a második ujj felülírja az elsőt; `kick_cmd touch` csak slot 0-t injektál; nyomás/terület nincs beolvasva → velocity csak hely-alapú lehet. Hanghoz az `ae.addLiveMidiNote/Msg` út kész (ezt használja a fizikai billentyű is). Motoroldali korlát: a pitch bend csak keygroup/dob programra hat, fix ±2 félhang; a sustain a generált ONE_SHOT zongora-projektre nem hat.
**Terv (teljes értékű, sorrendben):**
1. **Per-ujj touch a hídban** (manual út): minden MT-slot külön touch-forrás; a `kick_cmd` kiegészítve többujjas injektálással (teszteléshez és hardveres méréshez).
2. **Zongora-képernyő** a `KeyboardComponent`-re építve (Pad Perform/Keyboard Control mellett új „Keyboard” nézet): 88/ görgethető billentyűzet, oktáv-váltó, a kijelölt track MIDI/program célja.
3. **Glissando** (`mouseDrag`), választható hangvezetés (csúszás-váltás vs. legato).
4. **Velocity-mód**: fix / hely-alapú (a billentyű alsó–felső része) / ujj-sebesség alapú; a `piano-ui/` mockup velocity-görbéjével összehangolva.
5. **Pitch bend és vibrato** érintéssel (vízszintes csúszás), a motor oldali tartomány konfigurálhatóvá tétele.
6. **Sustain-gomb** (CC64, folytonos helyes held-note logikával — a zongora-motor P0-jával együtt, 02).
7. **Skála-/akkord-jelölés** és a Pad Perform kiosztásával közös skála-forrás; külső (fizikai billentyű, szekvencia) hangok kirajzolása a billentyűzeten.
8. **Közös cél**: a képernyő-zongora és a Key 61 fizikai billentyűi ugyanarra a trackre/ transzponálásra mennek; note-off-ok konzisztensek.
9. **Késleltetés-mérés** hardveren (touch → hang): célérték és mérési módszer a híd 30 Hz-es üzenetszál-festéséhez képest (nincs adat; kód alapján nem becsülhető).
10. **Tesztek**: egységtesztek (többujjas állapotgép, glissando note-off), hardveres checklist (`HW_TEST_CHECKLIST`-be).

## 5. Új MIDI program/track hozzáadása (Program tab)
**Állapot:** működik. Hozzáadás gomb (`kick_AppMainMode.cpp:169`) → „CREATE PROGRAM” (Drum, Keygroup, Clip, Audio, Midi, CV, Plugin) → `ae.createProgram(type)`. A MIDI program port/csatorna/PC/bank be van kötve, mentődik (XML ValueTree) és betöltődik; CC0..127 szerkeszthető; pan→CC10 megy (csak property-változáskor).
**Hiányok/hibák:**
- a program hozzáadása **nem hoz létre trackot** (külön hozzárendelés a Track fülön, vagy „Duplicate Program To Track”, track-limit 128);
- alapnév „Program NN” (nincs típus-név, a számláló betöltés után ütközhet);
- Apply típus nélkül csendben bezár;
- program-szintű **note-tartomány, velocity-tartomány, transzpozíció nincs** (csak track-szintű); mod wheel beállítás nincs;
- **adatverseny**: a `midiOutputs/midiOutPorts` statikus konténereket az üzenetszál cseréli (`midi_device_manager.cpp:179,181`), a tick-szál iterálja (`:339`) → hot-plug alatt összeomlás;
- eltűnt port: a lista nem frissül, jelzés nincs; a timer 2,5 s-enként újraépíti a listát és szinkron hívja az `onSelectionChanged`-et;
- CC-csatorna a track `midiOutChannel`-jén, a note a program `midiCh`-ján megy → inkonzisztens; CC64 elnyelődhet, ha az az indexű pad némított (`isNoteEffectivelyMuted` minden üzenettípusra);
- CC95 elérhetetlen (hibás címkék: „CC21”-szomszéd, kétszeres „Send4”); `MTTrack::setMidiPort` önmagát írja felül; MidiControl „output” lista a bemenetekből épül; selector ikon-hiba; duplikált program neve nem egyedi;
- program/track létrehozás-törlés undo-ja nem igazítja a motor vektorait (nincs `valueTreeChildAdded/Removed`);
- tesztek: csak `MidiPanTests` (a `panToCC10` leképezés); port/csatorna/PC/bank/hot-plug/undo teszt nincs;
- hardver: a Key 61 DIN/USB kimenetét látja-e a JUCE, ha az adapter foglalja a rawmidi portokat — **nyitott** (`hw-behaviour.md:10`); hardveres bizonyíték MIDI programra nincs.
**Terv (sorrendben):**
1. **Biztonság/stabilitás**: zár vagy immutable-snapshot a MIDI-kimeneti listán; a port-lista frissítés ne hívjon visszahívást és ne építse újra feleslegesen; eltűnt portnál látható jelzés és újracsatlakozás.
2. **Hozzáadási folyamat**: az „Add MIDI program” opció opcionálisan **track-et is létrehoz** (és hozzárendeli); típus-alapú alapnév („MIDI 1”), egyedi név kikényszerítve; kötelező típusválasztás (Apply csak ha választott).
3. **Program-szintű beállítások**: note-tartomány/learn, velocity-tartomány, transzpozíció, mod wheel; a MIDI/CC-szűrők (Akai D-175, D-178, D-179).
4. **Konzisztencia**: egységes csatorna-forrás (program `midiCh`) a note és a CC-kre; CC elnyelés csak note-üzenetekre; CC-címkék javítása; a `setMidiPort` javítása.
5. **Undo/redo és perzisztencia**: `valueTreeChildAdded/Removed` a motorban; a pan→CC10 betöltéskor és portváltáskor is kimegy; ismeretlen típusról naplózás.
6. **Tesztek**: egységtesztek a fentiekre; a hardveres checklist bővítése (port-lista a Key 61-en, külső szinti, DIN OUT, PC/bank, hot-plug).
7. **Később (Akai-paritás)**: Monitor, track Output, MIDI-clock/MTC/MMC, program mentése, Track as MIDI export.

## 6. Egyéb nyitott tételek (az auditból)
- XY FX (Touch FX) képernyő: nem hat hangra/MIDI-re (`kick_XyFxView.cpp:191-197`); Note Repeat gomb: nincs fogyasztó; Song Export / Convert >SEQ, Sampler Export/Clear, Fájlböngésző Format és Eject (nem `umount`), System Resources Purge, Fejléc Automation gomb üres.
- ChannelMixer: a Q-Link `knobCommand` Output/Midi füle fordított `dynamic_cast` → nullptr (`kick_ui_channel_mixer.cpp:71-72,173-177`), Dátum/idő beállítás shell-parancs hibás (`kick_Hwpref.cpp:941,959`), WiFi gateway-mező a subnetet mutatja, Pad Mixer jobb csúcs/ minden pad ugyanazt a mérőt mutatja.
- MPC-n a Hardware-oldal pad-érzékenység beállítása hatástalan (a híd az UI-csomagoknak csak két típusát dolgozza fel), a Behringer-lemezkép parancsai (fényerő, factory reset, swupdate) az MPC fájlrendszerén valószínűleg nem léteznek.
- A hardveren nem igazolt: Aura-összeomlás hosszú futás után, pad „csendes állapot”, kerék/pedál → 1. track mérő, modális ablakok, Data Dial a List Edit-en (`known-issues-and-roadmap.md`).

## 7. Fájlböngésző-integrációk: Splice és Google Drive (owner ötlet, 2026-11-04; kutatás: `research/cloud-sources.md`)
**Rövid válasz:** a Spliceba nem lehet „bekötni” hivatalosan; a Drive csak korlátozottan, de járható utakkal.
- **Splice:** nincs nyilvános API/SDK/partneri program. Van egy béta MCP-szerver (`mcp.splice.com/mcp`, OAuth, AI-asszisztensekre; sampler általi használata nincs kimondva → NEM ELLENŐRIZVE), az MPC/Force beépített Splice-böngészője Splice-partnerség (zárt). A ToS tiltja a scrape-et, a belső API/adatbázis használatát, a minták izolált terjesztését és AI-tréninget. **Terv:** a Kick semmit nem „tud a Spliceról”; a felhasználó saját `~/Splice` mappáját helyi forrásként böngészi (desktop), az MPC-re a mintákat a felhasználó viszi át (pendrive/ Drive/ WebDAV). Valódi MPC-s Splice-böngészőhöz csak Splice-partnerség vezet.
- **Google Drive:** az eszköz-folyam (billentyűzet nélküli eszköz) csak `drive.file`/`drive.appdata` hatókört enged, a meglévő mappák böngészéséhez használhatatlan; a `drive.readonly` restricted (éves külső biztonsági felülvizsgálat); a Google API ToS 4(b)(1) tiltja a hitelesítési adatok beágyazását nyílt forrású projektbe → **nincs közös Kick-kliens**. Járható utak: (1) „bárki a linkkel” **nyilvános mappa** + felhasználói (BYO) API-kulcs, (2) **WebDAV-forrás** + `rclone serve webdav` a felhasználó gépén/NAS-án (teljes privát Drive), (3) BYO kliens ID haladóknak.
- **Architektúra:** `FileBrowserSource` interfész (state, beginAuth, list, stat, fetch `.part`+Range-folytatás, changes, signOut), közös gyorsítótár és letöltési sor; hálózati kód külön, alacsony prioritású szálakon; token: refresh token (desktopon kulcstár, MPC-n 0600 fájl); betöltés csak teljesen letöltött fájlból.
- **Sorrend:** (M0) interfész + helyi mappa forrás + cache (Splice-mappa desktopon), (M1) WebDAV (rclone-Drive, OneDrive, Dropbox), (M2) nyilvános Drive-mappa BYO kulccsal, (M3) általános eszköz-folyam + OneDrive, (M4) S3/Dropbox.
- **Kockázatok:** Google ToS ↔ AGPL (BYO-kulcs oldja), a Splice ToS tilalmai, libcurl/TLS licenc-értelmezés — **jogi átnézés kell (a ToS-mondatokat emberi szemmel újra kell olvasni)**.

## 8. Preferences (Menü → Preferences / Settings) — mélységi audit
Alap: `research/preferences-part1.md` (1–4. oldal), `research/preferences-part2.md` (5–7. oldal + rendszer-réteg) — vezérlőnkénti táblák fájl:sor bizonyítékkal; kódolvasás + az élő MPC **csak olvasásos** ellenőrzése (2026-11-04). Oldalak (`kick_ui_settings.cpp:88-96`): 1 Info, 2 Project Defaults, 3 Hardware, (Audio kikommentezve, üres stub), 4 MIDI, 5 CV, 6 Ethernet, 7 WiFi.

### 8.1 A legfontosabb: MPC-n a board-parancsok nem futnak
A `hardwareEnabled()` false, ha `KICK_NO_HARDWARE=1`, és a telepített MPC mindig így indul (`tkgl_bootstrap:286`). Ezért a **fényerő, dátum/idő, időzóna, Ethernet/DHCP/IP, WiFi, Factory Reset és Firmware Update némán nem csinál semmit**, és a UI ezt nem jelzi. Emellett a kód Behringer-lemezképre írt útvonalakat/parancsokat használ, amelyek az MPC-n **nincsenek meg** (élő ellenőrzés):
| Kód használja | MPC-n |
|---|---|
| fényerő: `/sys/devices/platform/backlight/backlight/backlight/brightness` | **nincs**; a valódi: `/sys/class/backlight/mipi-backlight` (`…/platform/mipi-backlight/backlight/mipi-backlight`) |
| Ethernet `eth0` | az interfész **`end0`** |
| WiFi: `wpa_supplicant`, `wpa_cli`, `iwlist`, `iw` | **mind hiányzik**; a rendszer **iwd**-t használ (`/data/system/etc/overlay/iwd`) |
| `dhclient` | hiányzik (van `udhcpc`) |
| `swupdate` (Firmware Update) | **hiányzik** |
| `/sys/devices/soc0/serial_number`, `/etc/version.txt` (Info) | **nincs** |
| `timedatectl`, `date`, `hwclock`, `ifconfig`, `ip`, `awk` | megvan |
| factory reset: `/home/Kick/{projects*,prefs}` | a Kick adatai `/data/prefs`, `/data/projects` → rossz útvonal; a WiFi-fájlok a resetet túlélik |

### 8.2 Oldalanként
**Info:** Control Version üres (az `EN_SYS_GET_SW_VER` küldőjét nem találtam); `refresh()` üres; Build Date csak `__DATE__/__TIME__` (git hash/ág nincs); Factory Reset útvonal-hiba, nincs újraindítás/visszajelzés; Restore Defaults csak `projects/hardware/eth/cv`-t állít vissza (midiControl, wifi, metronome, browser, cvclib kimarad), a futó állapotot nem alkalmazza újra, törli a `currentProject`-et és az `isAutoLoad`-ot.
**Project Defaults:** működik: RECENT PROJECTS, AUTO SAVE, SAVE AFTER, a névelőtag. **Fogyasztó nélküli** mezők (a mentett érték semmit sem állít): ON LAUNCH, BARS, ROOT NOTE, SCALE, SEQUENCES, TRACKS/SEQ, BIT Depth, DefaultProjectState; TEMPO csak a StepSeq tartalék-kijelzésben, NUMERATOR/DENOMINATOR csak a Looper-felvevőre hat. **BUG:** az AUTO SAVE konstruktora a mentett értékre állítja a statikus flaget (alapérték false), ami felülírja a `setAutoSave(true)`-t → indulás után a projekt-autosave alapból kikapcsolt; a TIME STAMP névstílus egyszer fagyasztja az időt; CUSTOM mindig „KICK_”-ra állítja az előtagot, újraindítás után a mező nem szerkeszthető.
**Hardware:** pad Threshold/Sensitivity/Curve a DCL-csomagban kimegy, de az MPC híd csak az aftertouch-enable-t és a színt dolgozza fel (`kick_hardware_bridge.h:137-160`) → **hatástalan**; aftertouch MPC-n mindig 0 (`kSendPadAftertouchPackets=false`); a VELOCITY/AFTERTOUCH csúszka csak mérő, minden ütésnél a perzisztens fába ír; Tap LED/NO OF TAPS fogyasztó nélküli (fix 3 koppintás); **dátum/idő:** a parancs csak a puszta `"2025-5-15"` string (`date -s` nincs), 1 Hz-es refresh visszaírja a rendszeridőt, az év-handler 31 napot ad, a hónap-mező címkéje „Minute”; időzóna alapérték `Asia/Kolkata`, a `getDateTime` folyamat-szinten `setenv("TZ")`-t hív, a `timedatectl set-timezone` parancs összefűzve fut (injekció-kockázat).
**MIDI:** mind a hat beállítás fogyasztó nélküli (két portnév, négy TRACK/CONTROL kapcsoló); az 1 Hz-es refresh szinkron hívja az `onSelectionChanged`-et → a mentett portnév felülíródik a kijelölttel (üres listánál `""`-val); a kimeneti port nem áll vissza; `KickMidiControl::isValid()` rossz ID-t néz (mindig false); Sync/Clock/MMC/MTC vezérlő nincs.
**CV (hardveren nem mért):** a kalibráció csak az APPLY gombra alkalmazódik → bootkor a jegyzetek 0 DAC-értéket kapnak, a mentett offsetek nem érvényesülnek; csatorna-leképezés hibás irány (`kick_I2c.cpp:136,163,298`), a 8. CV port nem címezhető, dupla offset (program `CVStartNote`), `calibrateCV` csak 1..4, a UI sorai 0..3; lapos jegyzetnév-módban −1; `std::map` védelem nélkül két szál között.
**Ethernet:** DNS és gateway nem érvényesül (`setIP` eldobja a gateway-t); a MODE kapcsoló megfordított („MANUAL” DHCP-t indít); a `getIP` újsorral végződő kimenete bekerül a prefsbe és a bootkori `ifconfig`-ba → netmask nem érvényesül, parancs-injekcióra alkalmas; az ENABLED kapcsolót az 1 s-os refresh visszaírja.
**WiFi:** gateway-mező a subnetet mutatja; az ENABLED kapcsoló csak a prefsbe ír, bootkor nincs újracsatlakozás; **a jelszó szövegesen** kerül a prefsbe, a credentials-fájlba és a wpa conf-ba (a reset nem törli); ismert SSID régi jelszava felülírja az újat; a wpa conf idézőjel nélkül épül (`"`/újsor → config-injekció); a connect a UI szálon fagyaszt (≥3 s). **MPC-n az egész eleve nem működik** (wpa_supplicant nincs, iwd fut).
**Perzisztencia:** 1,5 s-os debounce-autosave, atomi írás; nincs kilépéskori mentés (az utolsó <1,5 s elveszhet); sérült XML esetén a settings használhatatlan, nincs javítás; a `version="1.0"` csak íródik, nincs migráció; a `makeDefaults` nem érinti a wifi/cvclib/midiControl/metronome/browser fákat.
**Tesztek:** Preferences/HwPref/KickSettings/CV-kalibráció/WiFi teszt **nincs** (csak `CvScaleTests` és egy képernyőkép-smoke).
**Doksi-ellentmondás:** a `WP-G` több „OK” ítélete (G-053, G-062, G-071/072, G-105, G-107/108, G-115/116, G-122..124, G-137, G-141, G-143/144) a kód alapján cáfolható.

### 8.3 Hiányok az Akai Preferences-hez képest
Sequencer csoport teljesen (Rec Arm Behavior, Solo Behavior — G-126..136); MIDI/Sync: Global, Sync, MMC, MTC, Program Change, port-átnevezés/reset (G-090..104); Project Defaults/Load-Save legtöbbje (G-138..158); Hardware: Dimming, 4 fokozatú fényerő, Curve A–D, Bank Button Press, Shift+Q-Link, Global Pitch Bend, alapértelmezett Touch Strip mód; Info: Legal Info, Version Info.

### 8.4 Terv (sorrendben)
1. **Platform-réteg (alap):** a board-parancsokat egy `PlatformServices` interfész mögé (desktop / Behringer / **Akai MPC** implementáció); az MPC implementáció a valós útvonalakkal/parancsokkal (backlight sysfs, `end0`, `timedatectl set-time`, **iwd/`iwctl` vagy a D-Bus API** a WiFi-hez, `udhcpc`, a Kick saját frissítője a `swupdate` helyett — lásd 01). A „hardver kikapcsolva” állapotot a UI **jelezze** (letiltott/szürke vezérlők + ok), ne némán nem csináljon semmit.
2. **Gyors javítások (biztos hibák):** Ethernet MODE megfordítás, DNS/gateway alkalmazása, dátum/idő parancs (`date -s`/`timedatectl set-time`, az év-handler és a „Minute” címke), AUTO SAVE alapérték-felülírás, MIDI-port visszaállítás (ne hívjon visszahívást a refresh), WiFi gateway-mező, `KickMidiControl::isValid()`, factory reset útvonalak (`/data/prefs`, `/data/projects`, WiFi-adatok is), shell-injekció megszüntetése (paraméterezett végrehajtás `execve`-szerűen, bemenet-validáció, WiFi-jelszó idézése), WiFi-jelszó titkosított/0600 tárolása.
3. **Fogyasztó nélküli beállítások bekötése vagy eltávolítása:** Project Defaults (ON LAUNCH, BARS, ROOT NOTE, SCALE, SEQUENCES, TRACKS/SEQ, BIT Depth, TEMPO, időjel az új projektnél), MIDI-oldal (sync/clock/MMC/MTC valódi implementációval), pad-érzékenység MPC-n (a híd feldolgozza a küszöb/érzékenység/görbe csomagot a Key 61 pad-kezeléséhez). Félrevezető, működő-látszó kezelőszerv ne maradjon.
4. **CV oldal:** kalibráció alkalmazása bootkor, a csatorna-leképezés és az offset javítása, 8. port; hardveres mérés (a CV hardveren nem tesztelt).
5. **Perzisztencia:** kilépéskori mentés, sérült fájl esetén biztonsági másolat + visszaállítás, verzió-migráció, teljes `makeDefaults`.
6. **UX:** WiFi/Ethernet állapotsor és hibaüzenet, WiFi-connect háttérszálon, megerősítő párbeszéd + újraindítás a Factory Reset-hez, Info oldal: valódi verzió/ git hash/ build-azonosító (a pendrive-os `VERSION` fájllal egyezően), Legal/Version info.
7. **Akai-paritás (később, külön réteg):** Sequencer csoport, MIDI/Sync teljes, Load/Save defaults, Hardware Dimming/Curve A–D/Bank Button, alapértelmezett Touch Strip mód.
8. **Tesztek:** egységtesztek a perzisztenciára, a validációra és a parancs-összeállításra (injekció-teszt), HwPref-mockkal; hardveres checklist az MPC-s Preferences-hez.
