# Next Sequence / Song / Color Edit — review és TODO

Állapot: **statikus kódolvasás** (build/futtatás/hardver nélkül; az MPC-hez nem csatlakoztam), 2026-11-04, ág `claude/kick-plans`. A „KÉSZ” itt azt jelenti: van bekötött handler az engine-ig, nem azt, hogy hardveren igazolt. Három képernyő egy fájlban; a hibák és a TODO kódjai képernyőnként külön prefixet kapnak: **NS** = Next Sequence (`M/kick_ui/modes/kick_nextSequence.{h,cpp}`), **SG** = Song (`kick_song.{h,cpp}`), **CE** = Color Edit (`kick_PadColor.{h,cpp}`). Rövidítések: `M/` = `Kick/modules/`, `A/` = `Kick/apps/kick/`, `MODES/` = `M/kick_ui/modes/`, `bridge` = `A/kick_hardware_bridge.h`. Prioritás: **P0** összeomlás / adatvesztés / biztonság, **P1** hibás vagy hiányzó alapfunkció, **P2** javítás / kényelem. Méret: S ≤ fél nap, M ≤ 2 nap, L > 2 nap.

## Állapot (R-8, 2026-11-04 — a S-C és az R-8 kör után; ez a szakasz a mérvadó, az alábbi táblák a review idejét mutatják)

S-C: NS-01..03, 06..09 (`dec5d617`, `d2d51ce2`), SG-01..08 (`10217009`), CE-01, 03..07 (`3f265a07`). R-8:

| Tétel | Állapot |
|---|---|
| **NS-05** Hold | KÉSZ a kézikönyv szerint: a Hold bekapcsolva a jelenlegi sorozat ismétlődik (a motor sora törlődik), a pad-nyomás csak **kijelöl** (zöld szegély), nem állít sorba; Next Bar / Sudden a kijelölésre vált (Hold alatt a kijelölés, egyébként a sorban álló); Hold kikapcsolása: normál működés, a kijelölés nem kerül sorba (a régi viselkedés a kijelölést kikapcsoláskor sorba tette: a kézikönyv szerint a pad-nyomást „ideiglenesen figyelmen kívül hagyja”). Next Bar / Sudden nem indítja újra a játszó sorozatot, ha nincs kijelölés. Host-teszt: `SequenceLogicTests` (setHold, pressPad, switchTarget, afterSudden) |
| NS Clear | A kézikönyv szerint a Clear a kijelölt sorozatot **törli a lejátszási listából**, csak ha az nem játszik (a régi Kick-viselkedés: a sorban álló eldobása, ez továbbra is elérhető: a játszó pad újra megnyomása). KÉSZ: kérdés (Igen / Nem), UNDO a státuszsávban, a játszó és az utolsó sorozat védett; az indexek igazítása host-tesztelt (`afterDelete`) |
| NS „Song” gomb | KÉSZ (kézikönyv: Copy to Song, leállítva): a lista minden sorozata lépésként (ismétlés 1) a kiválasztott vagy egy új dalhoz **hozzáfűződik** (a kézikönyv nem mondja, hogy felülír-e: nem romboló), egy undo-lépés (`MTSongManager::appendSongSteps`) |
| **NS-04** pad-LED irány | JAVASLAT (owner-döntés): **a híd tartson egy `PadLedState`-et** (16 pad: szín, állapot, villogás), amelyet a meglévő `SET_SINGLE_PAD` / `ALL_SINGLE_COLOUR` / `BLINKING` / `COLOR_MODE` parancsok írnak, a villogást a híd animálja; a `SET_COLOUR_STATE` marad az alapréteg. Ez egyszerre oldja NS-04, SG-10, CE-02, SS-07 és a többi `setPadDataMessage`-t használó képernyőt (AssignSample, Mute, Sampler); a képernyők nem változnak. Az alternatíva (a képernyők átállítása a 16 szín + állapot csomagra) villogás nélküli, és minden képernyőt érint. A híd az R-3 (agy) dolga, itt nem nyúltunk hozzá |
| NSC-01 Akai-összevetés | alább |
| **SG-09** Convert > Seq | NINCS motor-függvény (`songToSequence`): a dal lépések listája, a sorozat-lánc eseményekkel való egyesítése (sávonként a jegyzetek másolása eltolással, ütemmutató / tempó kezelés, „Track Status: Exclude / Include Muted Tracks”) a motor önálló munkája. A gomb nincs megjelenítve. Jelentve |
| SG Export | owner-döntés (a kézikönyv: Audio Mixdown ablak — az audió-exportot az R-2 / a mixdown-tervek kezelik) |
| CE (Color Edit) saját logika | KÉSZ: Shift + színgomb = minden pad ezt a színt kapja, Shift + pad (érintés vagy hardver) = a pad színét választja ki (kézikönyv), Make Default kérdez (kézikönyv: Do It / Cancel) + UNDO a státuszsávban, All Pads UNDO-val. CE-02 / CE-08: a híd (R-3) |

