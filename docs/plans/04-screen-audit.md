# Terv 04 — Képernyő-audit és fejlesztési tervek (indítóképernyő, főképernyő tabjai, Grid, érintő-zongora, MIDI program hozzáadása)

Állapot: terv. Alap: **statikus kódolvasás** (build/futtatás/hardver nélkül) — a „KÉSZ” azt jelenti: van bekötött handler az engine-ig, nem azt, hogy hardveren igazolt. A hardveres igazolás a `research/known-issues-and-roadmap.md`-ben. Részletes bizonyítékok (fájl:sor): `research/screen-inventory.md`, `research/midi-program-add.md`, `research/grid-and-piano-view.md`. Ahol a régi `docs/AKAI_MPC_KEY61_*.md` és a kód ellentmond, a **kód az igazság**.

## 0. Prioritások (összefoglaló)
1. Indítás: nincs indítóképernyő; két gyanús versenyhelyzet (dupla betöltés, üres képernyő); a hardver-/hang-init hibák láthatatlanok.
2. Grid: dupla bank-eltolás (csak az A bank jó), COPY/ERASE kétszer fut, a Magnify eszköz az MPC-n halott, Q-Link zoom gyakorlatilag nem működik.
3. Főképernyő: Track „Monitor” gomb, Sequence „Time Signature” stub; a program-/track-létrehozás undo-ja nem követi a motor állapotát.
4. MIDI program: működik, de nincs program-szintű note/velocity-tartomány/transzpozíció, adatverseny a MIDI-kimenet listán, eltűnő port jelzés nélkül.
5. Érintő-zongora: nincs használatban lévő zongora-nézet (a `KeyboardComponent` nincs bekötve); a touch az MPC-n egyujjas.

---
## 1. Indítóképernyő
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
- **Hiányzik / terv (Akai-eltérés, `KEY61_DOC_VS_KICK.md`, WP-D/G):** sequence-szintű beállítások, count-in, SEQ REC ARM, Next Sequence mód finomságai; Song konvertálás (`Convert >SEQ` stub).
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
- hardver: a Key 61 DIN/USB kimenetét látja-e a JUCE, ha az adapter foglalja a rawmidi portokat — **nyitott** (`KEY61_HW_BEHAVIOUR.md:10`); hardveres bizonyíték MIDI programra nincs.
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

## 7. Fájlböngésző-integrációk: Splice és Google Drive (ötlet, owner 2026-11-04)
Cél: a Kick fájlböngészőjében (`modes/filebrowser/`, jelenleg helyi tár/USB/SD) felhőforrások is böngészhetők és tölthetők legyenek.
- **Google Drive (kivitelezhető, kutatás kell a részletekre):** Drive API v3; az MPC-n nincs billentyűzet → OAuth **eszköz-folyam** („limited-input device”: a Kick kódot mutat, a user a telefonján hagyja jóvá). A letöltés `/data` alatti gyorsítótárba megy (offline is használható), a böngésző virtuális mappaként mutatja. Nyitott: OAuth-kliens és Google-ellenőrzés (a teljes Drive-olvasás „restricted” hatókör → ellenőrzési/ biztonsági felülvizsgálati teher; szűkebb hatókör, pl. egy dedikált mappa, egyszerűbb), token-tárolás az MPC-n biztonságosan, hálózat/WiFi-állapot kezelése, hibaüzenetek (lásd 1. pont), nagy fájlok (szakaszos letöltés, megszakítható).
- **Splice (NEM ELLENŐRIZVE, kutatás kell):** nem tudom, hogy van-e **nyilvános API** harmadik fél alkalmazásoknak; a licenc (royalty-free, de a minták önmagukban nem terjeszthetők) miatt a Kick nem tárolhat/ osztogathat Splice-mintát. A kivitelezhetőség kérdése: (a) hivatalos API/ partnerség létezik-e, (b) ha nincs, a Splice asztali app által letöltött **helyi mappa** böngészése (a felhasználó saját letöltéseit a Kick csak olvassa/ importálja, pl. Drive/ pendrive vagy hálózati megosztás útján), (c) a Splice szolgáltatási feltételei tiltják-e az automatizált hozzáférést. Addig nincs ígéret az egyetlen kattintásos Splice-integrációra.
- Közös: `FileBrowserSource` absztrakció (helyi, USB, SD, Drive, később mások) — a böngésző forrás-független legyen, hogy a harmadik fél (03 terv) is adhasson forrást; letöltési sor, megszakítás, gyorsítótár-kezelés, offline jelzés.
- Teendő: kutató-kör a Drive OAuth-eszközfolyamra és hatókörökre, valamint a Splice hivatalos lehetőségeire (feltételek, API) — **forrás-URL-lel, ne emlékezetből**.