### Akai-összevetés, WP-B / WP-C (kézikönyv 316–317., 326–328., 336–337. o.) — NSC-01
**Next Sequence**
| Kézikönyv | Kick |
|---|---|
| Sorozat-lista (Sequence, Length, BPM), kijelölés érintéssel | KÉSZ |
| Pad A01 = 1. sorozat; sárga = van sorozat, halvány sárga = üres; a kijelölt pad villogó zöld majd zöld | a képernyőn KÉSZ (szín-kiosztás eltér: narancs / zöld); a hardver pad-LED nem (NS-04) |
| Lejátszás közben pad = a következő sorozat; ha nincs, a jelenlegi ismétlődik | KÉSZ |
| Next Bar / Sudden (a kijelölt sorozatra) | KÉSZ (R-8: a kijelöltre, Hold alatt is) |
| Clear (a kijelölt sorozat törlése a listából, ha nem játszik) | KÉSZ (R-8) |
| Hold (a jelenlegi ismétlődik, a pad-nyomás kijelöl, nem sorba állít) | KÉSZ (R-8) |
| Song (Copy to Song: leállítva, dal kiválasztása, Do It) | KÉSZ (R-8) |
| Time Counter | a státuszsáv BBT-je |
| Hardver gomb: Next Seq (Key 61: Shift + Next Seq = Song) | NINCS Key 61 gomb-hozzárendelés a hídban (R-3) |
**Song**
| Kézikönyv | Kick |
|---|---|
| 32 dal, 999 lépés, lépésenként 999 ismétlés | KÉSZ (R-8: korlátok és az ismétlés 999-ig) |
| Song mező, billentyűzet-ikon = átnevezés | KÉSZ (Rename gomb) |
| BPM mező + Seq/Gbl | NINCS (a dalnak nincs saját tempója a motorban); rejtett |
| Lépés-lista: sorszám, sorozat, ismétlés, tempó, ütemek; **Hold (legkisebb érték) = végtelen ismétlés** | sorszám, sorozat, ismétlés, tempó, ütemek KÉSZ; a végtelen ismétlés **nincs** (a motor lejátszási sora véges lépésekre számol; motor-munka) |
| Pad-bank gombok a sorozatokhoz | KÉSZ |
| Lépés beszúrása: lépés kijelölése + Insert | KÉSZ |
| Dal „felvétele” padokkal (Rec / Overdub élesít, pad = lépés, többszöri nyomás = több ismétlés, Stop vége) | KÉSZ (R-8: Rec kapcsoló, vagy a hardveres REC élesítve; a pad az utolsó lépésként illeszti be, ugyanaz a pad ismétlést növel; a lejátszás nem szól) |
| A lépés sorozatának cseréje: sorozat-név érintés + Data Dial / +/- | KÉSZ (R-8: a kijelölt lépés sorozatát a Data Dial / +/- állítja; Shift vagy nincs kijelölés: a lépéseken lépked) |
| Clear (minden lépés), Delete (lépés) | KÉSZ (S-C: UNDO) |
| Next / Sudden (játszás közben a következő lépésre) | KÉSZ (R-8: `skipSongStep`, a tick / ütem szálon fogyasztva) |
| Convert > Seq (To Sequence, Track Status) | NINCS motor-függvény (SG-09) |
| Export = Audio Mixdown ablak | owner-döntés / mixdown |
| Dalba lépni lejátszás közben nem lehet | a Song képernyő a lejátszás alatt zárolja a táblát (S-C); a belépés lejátszás alatt nincs tiltva (nem vizsgált) |
**Pad Color**
| Kézikönyv | Kick |
|---|---|
| Track mező, Bank mező | program-választó, bank (◀ ▶, A-D / Shift E-H) KÉSZ |
| Alsó mező: pad fénymód: Off / Classic Velocity (pad-nyomásra velocity szín piros-sárga) / Fixed / Off->Velocity / Dim->Velocity / Bright->Velocity | A Kick: Pad Off (Dim / Off) x Pad On (Velocity / Full / Off) + Empty = 9 mód. Megfelelés: Off = Off/Off; Off->Velocity = Off/Velocity; Dim->Velocity = Dim/Velocity; Fixed ≈ Dim|Off / Full. **Nincs**: Classic Velocity (piros-sárga velocity skála) és Bright->Velocity (világos alapfény) — az MPC hardver pad-rétege, **NEM-KICK** (owner: később; a híd kezeli) |
| Empty Pads: off / dim / normal | Empty: Dim / Off KÉSZ; a „normal” (üres pad ugyanúgy világít) NINCS (MPC-specifikus pad-réteg) |
| Single Pad / All Pads; Shift + színgomb = minden pad | KÉSZ (R-8) |
| 18 színgomb; Shift + pad = a pad színét választja | KÉSZ (R-8) |
| Pad-nyomás (hardver) / érintés = a szín ráállítása | KÉSZ (S-C) |
| Make Default = az aktuális színek lesznek a program-típus alapértékei (Do It / Cancel) | **Eltérés:** a Kick „Make Default” gombja a program típusának alapszíneire **állít vissza** (S-C), a kézikönyv szerint viszont az aktuális elrendezést **menti** alapértéknek. Megvalósítva: kérdés Do It / Cancel szerint. Nincs: a mentett per-típus alapérték (tartós tároló a KickSettings-ben + migráció, a program-létrehozás átvétele): owner-döntés (melyik jelentés legyen a gomb), javaslat: két gomb, „Make Default” (ment) és „Reset” (gyári) |
| Keygroup / plugin / MIDI / CV sávok **alapból a Pad Perform pad-szín kiosztását** használják; egyedi kiosztás a pad-terület megérintésével | JAVASLAT (owner-döntés): a Keygroup / MIDI programok alapszíne a Pad Perform színei: gyökér = piros, skála-hang = sötétsárga, a többi halvány (S-F: ugyanez a pad-LED logika); ma `defaultPadColours` = { Red } (csak az első pad piros, a többi 0 = sötétpiros). Ezt a `mt_audio_iprogram.h` tömbje cseréli, a program-típusonként |


**Az Akai-összevetésről.** Az Akai útmutató erre a három módra vonatkozó fejezeteit (WP-B: Next Sequence, Song, Looper; WP-C: Pad Color) a `WALKTHROUGH_PLAN.md` szerint még **nem** dolgozta fel senki (a `walkthrough/` mappában csak WP-B.raw.json, WP-D, WP-G van, és a nyers JSON-ban nincs „Next Sequence” / „Song” / „Color” találat). Ezért ahol az Akai-viselkedésről szólok, az **NEM ELLENŐRIZHETŐ KÓDBÓL / DOKUMENTUMBÓL**; az első TODO ennek pótlása (NSC-01).

## 1. Áttekintés

### Next Sequence (`MODES/kick_nextSequence.cpp`, 680 + 158 sor)
**Mit csinál.** Sorozat-elindító nézet: balra a sorozatok táblázata (sorszám, név, hossz, BPM), jobbra 16 pad (egy bank = 16 sorozat, A–H bank = 128 sorozat) az aktuális bank sorozataival, alul kapcsolók: **Next Bar**, **Sudden**, **Hold**, **Clear**. Lejátszás közben egy pad = a következőnek sorba állított sorozat (zöld/narancs „lélegző” pad), Next Bar/Sudden mód szerint vált. Nem lejátszva a pad/sor választás a „kiválasztott” sorozatot állítja (`ae.setSelectedSequence`).
**Elérés.** Menü „Sequences” csempe (`M/kick_ui/dialogs/kick_AppMenuOptions.cpp:88`). Hardveres gomb a Key 61-en **nincs**: a `HW_BTN_NEXT_SEQ` (`bridge:1366`) az `akai_map.h`-ban nem szerepel, csak `kick_cmd` `btn NEXT_SEQ`-kel érhető (`bridge:1987`); a DCL `EN_NEXT_SEQ` → `kick_AppHome.cpp:507`. Nincs mód-LED (`bridge:270-290`).
**Hardver-kötések.**
| Vezérlő | Mit tesz | Kód | Hardveres állapot |
|---|---|---|---|
| PLAY / PLAY START | `onPlay()` — mindkettő ugyanaz: `ae.playSequence( playingSequence )` (ha nem játszik) | `MODES/kick_nextSequence.cpp:619-627, 340-353` | nem mért |
| STOP | `onStop()` → `ae.stopSequence()` | `:628-630, 355-369` | nem mért |
| Pad (16) | `pads->OnButtonClicked( id )`: játszva sorba állít, nem játszva a táblát választja | `:128-144` | pad-ütés általában igazolt; a LED-visszajelzés MPC-n **nem látszik** (B3) |
| Bank A–D / Shift E–H | `bankChanged` → pad-bank csere | `:222-234` | A–D LED igazolt |
| Q-Link 1–4 | BPM / Length / Loop Start / Loop End (a kiválasztott sorozaton); 5–13 nincs a Key 61-en | `:634-679` | csak ha `knobControl.getMode() != 0` (különben a Knob Edit viszi: `kick_AppHome.cpp:561-570`) |
| Data Dial | nincs kötés (a `handleCommand` a központi gombot kihagyja) | `:599-617` | — |
Érintés: a táblázat sorai választanak (lejátszás közben a tábla nem kattintható: `:411`), a pad-ok a képernyőn is kattinthatók; **nincs érintéses Play/Stop** (a fejlécben `showPlayButton` nincs hívva: `KickModeHeader::bShowPlay{ false }`, `M/kick_ui_header/header/kick_modeHeader.h:66`).

### Song (`MODES/kick_song.cpp`, 612 + 169 sor)
**Mit csinál.** Dal-szerkesztő: fent a dallista (`CREATE NEW SONG` bejegyzéssel), BPM mező, Seq/Song BPM kapcsoló, Export ikon; balra a dal lépéseinek táblázata (SEQ, REPEAT, BPM, BARS), jobbra a 16 pados sorozat-kiválasztó 3 soros felirattal (név, hossz, BPM); alul Clear, Insert, Delete (a Convert >SEQ gomb rejtett). Insert: a kiválasztott pad sorozatát illeszti a lépésekbe (azonos sorozat újbóli Insert-je +1 ismétlés); PLAY a kijelölt lépéstől játssza a dalt.
**Elérés.** Menü „songs” csempe (`kick_AppMenuOptions.cpp:93`). Hardveres gomb **nincs**. Nincs mód-LED.
**Hardver-kötések.**
| Vezérlő | Mit tesz | Kód | Hardveres állapot |
|---|---|---|---|
| PLAY / PLAY START | `playOrResume()` — mindkettő ugyanaz; játszva újra PLAY a kijelölt lépéstől újraindít | `MODES/kick_song.cpp:528-542, 216-228` | nem mért |
| STOP | `stopOrPause()` — valójában Stop: a kijelölést a 0. sorra állítja | `:230-243` | nem mért |
| Pad (16) | a Insert célsorozatát választja | `:116-131` | LED nem látszik MPC-n (SG-B10) |
| Q-Link | ugyanaz a (másolt) kiosztás, mint a Next Sequence-ben | `:466-511` | lásd NS |
| Data Dial | a tábla / lista fókuszától függ | `KickControlBase` | nem mért |
Érintés: a dallista (`KickListBox`), a táblázat és a gombok érintésre működnek; nincs érintéses Play/Stop (`header.setPlayState` hívódik, de a Play-gomb nincs megjelenítve: `bShowPlay == false`).

### Color Edit (`MODES/kick_PadColor.cpp`, 554 + 147 sor)
**Mit csinál.** Egy program pad-színeinek szerkesztése: bal oldalon *Pad Off* (Dim / Off), *Pad On* (Velocity / Full / Off), *Empty Pad Color Mode* (Dim / Off), *Pad Type* (Single / All Pads), 18 színes gomb; jobb oldalon a 16 pados rács az aktuális bankkal (bank-léptető ◀ ▶), „Make Default” gomb; a fejlécben program-választó. Egy pad megérintése az aktuális színt adja a padnak (programonként 128 szín: `padColour[128]`), Pad Type = All Pads az egész programot átszínezi.
**Elérés.** Menü „Coloreditor” csempe (`kick_AppMenuOptions.cpp:95`); Program opciók → „Pad colour” (`M/kick_ui/program_view/source/kick_AppProgramOptions.cpp:67`). Hardveres gomb **nincs**; nincs mód-LED.
**Hardver-kötések.**
| Vezérlő | Mit tesz | Kód | Hardveres állapot |
|---|---|---|---|
| Pad (16) | **csak élő MIDI** a kiválasztott trackre (`noteOn`/`noteOff`, `bankOffset + pad`); a hardveres pad **nem** színez | `MODES/kick_PadColor.cpp:364-387` | pad-ütés általában igazolt |
| Bank A–D / E–H | `bankChanged` → rács + bank-címke | `:89-91, 323-334` | A–D LED igazolt |
| PLAY / PLAY START / STOP | `setSequencePlay` / `setSequencePause` | `:352-363` | nem mért |
| Data Dial, Q-Link | nincs kötés (a szín- és mód-választás csak érintéssel) | — | — |
Színezés után a hardver pad-LED frissítése: az UI `setPadDataMessage` / `setAllPadDataColourInfoMessage` / `setPadColorModeMessage` parancsot küld — **az MPC híd ezeket eldobja** (CE-B2).

### Közös: pad-LED az MPC Key 61-en
A híd az UI→hardver (DCL) parancsok közül **csak** az `EN_PAD_SET_COLOUR_STATE`-et (16 szín + 16 állapot) és az `EN_PAD_SET_AFTER_TOUCH`-ot dolgozza fel (`bridge:138-160`; a többit `return`-nel elnyeli, `bridge` megjegyzése + `screen-inventory.md` §5). A Next Sequence, Song és Color Edit a `EN_PAD_SET_SINGLE_PAD` (`setPadDataMessage`), `EN_PAD_SET_ALL_SINGLE_COLOUR` (`clearAllPadsMessage`, `setAllPadDataColourInfoMessage`), `EN_PAD_SET_BLINKING` (`setPadBlinkingMessage`) és `EN_PAD_COLOR_MODE` (`setPadColorModeMessage`) parancsokat használja (`M/contracts/dcl/dcl_contracts_helper.cpp:181-185, 205-219, 246-250, 284-288`). Az egyetlen támogatott út a `refreshPadLeds` → `KickLEDs::changePadColours` → `setPadColourAndStateInfoMessage` (`M/kick_leds/kick_leds.cpp:21-26, 366-418`), ami a *kiválasztott program* színeit adja. A híd a villogó állapotokat sem animálja (`A/hardware/pad_colour_map.h:41`: „blink states are not animated”).

## 2. Funkciók és állapotuk

### Next Sequence
| Funkció | Állapot | Bizonyíték |
|---|---|---|
| Sorozat-táblázat (sorszám, név, hossz, BPM) | KÉSZ | `MODES/kick_nextSequence.cpp:88-110` (a `getSequenceData` null-védelme hibás: B4) |
| 16 pados bank, A–H, pad-feliratok | KÉSZ | `:433-461` |
| Sorba állítás játszás közben (pad) | KÉSZ | `:128-139, 244-263` |
| Next Bar | KÉSZ | `:176-192` |
| Sudden | KÉSZ | `:164-174` |
| Hold | RÉSZLEGES | `:146-162` — bekapcsolása törli a sort (viselkedés N/E) |
| Clear | **BUGOS GYANÚ** | `:194-201, 315-338` — `std::abort()` ág (B1) |
| Állapot-visszaállítás belépéskor / bankváltáskor | **BUGOS** | `:371-404` — `std::abort()` (B1) |
| Play / Stop hardverről | RÉSZLEGES | `:619-632` — PLAY START = PLAY, más motorhívás (B2) |
| Play / Stop érintéssel | **HIÁNYZIK** | nincs `showPlayButton` / `onPlayStopClick` |
| Pad-LED visszajelzés MPC-n | **NEM MŰKÖDIK** | híd (B3) |
| Q-Link 1–4 (BPM, Length, Loop Start/End) | RÉSZLEGES | `:634-679` (5–13 nincs a Key 61-en; null-védelem hiányzik) |

### Song
| Funkció | Állapot | Bizonyíték |
|---|---|---|
| Dal létrehozása, kiválasztása | RÉSZLEGES | `MODES/kick_song.cpp:380-409` (validáció nélkül, SG-B7) |
| Dal törlése / átnevezése | **HIÁNYZIK** | az engine `deleteSong` megvan (`M/engine/audio_engine/song/mt_song.cpp:180-181`), UI nincs |
| Lépés-táblázat (SEQ, REPEAT, BPM, BARS) | KÉSZ | `:95-114, 580-611`; Repeat szerkeszthető (`:108-111`) |
| Insert / Delete / Clear | RÉSZLEGES | `:133-172` — nincs undo (SG-B4), nincs megerősítés |
| Dal lejátszása / leállítása | RÉSZLEGES | `:216-243, 528-542` (UI-zár a dal végén: SG-B3) |
| Seq/Song BPM kapcsoló + BPM mező | **BUGOS GYANÚ** | csak megjelenítés (SG-B6) |
| **Export** | **STUB** | a dialógus megnyílik, `if( R == UI::APPLY ) {}` üres — `:411-423` |
| **Convert >SEQ** | **STUB** | tag létrejön és hozzáadódik (`:83-85`; `.h:96`), nincs `onClick`, a `setBoundsRelative` kikommentezve (`:283`) → láthatatlan |
| Pad-LED visszajelzés MPC-n | **NEM MŰKÖDIK** | híd (SG-B10) |
| Q-Link 1–4 | RÉSZLEGES | `:466-511` — a Next Sequence kódjának másolata (null-védelem nélkül) |

### Color Edit
| Funkció | Állapot | Bizonyíték |
|---|---|---|
| Program-választó (fejléc) | KÉSZ | `MODES/kick_PadColor.cpp:60-64, 399-400` |
| Szín kiválasztása (18) és pad-színezés (Single) | KÉSZ (UI) | `:290-321`; hardver-LED: **nem frissül MPC-n azonnal** (CE-B2) |
| Pad Type = All Pads | RÉSZLEGES | `:309-317` — megerősítés / undo nélkül |
| Pad Off (Dim/Off), Pad On (Velocity/Full/Off), Empty Pad mód | RÉSZLEGES | `:43-58, 111-167, 169-229`; a motor-oldali érték mentődik, a hardver-hatás **MPC-n nincs** (`EN_PAD_COLOR_MODE` eldobva) |
| Make Default | **BUGOS** | `:104-108` — minden pad a 3-as (zöld) színre, nem az alapértékre (CE-B1) |
| Bank léptetés (◀ ▶, HW bank gombok) | KÉSZ | `:71-91, 323-334` |
| Pad-feliratok (mintanév) | KÉSZ | `:231-283` |
| Hardveres pad = élő hang | KÉSZ | `:364-387` |
| Hardveres pad = színezés / Data Dial = szín | **HIÁNYZIK** | — |
| „Please Select Program” üzenet program nélkül | KÉSZ | `:413-421` |

## 3. Mi nem működik / hibák

### Next Sequence
**NS-B1 — `std::abort()` az alkalmazásban, P0.** A kód két helyen szándékosan összeomlasztja a programot („Forces a crash here”): `clearNextSequence()` (`MODES/kick_nextSequence.cpp:336`, ha nem játszik és `playingSequence < 0`) és `restoreData()` (`:397-400`, ha `!isPlaying && nextSequence >= 0`). A második **elérhető**: a `nextSequence` UI-állapot a STOP-tól és a képernyő elhagyásától függetlenül megmarad (`stopPlaying`: `:355-369`, `visibilityChanged(false)` játszás közben: `:528-532` nem nullázza), míg a `restoreData` a motor *aktuális* állapotát olvassa (`isPlaying = ae.isSequencePlaying()`, `:376`). Reprodukció (kódból, nem futtattam): (a) Next Sequence képernyőn PLAY, egy másik pad = sorba állított következő; STOP (HW) → **bankváltás** (HW A–D) → `bankChanged` kezelő → `updateData(); restoreData();` (`:222-234`) → `abort()`. (b) Játszás közben sorba állítás, kilépés a képernyőről (Menü), STOP egy másik képernyőn (a Menü/Main PLAY/STOP gombja közvetlenül a motort hívja: `kick_AppMenuOptions.cpp:60-75`), visszalépés a Next Sequence-be → `visibilityChanged(true)` → `restoreData()` → `abort()`. Hatás: az MPC alkalmazás meghal (a crash-handler `/data/kick.crash`-t ír), a nem mentett munka elvész (az autosave beállításától függően).

**NS-B2 — Transport inkonzisztens, érintéssel nem indítható, P1.** (a) `EN_PLAY` és `EN_PLAYSTART` ugyanazt az `onPlay()`-t hívja (`:623-627`), míg a többi képernyő megkülönbözteti (`ae.setSequencePlay( seq, false, true )` vs. `( seq, true, true )`: pl. `kick_AppMenuOptions.cpp:69-70`, `kick_PadColor.cpp:358-359`). (b) A képernyő `ae.playSequence()` / `ae.stopSequence()` (`:352, 368`) hívást használ, a többi `setSequencePlay` / `setSequencePause`-t — eltérő motor-út. (c) Nincs érintéses Play/Stop (nem hívja a `showPlayButton`-t), tehát asztali / iOS / érintéses használatban a transport csak a bridge-hez kötött HW gombokról él. (d) Az `isPlaying` tagot csak a saját `onPlay/onStop` és a `restoreData` állítja; ha a lejátszást máshol indítják/állítják le, a képernyő addig stale (a `onSequenceSelectionChange` csak a kijelölést frissíti, `:427-431`).

**NS-B3 — MPC-n nincs pad-LED visszajelzés, P1.** Az összes LED-hívás (`setPadLED`, `setPadBlinking`, `clearAllPadsMessage`, `setPadColorModeMessage`) olyan parancs, amit a híd eldob (lásd 1., „Közös: pad-LED”). Az MPC pad-jai tehát a kiválasztott program színeit mutatják, nem a sorozat-állapotot (elérhető / játszik / sorban áll). A játszó/várakozó pad-ok kijelzése csak a képernyőn látszik. Reprodukció: Next Sequence megnyitása MPC-n, pad-LED-ek megfigyelése játszás közben.

**NS-B4 — `cellComponent` null-dereferencia a határellenőrzés előtt, P1.** `auto data = ae.getSequenceData( Row ); … data->name.get()` (`:94-100`) megelőzi az `isPositiveAndBelow( Row, ae.getNumSequences() )` ellenőrzést (`:102`). Üres projektnél / sorozat törlése közbeni frissítésnél `nullptr` dereferencia. A `knobCommand` is `seqData->tempo`-t olvas null-védelem nélkül (`:636-642`); projekt nélkül a `KickHome` kapuja véd (`kick_AppHome.cpp:487`), a hosszan futó állapot-váltásnál nem.

**NS-B5 — Hold, Q-Link és copy-paste, P2.** A Hold bekapcsolása törli a sorba állított következőt (`:146-157`) — Akai-szerinti szándék NEM ELLENŐRIZHETŐ. A Q-Link kiosztás (`:634-679`) a Behringer 13 gombjára készült (EN_KNOB_5 = sorozat-választás, 6–10 track, 11–13 metronóm); a Key 61-en csak az 1–4 létezik → a sorozat-választás és a track-paraméterek elérhetetlenek. A kód szó szerint megegyezik a Song `knobCommand`-jával (`kick_song.cpp:466-511`). A `bankChanged` kezelő a *globális* `ui::fileUtils::lastPressedPad = -1`-et nullázza (`:225`) — keresztkapcsolat a File Browser Load célpadjával.

**NS-B6 — apróságok, P2.** A `nextBar` neve „NextT Bar” (`kick_nextSequence.h:96`); `ObservableInt` felesleges `onValueChange` tagja (`kick_nextSequence.h:56`); a `mt::signals::*` kapcsolatok sosem bontódnak (`:203, 222`); a `KickNextSequenceContent::handleCommand` üres (`:553-555`).

### Song
**SG-B1 — Export: működésnek látszó STUB, P1.** Az Export ikon egy „Do You Want To Export <dal> Song” (szerkeszthető szövegmező!) dialógust nyit, az APPLY ág üres (`MODES/kick_song.cpp:411-423`; `SaveFile` `:45-59`). A felhasználó azt hiszi, exportált. Hogy *mit* kellene exportálnia (MIDI-fájl, audio-mixdown, sorozat-lánc, projekt-részlet), nincs specifikálva; az Akai-nál a Save ablak kínálja a Track as MIDI / Pattern, Audio Mixdown, … lehetőségeket (WP-G G-188 – G-191).

**SG-B2 — Convert >SEQ: STUB, P1.** A gomb létezik (`.h:96`), a konstruktor hozzáadja (`:83-85`), de nincs `onClick` és a `setBoundsRelative` kikommentezve (`:283`) → láthatatlan. A dal → új szekvencia konverzióhoz nincs motor-függvény (`ae.*Song*` listában nincs `songToSequence`: `M/engine/interface/source/mt_audio_interface.h:636-645`).

**SG-B3 — A UI a dal végén lezárva marad; a motor viselkedése N/E, P1.** A motor a dal végén `onSongStopped`-ot küld (`M/engine/audio_engine/engine/mt_audio_manager_transport.cpp:281-286`), a Song képernyő erre csak a fejléc Play-állapotát állítja (`kick_song.cpp:374-378`); a `content.isPlaying`, a tábla `setInterceptsMouseClicks( false )` és a Delete tiltása marad (`:216-243`), amíg a felhasználó nem nyom STOP-ot → a pad-okra (`:116-131`), a táblázatra nem lehet kattintani. Ráadásul a `songActive = false; onSongStopped()` ágban **nincs `stop()` hívás** a motorban — az utolsó szekvencia tovább loopolhat; NEM ELLENŐRIZHETŐ KÓDBÓL (hardveres / tesztes ellenőrzés kell).

**SG-B4 — A dal-lépések nem visszavonhatók, Clear/Delete megerősítés nélkül, P1.** A manager függvényei `MTSong song( vt, nullptr )`-lal dolgoznak (`mt_song.cpp:190-220`) — `nullptr` UndoManager → add / edit / delete / clear **kimarad az undo-ból** (a `createSong` viszont `undo`-t használ: `:172-178`). A `clr` gomb (`kick_song.cpp:133-141`) és a `del` (`:168-172`) megerősítés nélkül töröl.

**SG-B5 — Törölt sorozatra hivatkozó lépések csendes eltávolítása és null-deref, P1.** `displayState` minden `updateSong`-nál törli azokat a lépéseket, amelyek `dispName`-je üres (azaz a hivatkozott sorozat már nem létezik): `:207-214` + `MTSongStep::getInfo()` (`mt_song.cpp:44-60`: a név csak találat esetén töltődik). A felhasználó nem kap jelzést; a törlés *nem visszavonható* (SG-B4). A `provider.cellComponent` `seqData->length`-et olvas az `ae.getSequenceData( steps[Row].seqNo )` null-védelme nélkül (`:99-102`).

**SG-B6 — A BPM mező és a Seq/Song kapcsoló csak megjelenít, P1 (szándék N/E).** A `bpmNumber.onValueChanged` → `content.setBpmValue()` (`:430-433`, `:340-348`) csak a táblázat / pad-feliratok BPM-oszlopát írja át (`info.steps[Row].bpm`, `refreshSequenceList`); egyetlen motor-hívás sincs (a fájl egyetlen `setSequenceBPM`-je a Q-Link kiosztás: `:474`). A felhasználó azt hiszi, a dal tempóját állítja.

**SG-B7 — Dal törlése/átnevezése hiányzik, név-validáció nincs, P1.** A „CREATE NEW SONG” bejegyzés a lista *sentinel*-je (`:381`); egy „create new song” nevű valódi dal kiválasztása újra az új-dal-dialógust nyitná, és `ae.isSongExists` nem látja a sentinelt (`mt_song.cpp:222-240`). Üres név engedélyezett (`createSong( "" )`), a billentyűzet kezdőszövege „Enter Name” (`:383`) — törölni kell.

**SG-B8 — Kisebb logikai hibák, P2.** (a) `stopOrPause`: `setPadLED( selectedId + pads->getBankOffset(), … )` (`:239`) a `setPadLED` maga is kivonja az offsetet (`:327`) → a bank-offsettel kétszer számol: B–H bankban a leállított sorozat pad-LED-je nem áll vissza. (b) `playOrResume` a `header.setPlayState( true )`-t hívja üres dalnál is (`:532-536`, a `stopOrPause()` ugyan lefut), a fejléc „játszik” állapotban marad. (c) Lejátszás közben újabb PLAY újraindítja a dalt a kijelölt lépéstől (`:216-228`). (d) Insert kijelölés nélkül az *elejére* szúr be (`insPos = -1` → `addSongStep( …, 0 )`, `:151, 160`), nem a végére; a kijelölt-lépés-ismétlés trükk (azonos sorozat Insert-je +1 repeat) nincs a UI-n jelezve. (e) A `getSelectedRow()` üres kijelölésnél is `0`-t ad (`kick_table.h:240, 153-156`), tehát a Delete kijelölés nélkül az első lépést törli.

**SG-B9 — A `refreshStep` `sendNotification`-nel visszhangozza az értéket a motorba, P2 (gyanú).** `repeat.setValue( step.repeatCount, juce::sendNotification )` (`:600`) → `repeat.onValueChanged` → `onRepeatChanged` → `ae.editSongStep( … )` (`:108-111`) minden táblafrissítésnél, ha az érték változott (pl. Insert után eltolt sorok); felesleges motor-írás és zaj a ValueTree-ben. Idempotens, de redundáns.

**SG-B10 — MPC-n nincs pad-LED.** Azonos az NS-B3-mal (`setPadLED` `:325-333`, `clearAllPadsMessage`).

### Color Edit
**CE-B1 — „Make Default” nem az alapértelmezett színeket állítja vissza, P1.** `ae.setProgramPadColour( prog, 3, -1 )` (`MODES/kick_PadColor.cpp:104-108`): minden 128 padot a **3-as** színre (`padColourlightGreen1`, `M/contracts/dcl/dcl_contracts.h:37`) állítja be. Az engine tényleges alapértelmezései típusonként mások: Drum `defaultDrumColours`, Clip `defaultClipColours`, minden más `defaultPadColours` (`M/engine/audio_program/source/mt_audio_iprogram.h:37-45`; kiolvasás: `M/engine/audio_engine/engine/mt_audio_interface_types.cpp:359-374`). Ráadásul mindhárom tömb csak az első **16** elemét inicializálja (a `defaultPadColours` csak az elsőt: `{ PadColourRed }`), a maradék 112 elem 0 = `padColourDarkRed` → a B–H bank padjai alapból sötétvörösek (Drum esetén is). Reprodukció: Make Default egy drum programon → minden pad zöld.

**CE-B2 — MPC-n a színválasztás és a módok hardveres hatása hiányzik/késik, P1.** A `buttonClicked` közvetlenül `setPadDataMessage( EN_PAD1 + pad, StateOn, colour )`-t, All Pads esetén `setAllPadDataColourInfoMessage`-et küld (`:302-317`), a `ae.setProgramPadColour` pedig a `padColourChanged` jelen át ugyanezeket (`M/kick_leds/kick_leds.cpp:30-37`); a mód-váltók `setPadColorModeMessage`-et (`:164-166`). A híd mindet eldobja (`bridge:138-160`). A hardver pad-ok csak a következő `refreshPadLeds`-nél (képernyő-belépés `:398`, bank-váltás `:331`, program-váltás `:63`) kapják meg a színt a támogatott `SET_COLOUR_STATE`-en át — a `buttonClicked` végén a `refreshLEDs()` nem hívódik (a `refresh()`-ben ki van kommentezve: `:282`). A *Pad Off / Pad On / Empty Pad* módok (DIM / OFF / VELOCITY / FULL, `mt::PadColorMode`) a híd saját szín-/dim-logikájában egyáltalán nem szerepelnek (`A/hardware/pad_colour_map.h:41-51`: csak `SampleAssigned` / `NotAssigned` állapot szerinti két fix halványítás), tehát a Color Edit módválasztása MPC-n hatástalan. Az Akai-ban a Pad Color nézet egyik fő célja a hardver pad színe.

**CE-B3 — A hardveres pad nem színez, Data Dial nem választ, P1.** A pad-ütés élő MIDI-t küld (`:364-387`), a színezés csak a képernyő rácsának koppintásával megy (`right.pads->OnButtonClicked → buttonClicked`, `:67-69`). MPC-n a természetes használat („nyomd meg a padot, amit színezni akarsz”) nem lehetséges, és a 18 színt nem lehet Data Dial-lal léptetni.

**CE-B4 — Érvénytelen mód küldése, P2.** `sendColourModeToHardware` `padColorMode = INVALID`-dal indul, és ha a `padOff` sem `PAD_OFF_STATE` (1), sem `PAD_DIM_STATE` (0) → `INVALID` megy a hardverre és a programba (`:128-166`; `mt::PadColorMode::INVALID = -1`, `M/engine/interface/source/mt_audio_interface_types.h:149-152`). A `Pad Off`/`Pad On` kombinációk (2 × 3 + Empty) 9 módot adnak, a visszaolvasás (`getPadOffColourMode` stb., `:169-229`) nem szimmetrikus (pl. `OFF` → `Off/Off`, `INVALID` → `Off`); egységteszt nincs.

**CE-B5 — Mellékhatások és teljesítmény, P2.** (a) `refresh()` minden kattintásnál mind a 128 padra `setBorderColor`-t hív és újraépíti a feliratokat (`:231-283`, `:319`); elég az érintett pad. (b) A `setBorderColor( pad, juce::Colour( left.selectedColourId ), … )` (`:307`) az enum *számát* ARGB-nek veszi (tehát majdnem átlátszó fekete), a következő `refresh()` felülírja — halott sor. (c) A `visibilityChanged` minden megjelenítéskor `ae.setProgramEmptyPadColorMode( … )`-ot ír (`:411`) felhasználói művelet nélkül. (d) A `handleCommand` pad-ága alapértelmezetten konstruált (üres SysEx) `MidiMessage`-et is átad, ha az állapot nem PRESSED/RELEASED (`:377-384`). (e) Beégetett pixel-pozíciók a `resized()`-ben (`:342-346`) — mobil T-layoutban rossz hely. (f) All Pads módban megerősítés / undo nélkül ír mind a 128 padra; hogy az `setPadColor` undo-zható-e (`MTIProgramData::padColour` ValueTree-referencia, `M/engine/audio_program/source/mt_audio_iprogram.cpp:250-259`), NEM ELLENŐRIZHETŐ KÓDBÓL.

## 4. Tervben / hiányzik (Akai MPC Standalone-hoz képest)

- **Akai-összevetés mindhárom módra hiányzik** (NEM ELLENŐRIZHETŐ): WP-B (Next Sequence, Song) és WP-C (Pad Color) nincs kész; sem a doksi, sem a `KEY61_DOC_VS_KICK.md` nem ír róluk. A `known-issues-and-roadmap.md` a Next Sequence / Song / Color Edit-et csak a leltárból (`screen-inventory.md` §4.21 – 4.23) említi: Next Sequence és Color Edit „KÉSZ, logika mélysége nem követve az engine-ig”, Song Export / Convert STUB.
- **Song**: Export és Convert >SEQ (STUB); dal törlése / átnevezése; undo; az Akai Save ablak szerinti exportok (Track as MIDI / Pattern G-188, Audio Mixdown G-190, G-191) — a Song Export tartalmáról döntés kell.
- **Next Sequence**: Key 61 gomb nincs; érintéses transport nincs; a Hold/Sudden/Next Bar/Clear Akai-szerinti pontos viselkedése N/E.
- **Color Edit**: a pad-szín hardveres megjelenítése MPC-n (lásd CE-B2); Akai Pad Color 3 mód viselkedése N/E.
- **Pad-LED réteg**: a híd teljes DCL pad-parancs-támogatása (`SET_SINGLE_PAD`, `SET_ALL_SINGLE_COLOUR`, `SET_BLINKING`, `COLOR_MODE`, villogás-animáció) — `screen-inventory.md` §5 jelzi a hiányt; a `known-issues` H8 (gomb-LED hiányok) a gombokról szól, a pad-LED parancsok eldobása ott nincs külön tétel.
- **Indítás / MPC-viselkedés**: a Song BPM és a Looper/Song tempó-szemantika az Akai-nál N/E.

## 5. Javaslataim

1. **A `std::abort()` sorok azonnali eltávolítása, állapot-normalizálással.** `restoreData`: ha `!isPlaying && nextSequence >= 0` → `nextSequence = -1; ae.setNextSequence( -1 )`; `clearNextSequence`: ha `playingSequence < 0` → `playingSequence = ae.getSelectedSequence()`. A tiszta állapotgép külön osztályba (`NextSequenceModel`: playing, next, hold, bank → átmenetek), amit host-teszt fed — a LED-/pad-kimenet külön réteg.
2. **Közös pad-LED réteg a hídban.** Az UI minden pad-parancsát (`SET_SINGLE_PAD`, `SET_ALL_SINGLE_COLOUR`, `SET_BLINKING`, `COLOR_MODE`) a híd egy `PadLedState`-be gyűjti (16 pad: szín, állapot, villogás), a timer rajzolja (a meglévő `m_pad_dirty` mechanizmusra építve) és a villogást animálja; a `SET_COLOUR_STATE` marad a projekt-szín alapréteg. Így mindhárom képernyő (és a többi `setPadDataMessage` felhasználó: AssignSample, PadMute, TrackMute, Sampler, StepSeq) működik MPC-n. Indoklás: NS-B3, SG-B10, CE-B2 egyetlen gyökérokra mennek vissza. Alternatíva: a képernyők a támogatott `setPadColourAndStateInfoMessage`-t használják (16 szín + állapot), villogás nélkül.
3. **Transport egységesítése**: egy közös `TransportActions` (PLAY → `setSequencePlay( seq, false, true )`, PLAY START → `( seq, true, true )`, STOP → `setSequencePause`), minden mód-képernyő ezt hívja; a Next Sequence és a Song kapjon fejléc Play/Stop gombot (`showPlayButton` + `onPlayStopClick`), és az `isPlaying`-et a `mt::signals::playStateChanged` jelből vezesse (ne saját tagból).
4. **Song: undo és megerősítés**: a manager a projekt `UndoManager`-ét kapja (nem `nullptr`-t), a Clear / Delete megerősítést kér vagy undo-zható; a törölt sorozatra hivatkozó lépések *jelölve* maradjanak (piros „missing”), ne tűnjenek el csendben. Dal törlése / átnevezése (hosszú nyomás a listán).
5. **Song Export döntés**: amíg nincs specifikálva, az ikon legyen rejtett vagy szürke („Coming soon”) — hamis megerősítő dialógus helyett. Convert >SEQ: motor-függvény tervezése (`songToSequence`) vagy kivétel az UI-ból.
6. **Color Edit**: „Make Default” = a program típusa szerinti `defaultXxxColours` visszaállítása (a tömbök teljes 128 elemmel, bankonként értelmes alapszínekkel); hardver-pad színezés és a Data Dial a 18 színhez; színezés után `refreshLEDs()`; a módválasztó (Pad Off / Pad On / Empty) **megjelenítése a hardveren is** a híd `COLOR_MODE` támogatásával, vagy a vezérlők elrejtése MPC-n (félrevezető, hatástalan kezelőszerv ne maradjon).
7. **Q-Link közös kiosztás**: egy `SequenceQLinkMap` (1 = BPM, 2 = Length, 3 = Loop Start, 4 = Loop End; null-védelem) az NS és SG közös használatára; a Key 61 oszlopváltással (4 Q-Link × oszlopok) kiterjeszthető.
8. **Tesztelhetőség**: (a) `PadColorMode` ↔ (Pad Off, Pad On, Empty) leképezés tiszta függvényekbe, round-trip teszt mind a 9 módra (CE-B4); (b) Next Sequence állapotgép host-teszt; (c) Song lépés-műveletek (insert / edit / delete / clear / undo, törölt sorozat-hivatkozás) tmp ValueTree-n; (d) híd: az összes `EN_PAD_*` parancs leképezése (`pad_colour_map.h` bővítés) a `PadColourMapTests` mintájára.

## 6. Hardveres és unit-tesztek

**Ami ma fedi.**
- `PadColourMapTests` (`kick-pad-colour`): a híd pad-szín-palettáját veti össze a `theme.xml`-lel — a `SET_COLOUR_STATE` útvonal fedett, a többi pad-parancs nincs.
- `AkaiMapTests` / `LedMapTests`: pad- és gombtérkép; a három képernyő logikáját nem fedik.
- `AppSmoke.py` / `KICK_SMOKE_SCREENS`: `MenuNextSequence`, `MenuSong`, `MenuColorEdit` képernyőkép készíthető (`A/Main.cpp:152-198`), csak külön környezeti változóval; az alapteszt nem járja végig.
- Nincs teszt: a Next Sequence állapotgép, a Song műveletek, a Color Edit mód-leképezés, a `std::abort()` ágak, a pad-LED parancsok.

**MPC-n ellenőrzendők.**
1. **Next Sequence abort** (NS-B1): (a) PLAY → pad = következő sor → STOP → Bank B; (b) játszás → sorba állítás → Menü → STOP → vissza. Várt: `/data/kick.crash`.
2. Next Sequence: PLAY vs. PLAY START viselkedés; Next Bar/Sudden/Hold/Clear sorozatban; pad-LED-ek (nem látszanak, NS-B3); bankváltás játszás közben; Q-Link 1–4 hatása (és hogy a Knob Edit módtól függ-e).
3. Song: dal létrehozása, 3–4 lépés Insert, PLAY a 2. lépéstől, a dal vége után a UI zárva marad-e és szól-e még a hang (SG-B3); STOP → kijelölés a 0. sorra; Clear/Delete + visszavonás (SG-B4); Seq/Song BPM kapcsoló hatása a hangzó tempóra (SG-B6).
4. Song: törölt sorozatra hivatkozó dal megnyitása (SG-B5); „create new song” nevű dal (SG-B7); üres név.
5. Color Edit: egy pad színezése — azonnal látszik-e a hardver pad-on (CE-B2), a bank váltás után; a Pad Off / Pad On módok hatása a hardveren; Make Default eredménye (CE-B1); B–H bank alapszínei.
6. Color Edit hardveres pad-nyomás: csak hang (CE-B3).
7. Menü → mindhárom képernyő megnyílik-e, Back hova visz.

## 7. TODO

Függőségek: NS-03 → NS-02; NS-05, SG-10, CE-04 ← NS-04 (közös pad-LED réteg a hídban); SG-01 ← SG-03 (undo); CE-01 ← CE-02; NS-11 ← NS-01.

*Next Sequence — hibák*
- [ ] **NS-01** (P0, S) Az `std::abort()` sorok eltávolítása (`restoreData` `:397-400`, `clearNextSequence` `:336`), állapot-normalizálással; regressziós teszt a két reprodukciós úttal — `MODES/kick_nextSequence.cpp`.
- [ ] **NS-02** (P1, S) `cellComponent`: a null-védelem a `getSequenceData` után, a határellenőrzés előtte; `knobCommand` null-védelme — `:92-110, 634-679`.
- [ ] **NS-03** (P1, M) Transport egységesítés: PLAY ≠ PLAY START, közös `TransportActions`, fejléc Play/Stop (`showPlayButton`, `onPlayStopClick`), `isPlaying` a `playStateChanged` jelből — `:350-369, 406-420, 619-632`, `M/kick_ui_header/header/kick_modeHeader.h:41`. Függ: NS-02.
- [ ] **NS-04** (P1, L) Közös pad-LED réteg a hídban (`SET_SINGLE_PAD`, `SET_ALL_SINGLE_COLOUR`, `SET_BLINKING` villogás-animációval, `COLOR_MODE`) — `bridge:138-160`, `A/hardware/pad_colour_map.h`, `M/contracts/dcl/dcl_contracts_helper.cpp:181-288`; vagy a három képernyő átállítása `setPadColourAndStateInfoMessage`-re. Döntés: melyik irány. (A SG-10, CE-02 és a többi pad-LED-es képernyő is erre épül.)
- [ ] **NS-05** (P2, S) Hold-viselkedés tisztázása (Akai WP-B alapján), a Hold bekapcsolás-törlés szándékossá tétele vagy javítása — `:146-162`. Függ: NSC-01.
- [ ] **NS-06** (P2, S) Q-Link kiosztás közös helperbe (`SequenceQLinkMap`) az SG-vel, a Key 61-hez (1–4 + oszlopváltás) igazítva — `:634-679`, `kick_song.cpp:466-511`.
- [ ] **NS-07** (P2, S) A `lastPressedPad` keresztkapcsolat megszüntetése (`bankChanged` kezelő) — `:225`; kapcsolódik: FB-10-es cél-pad modell (`file-browser.md` B10).
- [ ] **NS-08** (P2, S) Takarítás: „NextT Bar” név, `ObservableInt::onValueChange`, signal-lekapcsolás, üres `handleCommand` — `kick_nextSequence.h`, `:203, 222, 553`.

*Next Sequence — tesztek*
- [ ] **NS-09** (P1, M) `NextSequenceModel` állapotgép kivonása + host-teszt (playing / next / hold / clear / bank átmenetek; a két abort-forgatókönyv). Függ: NS-01.

*Song — hibák*
- [ ] **SG-01** (P1, M) Undo-bekötés: a `MTSongManager::addSongStep / editSongStep / deleteSongStep / deleteAllSongSteps` a projekt `UndoManager`-ét használja (`nullptr` helyett); Clear/Delete megerősítés vagy undo — `M/engine/audio_engine/song/mt_song.cpp:190-220`, `kick_song.cpp:133-172`.
- [ ] **SG-02** (P1, S) A dal végén a UI feloldása (`onSongStopped` → `stopOrPause`-szerű reset), és a motor viselkedésének tisztázása (az utolsó sorozat loopol-e) — `kick_song.cpp:374-378, 216-243`, `mt_audio_manager_transport.cpp:281-286`.
- [ ] **SG-03** (P1, S) Törölt sorozatra hivatkozó lépések: jelölés ahelyett, hogy `displayState` csendben törölné; `cellComponent` null-védelme — `kick_song.cpp:98-103, 207-214`, `mt_song.cpp:44-60`.
- [ ] **SG-04** (P1, S) A BPM mező / Seq-Song kapcsoló szemantikájának tisztázása (és vagy motor-hívás, vagy a kezelőszerv eltávolítása) — `kick_song.cpp:340-348, 424-433`.
- [ ] **SG-05** (P1, M) Dal törlése / átnevezése (UI) + név-validáció (üres, sentinel-ütközés, előtöltött „Enter Name” törlése) — `kick_song.cpp:380-409`, `mt_song.cpp:180-181`.
- [ ] **SG-06** (P2, S) Kisebb hibák: `stopOrPause` dupla bank-offset, `playOrResume` fejléc-állapot üres dalnál, PLAY közbeni újraindítás, Insert pozíció kijelölés nélkül, `getSelectedRow()`-0 Delete — `kick_song.cpp:216-243, 143-172, 528-542`.
- [ ] **SG-07** (P2, S) `refreshStep` `sendNotification` → `dontSendNotification` + külön felhasználói-változás ág (felesleges `editSongStep`) — `kick_song.cpp:600, 108-111`.

*Song — hiányzó funkciók*
- [ ] **SG-08** (P1, M) Export: specifikáció (mit exportál: MIDI / audio / lánc) és megvalósítás — vagy az ikon elrejtése, amíg nincs; az üres APPLY ág és a hamis megerősítő szöveg megszüntetése — `kick_song.cpp:411-423`. Döntés kell (owner).
- [ ] **SG-09** (P2, L) Convert >SEQ: motor `songToSequence` + UI, vagy a halott tag/gomb eltávolítása — `kick_song.cpp:83-85, 283`, `kick_song.h:96`.
- [ ] **SG-10** (P1, M) Pad-LED MPC-n (lásd NS-04); érintéses Play/Stop (NS-03). Függ: NS-04.

*Song — tesztek*
- [ ] **SG-11** (P1, M) Song lépés-műveletek host-teszt (tmp ValueTree: insert / edit / delete / clear / undo / törölt sorozat-hivatkozás). Függ: SG-01, SG-03.

*Color Edit — hibák*
- [ ] **CE-01** (P1, S) „Make Default” = a program típusának alapszínei (nem fix 3-as); az alapérték-tömbök teljes 128 elemmel, bankonként értelmes színekkel — `kick_PadColor.cpp:104-108`, `M/engine/audio_program/source/mt_audio_iprogram.h:37-45`, `mt_audio_interface_types.cpp:359-374`.
- [ ] **CE-02** (P1, M) Hardver pad-LED színezés után MPC-n: `refreshLEDs()` a `buttonClicked` végén és a híd `SET_SINGLE_PAD` / `ALL_SINGLE` / `COLOR_MODE` támogatása (vagy a módválasztók elrejtése MPC-n) — `kick_PadColor.cpp:282, 290-321, 128-167`, `bridge:138-160`. Függ: NS-04.
- [ ] **CE-03** (P1, M) Hardveres pad = színezés (Single módban), Data Dial = színpaletta léptetése, ± = bank — `kick_PadColor.cpp:349-388`.
- [ ] **CE-04** (P2, S) `INVALID` mód kizárása, a mód-leképezés tiszta függvényekbe — `:128-229`.
- [ ] **CE-05** (P2, S) `refresh()` csak az érintett padra; halott `setBorderColor( Colour( id ) )` törlése; `visibilityChanged` ne írjon engine-állapotot; üres `MidiMessage` védelme; relatív layout — `:231-283, 307, 342-346, 377-384, 411`.
- [ ] **CE-06** (P2, S) All Pads megerősítés és undo-bekötés ellenőrzése — `:309-317`, `mt_audio_iprogram.cpp:250-259`.

*Color Edit — tesztek*
- [ ] **CE-07** (P1, S) `PadColorMode` ↔ (Pad Off, Pad On, Empty) round-trip teszt mind a 9 módra. Függ: CE-04.
- [ ] **CE-08** (P2, S) Híd `EN_PAD_*` parancs-leképezés teszt (`pad_colour_map.h` bővítés a `PadColourMapTests` mintájára). Függ: NS-04.

*Közös*
- [ ] **NSC-01** (P1, M) Az Akai-összevetés elvégzése (WP-B: Next Sequence, Song; WP-C: Pad Color) a `WALKTHROUGH_PLAN.md` szerint — a Hold / Sudden / Next Bar / Clear, a Song Export és Convert, a Pad Off / Pad On / Empty Pad módok Akai-szemantikája, a Key 61 elérés. Blokkolja: NS-05, SG-08, CE-02 (módválasztók).
- [ ] **NSC-02** (P1, M) MPC hardveres ellenőrzőlista végrehajtása (6. szakasz), különösen NS-B1 (összeomlás) és a pad-LED viselkedés — a hardveres session végezze.
