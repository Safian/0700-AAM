# Sample Edit — review és TODO

Alap: **statikus kódolvasás** (olvasás kezdete: HEAD `40fb2db6`, ág `claude/kick-plans`, 2026-11-04; közben a Kick-kódot csak `168417a1` érintette — Looper-összeomlás-javítás —, az itt idézett fájlokat nem), semmi nem lett futtatva, hardveren nem mértem. A „KÉSZ” azt jelenti: van bekötött handler az engine-ig, nem azt, hogy hallgatással/hardveren igazolt. Ahol a kód és a régi doksi ellentmond, a kód az igazság. Jelölések: `M/` = `Kick/modules/`, `UI/` = `Kick/modules/kick_ui/audio_components/sample_editor/`, `ENG/` = `Kick/modules/engine/audio_engine/engine/`. Alap-dokumentumok: `research/screen-inventory.md` §4.8, `research/known-issues-and-roadmap.md`, `docs/akai_mpc_hardware/KEY61_DOC_VS_KICK.md` (#20), `walkthrough/WP-D.md`, `WP-G.md`.

## 1. Áttekintés (mit csinál, hogyan lehet elérni, hardver-gombok kötése)

**Mit csinál.** Egy minta (asset) hullámforma-szerkesztője. Két fül: **Tune** (alapértelmezett: `UI/kick_SampleEditor.cpp:165` `tuneCompButton.triggerClick()`; slice-lista, BPM/Semi/Fine mezők) és **Chop** (Manual / Threshold / Regions / Quantize szeletelés, Start/End/Cue, Split/Combine/Legato/Play Cue/Clear All). Fejléc: Settings (Cue/Slice preview, Auto scroll, Timeline units), Assign (padhoz rendelés), **Process** (külön képernyő, `Screen::ProcessEdit`, 14 művelet: `UI/kick_SampleEditor.cpp:125-129`, `310-345`, `395-531`). A szerkesztés **helyben, destruktívan** írja át a forrás-WAV-ot, a visszavonáshoz `undo_sampleeditor.wav` segédfájlt használ (`ENG/mt_sample_editor_actions.cpp:15-25, 336-349`). A régiókat (szeleteket) a projekt-fa `SAMPLEEDITOR/SAMPLESTATE/REGIONS` ágában tárolja, abszolút fájlútvonal szerint (`UI/kick_SampleEditorData.cpp:610-686`).

**Elérés.**
- Hardver: SAMPLE EDIT gomb (Akai-térkép note 6 → `HW_BTN_SAMPLE_EDIT`: `Kick/apps/kick/hardware/akai_map.h:89`), a híd a képernyőt kapcsolja (`kick_hardware_bridge.h:1293-1300`; Shift+gomb = Sampler). LED: `kick_hardware_bridge.h:279-280`. A gombpár a MPC-n **mért**: megnyitja a Sample Editort / Samplert (`docs/AKAI_MPC_KEY61_HW_TEST_REPORT.md:98-99, 234`).
- Érintés: Menü csempe #10 (`M/kick_ui/dialogs/kick_AppMenuOptions.cpp:97`), Sampler „Edit” gomb (`M/kick_ui/audio_components/sampler/kick_Sampler.cpp:208-215`).

**Hardver-kötés a képernyőn** (`UI/kick_SampleEditor.cpp:168-244`, `UI/kick_SampleEditorOptions.cpp:1011-1088`):
| Vezérlő | Hatás | MPC Key 61-en elérhető? |
|---|---|---|
| PLAY / PLAY START / STOP | a **szekvencia** transzportja (nem a minta előhallgatása) — `kick_SampleEditor.cpp:170-178` | igen |
| Pad 1–8 Tune (trim) módban | előhallgatás: 1/5 from start, 2/6 to start, 3/7 to end, 4/8 from end; a 5–8 felengedésre leáll (`:200-244`) | igen (16 pad) |
| Pad Chop módban | pad = slice (`padNum + bankOffset`) kiválasztása + lejátszás; ha nincs ilyen slice és fut a lejátszás: új vágás a cue pozíciónál (`Options.cpp:1033-1088`) | igen |
| Q-Link 1–4 (Chop) | **Start** léptetése 1 / 10 / 100 / 1000 mintával (`Options.cpp:1011-1021`) | igen |
| Q-Link 5–8 (Chop) | **End** léptetése (`Options.cpp:1023-1030`) | **nem** — a Key 61 hídja csak 4 Q-Linket ad (`kick_hardware_bridge.h:1529-1541`, `idx < 4`) |
| Q-Link 9 / 10 | zoom (`kick_SampleEditor.cpp:189-198`) | **nem** (ugyanez) |
| Data Dial (CENTER) | ezen a képernyőn nincs kezelő (`knob != EN_KNOB_CENTER` szűrés: `Options.cpp:1085`); csak a híd általános „utoljára érintett vezérlő” útja működhet (IGAZOLT más képernyőn: `known-issues` §1, 0a42bfdb) | ezen a képernyőn NEM ELLENŐRIZHETŐ KÓDBÓL |
| Undo / Redo gomb | a híd közvetlenül a **globális** UndoManagert hívja (a Sample Edit műveletei is ide mennek: `ENG/mt_sample_editor_interface.cpp:231-234`) | igen |

**Érintés.** A MPC-n a „manual” kézbesítés fut (`known-issues` §1; `kick_hardware_bridge.h:563`): a híd a legmélyebb találati komponens `mouseDown/mouseDrag/mouseUp` metódusát hívja közvetlenül, **`mouseMove` (hover) nélkül**. Egyujjas.

## 2. Funkciók és állapotuk

| Funkció | Állapot | Bizonyíték |
|---|---|---|
| Képernyő-nyitás hardverről, LED | KÉSZ (gombpár mért) | `akai_map.h:89`; `bridge:1293-1300`; RPT:98,234 |
| Minta-választó lista (asset-lista), betöltés | KÉSZ (a kezdő kiválasztás furcsa: `samples.size() > 1 ? 1 : 0`, azaz 2 minta esetén a 2.-at jelöli ki) | `UI/kick_SampleEditor.cpp:84-97, 246-257` |
| Hullámforma, skála, zoom gombok (+/−), görgetés | KÉSZ | `UI/kick_SampleEditor.cpp:130-137`; `UI/kick_SEViewComponent.cpp:326-358` |
| Egérgörgős zoom, Erase/Listen/Zoom eszköz | STUB (az eszköz nem állítható: `setTool` hívója nincs) | `UI/kick_SampleEditorData.h:438`; `UI/kick_SEViewComponent.cpp:283-296, 318-324` |
| Region-fogantyú / cue / loop húzása | BUGOS GYANÚ érintésre (a fogás `mouseMove`-ban dől el) | `UI/kick_SEViewComponent.cpp:210-261`; `UI/kick_SampleEditorCue.cpp:52-71`; `UI/kick_SEViewComponent.cpp:478-496` |
| Manual vágás a hullám tetejére koppintva | BUGOS GYANÚ (a `selectedTool` inicializálatlan, UB) | `UI/kick_SampleEditorData.h:533`; `UI/kick_SampleEditorData.cpp:1093-1096`; `UI/kick_SEViewComponent.cpp:276-282, 372-378` |
| Chop: Regions (egyenlő szeletek, 1–128) | KÉSZ | `Options.cpp:916-918, 987-1003`; `Data.cpp:165` |
| Chop: Threshold (+MIN) | RÉSZLEGES (működik, max 127 vágás; a „MIN SAMPLES” felirat **ms**-ot jelent) | `ENG/mt_sample_editor_interface.cpp:256-357` (`:322-323`, `:348`); `Options.h` (`minSamplesNumber 0..1000`) |
| Chop: Quantize (Bars × Time Div) | RÉSZLEGES (fix 4 beat/ütem, a tempó/jegyzet nem számít; csak egyenlő osztás) | `Options.cpp:974-985` |
| Split / Combine / Clear All / Slice+ / Legato / Play Cue | KÉSZ | `Options.cpp:932-962` |
| Chop: **Repeat** gomb, **X FADE** szám | STUB (nincs `onClick`; üres lambda) | `Options.cpp:913-915` (nincs `repeat.onClick` a `SampleEditorChopOPtions`-ban) |
| Chop-szeletek mentése az engine-be (asset-slice → pad) | **HIÁNYZIK / STUB** — a szeletek csak a szerkesztő fa-állapotában élnek; sehol nincs `addAssetSlice` a sample_editor mappában (egyedül a Sampler hívja) | `grep addAssetSlice` → csak `sampler/kick_Sampler.cpp:271,318,323,327-335` |
| Assign (padhoz rendelés) | RÉSZLEGES (csak Drum programra, csak a **teljes** fájlt, a „Slice type / Assign to / Crop” vezérlőket figyelmen kívül hagyja; Keygroup/Clip/Audio célra némán nem csinál semmit) | `UI/kick_SampleEditor.cpp:138-155`; `kick_SampleEditor.h:161-188` (`getAssingType/getSliceType/isCropSampleEnable` nincs hívva) |
| Tune fül: Detect BPM / To Seq / Match BPM | STUB (nincs `onClick`) | `Options.cpp:1125-1180` |
| Tune fül: Semi / Fine | STUB (nincs `onValueChanged`) | `Options.cpp:1133-1134, 1205-1206` |
| BPM-detektálás (`detectBpm`) | STUB: a detektor ki van kommentezve; `numBeats = 65`, `bpm = 65*60/hossz` | `ENG/mt_sample_editor_interface.cpp:422-473` |
| Beat-skála / Timeline „Beats” | BUGOS (minden minta 65 beatnek látszik, `totalBeats = len/60*bpm = 65`) | `Data.cpp:586-604`; `UI/kick_SEScaleComponent.cpp:96, 196` |
| Process: Discard / Delete / Silence / Normalize / Reverse / Gain | KÉSZ kód szerint (nem hallgatással) | `ENG/mt_sample_editor_interface.cpp:183-213`; `ENG/mt_sample_editor_actions.cpp:395-449, 644-720` |
| Process: **Fade In / Fade Out** | **STUB — no-op** (a görbe-alkalmazás ki van kommentezve; a fájl változatlanul íródik újra, undo-bejegyzéssel, „sikerrel”) | `ENG/mt_sample_editor_actions.cpp:451-479, 481-505` |
| Process: **Pitch Shift, Time Stretch, Bit Reduce** | **STUB** — `return false`; a UI megnyitja a párbeszédet, az eredményt (false) eldobja | `ENG/mt_sample_editor_interface.cpp:207-210, 217-218`; `Data.cpp:1110-1121` |
| Process: Extract / Copy / Stereo>Mono | BUGOS (lásd §3 B3) | `ENG/mt_sample_editor_actions.cpp:375-379, 626-640, 724-790`; `UI/kick_SampleEditor.cpp:409-418, 468-473, 517-531` |
| Undo / Redo (minta-műveletek + régió-szerkesztés) | RÉSZLEGES (globális UndoManager; mintaváltás törli az egész projekt undo-ját) | `ENG/mt_sample_editor_interface.cpp:361-369`; `Data.cpp:1227-1235` |
| Előhallgatás (MTSamplePlayer, a motor eszköz-callbackjén) | KÉSZ (konstans-hiba a cue-preview hosszában) | `ENG/mt_sample_player.cpp:3-4, 98, 144` |
| Settings: Cue Preview, Slice Preview, Auto Scroll | KÉSZ | `UI/kick_SampleEditorSettings.cpp:18-35`; `kick_SampleEditor.cpp:55-72` |
| Settings: Cue Play (toggle/one-shot), második Slice Preview | STUB (üres lambda) | `UI/kick_SampleEditorSettings.cpp:15, 28` |
| Settings: Timeline Units kezdőérték | BUGOS (kétszer ugyanaz a feltétel → „Samples” sosem jelölődik) | `UI/kick_SampleEditorSettings.cpp:45-46` |
| Régiók perzisztenciája (projekt-fa) | KÉSZ (útvonal-kulcsú) | `Data.cpp:610-686` |
| Fájlformátumok | RÉSZLEGES: olvasás JUCE basic formats (wav/aif/flac/ogg/mp3…), **írás mindig WAV-bájtok az eredeti kiterjesztéssel** | `ENG/mt_sample_editor_actions.cpp:244-261, 336-349`; `mt_audio_asset_manager.cpp:177` |
| `Regions::loopSelectedRegion()` | STUB (üres) | `Data.cpp:232` |
| Holt (lefordított, de sehol nem példányosított) kód: `SampleEditorChopOptions`, `…TrimOptions`, `…ProgramOptions`, `FromBPMMatch`, `SampleEditorLayer*`, `SampleLayerSettings`, `SampleEditorProcess/ProcessAction`, `SampleEditorAssign`, `SaveSampleDialog` | HOLT KÓD (~2000 sor) | `kick_SampleEditor.h:65-66` csak `…TuneOPtions` + `…ChopOPtions`-t használ; `kick_ui.cpp:43-53,88` |

## 3. Mi nem működik / hibák

**B1 (P0) Fade In / Fade Out semmit nem csinál.** `MTSampleEditorFadeInAction/FadeOutAction::performAction` beolvassa a blokkot, az `AudioFadeCurve::applyCrossfadeSection` hívás ki van kommentezve, `true`-t ad (`ENG/mt_sample_editor_actions.cpp:451-479, 481-505`). Hatás: a felhasználó „sikeres” műveletet lát, a hang változatlan; közben a teljes fájl újraíródik és undo-bejegyzés keletkezik. Repro: minta → Process → Fade In → Apply → hullámforma/hang azonos.

**B2 (P0) Pitch Shift, Time Stretch, Bit Reduce nincs.** `ENG/mt_sample_editor_interface.cpp:207-210, 217-218` `return false`; `SampleEditorData::performAction` nem vizsgálja a visszatérést (`Data.cpp:1110-1121`), tehát semmilyen visszajelzés. Az élő `ProcessSlice` Time Stretch lapja (Beat/Ratio/Original/New Tempo) kezelő és paraméter nélküli (`kick_SampleEditor.cpp:505-516`: csak láthatóvá teszi a mezőket, a `params` üres marad). A `inventory` §4.8 „14 művelet mind dispatch-elt” állítása így félrevezető: **3 stub (Pitch, Time Stretch, Bit Reduce) + 2 no-op (Fade In/Out)**; működő: Discard, Delete, Silence, Normalize, Reverse, Gain, Extract/Copy/Stereo>Mono (hibákkal, B3).

**B3 (P0) Extract / Copy / Stereo>Mono hibás.**
 a) A paraméter-lista **üres**, ha a felhasználó nem szerkeszti a nevet: az `ProcessSlice` a névmezőt csak `sampleName.onTextChange`-ben tölti a `params`-ba (`kick_SampleEditor.cpp:409-418`), a `case 3` / `case 11` ágak nem pusholnak semmit (`:468-473, 517-521`). Az engine `params.begin()->toString()` (üres `std::list` → UB/összeomlás): `ENG/mt_sample_editor_actions.cpp:636, 732`. Stereo>Mono-nál `params.back()` a „Left/Sum/Right” indexet adja fájlnévnek (`:751`; a `case 13` csak egy paramétert pushol: `kick_SampleEditor.cpp:528-531`).
 b) `execute()` a `noProcessing` ágon **visszatér a takarítás előtt** (`ENG/mt_sample_editor_actions.cpp:375-379`), a destruktor üres (`mt_sample_editor_actions.h:102`) → szivárgó reader/writer, nyitva maradó fájlkezelő, és a minta mellett marad egy `<név>_temp.wav` (a `initializeReaderWriter` hozza létre: `:244-261`). Minden Extract/Copy/Stereo>Mono után.
 c) A kimeneti fájlt senki nem regisztrálja assetként (`Data.cpp:1110-1121` nem hív `addAsset`), a szerkesztő listájában nem jelenik meg.
 d) Stereo>Mono „Sum” a két csatornát **összeadja** (`:783`, nincs ×0,5) → +6 dB, vágás.

**B4 (P0) A Chop-szeletek nem jutnak el a lejátszó motorig.** A régiók csak a szerkesztő fa-állapotában élnek; `addAssetSlice` egyedül a Samplerből hívódik (`sampler/kick_Sampler.cpp:327-335`). Az Assign párbeszéd „Assign Slice To Pad / Make New Sample”, „Non-Destructive Slice / Pad Parameters”, „Crop sample” vezérlői bekötetlenek (`kick_SampleEditor.cpp:138-155` mindig a teljes fájlt adja a padnak). Hatás: a szeletelés (a képernyő fő célja) nem használható pad-kiosztásra. Repro: Chop → Regions 8 → Assign → pad: az egész minta kerül a padra.

**B5 (P1) Érintéssel a fogantyúk/cue nem fogható meg.** A cue/loop/region találat (`mouseFocus`, `resizeRegion`) csak `mouseMove`-ban számolódik (`kick_SEViewComponent.cpp:210-235`, `kick_SampleEditorCue.cpp:52-71`, `kick_SEViewComponent.cpp:478-496`), a híd manual útja viszont csak `mouseDown/Drag/Up`-ot küld (`kick_hardware_bridge.h:563-640`). Következmény: a `mouseDown` ágban `resizeRegion == nullptr` → Manual módban **minden koppintás új vágást szúr be** (`kick_SEViewComponent.cpp:276-282`) ahelyett, hogy a meglévő fogantyút megfogná. Ugyanaz az osztály, mint a Grid „Magnify halott” hiba (04-screen-audit §3). Feltétel: a MPC valóban a manual utat használja (RPT:199-211, `delivery=manual`). Repro: Chop/Manual → ujjal egy meglévő slice-határra húzni.

**B6 (P1) BPM-detektálás nincs; beat-skála hamis.** `detectBpm` az egész detektor-kódot kikommentezve hagyta, `numBeats == 0 → 65` (`ENG/mt_sample_editor_interface.cpp:467-473`). Hatás: a „Sample BPM” mező és a Beats idő-skála minden mintára 65 beat; a `Tune`/`From BPM` funkciók értelmetlenek.

**B7 (P1) Szerkesztés után a motor régi hangot használ + 5 s korlát.** Nincs `ASound` újratöltés/érvénytelenítés a szerkesztés után (`sampleModified` csak az undo-ágon sül el: `ENG/mt_sample_editor_interface.cpp:72-85`; `ASound::reload()`-ot senki nem hívja az engine-ben). Emellett a Samples-asset gyorsítótára **5 s-ra vágja** a mintát (`M/engine/audio_file_manager/source/mt_audio_asset_manager.cpp:137, 206, 334` → `audio_sound.cpp:86-91`) — a szeletek `loopEnd`-je is ehhez clampelődik (`audio_sound.cpp:253-275`). Hosszabb minta (pl. 20 s Sampler-felvétel) csak az első 5 s-ban szólal meg. Futtatással nem igazoltam → **MPC-n/hosztón ellenőrizendő**.

**B8 (P1) Inicializálatlan `selectedTool`.** `SeTool selectedTool;` nincs alapértékkel (`kick_SampleEditorData.h:533`, a konstruktor sem állítja: `Data.cpp:530-554`), pedig `isSplitModeManual()` ezzel dönt (`:1093-1096`). A heap nullázottsága véletlenen múlik. A `setTool` hívója nincs (Erase/Listen/Zoom elérhetetlen).

**B9 (P1) Mintaváltás törli a teljes projekt undo-történetét.** `MTSampleEditorData::setSource` → `getAudioInterface().getUndoManager().clearUndoHistory()` (`ENG/mt_sample_editor_interface.cpp:361-369`). A Sampler minden mentéskor betölti a mintát a szerkesztőbe (`kick_Sampler.cpp:229, 297`) → a felvétel mentése elveszíti a sorozat/program szerkesztések undo-ját.

**B10 (P1) Szerkesztés biztonsága.** (i) Nem-WAV forrás (flac/aiff/ogg/mp3) szerkesztésekor WAV-bájtokat írunk az eredeti kiterjesztéssel (`mt_sample_editor_actions.cpp:244-261, 344-349`) — a JUCE a kiterjesztés szerint választ olvasót, a fájl olvashatatlanná válhat (feltételes; futtatással nem igazolt). (ii) A csere nem atomi: `sourceFile.deleteFile()` majd `moveFileTo` (`:344-349`); összeomlás között a minta elveszik. (iii) Az `undo_sampleeditor.wav` és `_temp.wav` fájlok a minta mappájában maradnak összeomláskor, a visszavonás-fájl korlátlanul nő (`undoManager.setMaxNumberOfStoredUnits` ki van kommentezve: `mt_sample_editor_interface.cpp:17-18`). (iv) A gyári/megosztott mintát is helyben írja felül, minden olyan program is azt kapja, amelyik hivatkozik rá.

**B11 (P2) Egyéb.**
 - Normalize: `maxLevel == 0` esetén osztás nullával (`mt_sample_editor_actions.cpp:429`); nincs célszint; a teljes tartomány egy bufferben (`:437-449`, `int numSamples`), hosszú mintán nagy memóriafoglalás ARMv7-en (a MPC RAM-ja NEM ELLENŐRIZHETŐ KÓDBÓL).
 - Cue-preview „30 ms” valójában `sr*30/60` = 0,5 s mindkét irányban; a pozíció-üzenet „3 ms” valójában 50 ms (`ENG/mt_sample_player.cpp:3-4, 98, 144`).
 - Chop pad felengedésekor `if(!isRunning) playSelectedRegion` újrajátssza a szeletet (`Options.cpp:1076-1079`) — gyors ütésre kétszer szólhat.
 - A feldolgozás az üzenetszálon fut, háttérszál/progress nélkül (`Data.cpp:1110-1121`); a lejátszó fájlolvasása az audio-callbackben (`mt_sample_player.cpp:157, 164`) SD-n xrun-kockázat (NEM ELLENŐRIZHETŐ KÓDBÓL).
 - Settings „Timeline Units” kezdőérték hiba (`kick_SampleEditorSettings.cpp:45-46`).

## 4. Tervben / hiányzik (az Akai MPC Standalone-hoz képest, a docs-ból ismert tervek)

- **A v3.10 Sample Edit/Chop/Sampler fejezet (p. 267–303, „WP-F”) áttekintése NINCS elvégezve** (`docs/akai_mpc_hardware/WALKTHROUGH_PLAN.md:24`; `known-issues` §3.1 „csak WP-D és WP-G készült”). Ezért a teljes Akai-paritási lista **NEM ELLENŐRIZHETŐ a repóból**; első teendő a WP-F (SE-30).
- A meglévő WP-ek ide vágó pontjai: **G-042** Browser „Edit” → Sample Edit a kijelölt mintával: HIÁNYZIK (`WP-G.md:50`); **G-164/165** Cue/Slice Preview: ELTÉR (a Kickben a Sample Editor beállításai közt van, nem a Preferences-ben; `WP-G.md:172-173`); **G-186** Sampler Save ablak: ELTÉR (`WP-G.md:194`). `KEY61_DOC_VS_KICK.md` #20: gombpár „nincs eltérés”.
- A Kick **saját UI-ja** már tartalmazza a tervezett, de be nem kötött funkciókat (ezek a „mit akart a tervező” bizonyítékai): Chop → pad kiosztás (Assign to / Slice type / Crop), Detect BPM / To Seq / Match BPM / From BPM, Semi/Fine/Root note/Loop/X-fade slice-paraméterek (a holt Trim/Program/Layer osztályokban), Pitch Shift / Time Stretch / Bit Reduce.
- Nincs ide vágó tétel a `known-issues` H1–H30 listájában (a Sample Edit nem szerepel a hardveres igazoltak között sem, csak a gombpár).
- Terv 02 (zongora): a multisample zónák/rétegek mintakezelése a Sample Edit-et nem érinti, de a 5 s-os cache-korlát (B7) a zongora-tervet is (14 s-os minták) — egyeztetendő (lásd `research/codebase-survey.md` Memory/streaming).

## 5. Javaslataim (improvementek, indoklással)

1. **Chop → motor „egy igazságforrás”.** A szeletek legyenek az asset-szeletek (`ASoundSlice`) közvetlen nézetei (a `Regions` fa helyett vagy azt tükrözve), így az Assign, a Program Edit és a Sampler ugyanazt látja; a Sampler már így működik (`kick_Sampler.cpp:327-335`). Indok: ma két párhuzamos szelet-modell van, a szerkesztő nem tud pad-ot kiosztani.
2. **Offline DSP-réteg** a Pitch/Time/Bit-reduce/Fade/Normalize/Gain/Reverse műveletekhez, blokkos, háttérszálon futó, megszakítható, egységtesztelt (host) — a jelenlegi „teljes régió egy bufferben” modell helyett. A motorban lefordított, de használatlan pitch-shifter kód (`M/engine/objects/sampler/smbPitchShift`, `objects.cpp:36`) kiindulás lehet; a licencet és a minőséget (time-stretch nincs) **ellenőrizni kell**, a végleges könyvtárról owner-döntés kell.
3. **Nem-destruktív alapmodell**: szerkesztés előtt másolat a projekt `samples/` mappájába (a projekt-mentő már ide másol: `M/engine/project/kick_xml_manager.cpp:112-130`), atomi csere (temp + rename), WAV-konverzió nem-WAV forrásnál, „Revert to original”. Indok: B10 (megosztott/gyári minta, nem-atomi csere, undo-fájl-nő).
4. **Érintés-első kezelés**: hit-test `mouseDown`-ban (nem `mouseMove`-ban), ≥ 24 px találati zóna (a híd dupla-koppintás slopja 24 px: `kick_hardware_bridge.h:538`), fogantyú-kiemelés érintésre, külön Start/End nagy fogantyúk. Egyujjas környezetben nincs pinch → zoom: +/− gomb, **+/− hardver gomb**, Q-Link.
5. **Key 61-re szabott 4 Q-Link kiosztás** Chop-ban: Q1 = Start, Q2 = End, Q3 = Cue/zoom, Q4 = finomság (1/10/100/1000 váltó Shift-tel); Data Dial = a kijelölt fogantyú finom léptetése; a Q-Link 5–10 hivatkozások törlése vagy áttérés Shift-rétegre.
6. **Visszajelzés**: a `performAction` bool-ját jelenítsd meg (toast: „Kész / Hiba: ok”), hosszú műveletnél progress; az eredmény-fájl automatikus asset-regisztrációja és kiválasztása.
7. **Félrevezető vezérlők kezelése**: amíg egy funkció nincs kész, ne legyen aktív (szürke + ok) — kivéve ha a TODO-ban az elkészítése szerepel ugyanebben a körben (SE-01…SE-10).
8. **Holt kód**: a ~2000 soros legacy (Layer/Trim/Program/ProcessAction/Assign/SaveSampleDialog) vagy bekötendő (a Trim/Program oldal a hiányzó Start/End/Loop/X-fade/Root/Tune funkcióinak forrása), vagy törlendő — döntés után (SE-22).
9. **Tesztelhetőség**: a vágás/zero-snap/threshold/régió-matek és minden Process-művelet legyen JUCE-GUI-mentes függvény → host-teszt szintetikus WAV-okkal (a `Kick/tests/platform` mintájára).

## 6. Hardveres és unit-tesztek

**Ma fedi:** semmi nem fedi a Sample Editort. `Kick/tests/platform` 33 fájlja közül csak az `AudioTests.cpp` érinti a rögzítőt (nem a szerkesztőt); `grep SampleEditor|MTSampleEditor` a teszt-mappában 0 találat. Hardveren csak a gombpár nyitása igazolt (RPT:98-99, 234; LED-szín bizonytalan: RPT:116). Képernyőkép-smoke: a `KICK_SMOKE_SHOTS_DIR` névlistában szerepel `MenuSampleEdit` (`docs/AKAI_MPC_KEY61_TODO.md:82`), tartalmi ellenőrzés nélkül.

**Hiányzik:** a Process-műveletek (szintetikus WAV: reverse, normalize, gain, silence, fade, delete/discard hossz, stereo→mono), `splitByThreshold`, zero-snap, `Regions` matek, paraméter-lista-összeállítás (`ProcessSlice`), undo/redo fájl-konzisztencia (több lépés, mintaváltás), nem-WAV forrás.

**A MPC-n ellenőrzendők:**
- [ ] SAMPLE EDIT / Shift+SAMPLE EDIT ismételt nyomás (képernyő-toggle), LED-szín (RPT:116 bizonytalan).
- [ ] Hullámforma + zoom +/− érintéssel; hosszú (60 s+) minta megnyitási ideje és repaint-folyamatossága (a nézet 100 ms-os timerrel újrafest: `kick_SEViewComponent.cpp:192`).
- [ ] Fogantyúk/cue húzása ujjal (B5); Manual vágás koppintással; kétszeri koppintás szerepe.
- [ ] Q-Link 1–4 Start léptetés; Q 5–10 hatástalansága; Data Dial és +/- a fókuszált számmezőn.
- [ ] Pad 1–8 előhallgatás Tune módban; pad-szelet Chop-ban, élő vágás pad-dal; pad-felengedés újrajátszás (B11).
- [ ] Process: mind a 14 művelet hallgatással (különösen Fade — várhatóan nincs hatás), Extract/Copy/Stereo>Mono után a mappában maradó `_temp` fájlok, új asset megjelenése.
- [ ] Undo/Redo a hardver UNDO gombbal (minta + régió); Redo = Shift+UNDO.
- [ ] Szerkesztés után a programban a pad **az új hangot** adja-e (B7), >5 s minta teljes hosszban szól-e.
- [ ] Előhallgatás xrun nélkül SD-n; előhallgatás + szekvencia egyszerre.

## 7. TODO

Hibák (sorrendben):
- [ ] **SE-01** (P0, S) Fade In/Out tényleges görbe-alkalmazás (Linear/Log/Exp a `params`-ból) — `ENG/mt_sample_editor_actions.cpp:451-505`; teszt: SE-26.
- [ ] **SE-02** (P0, S) Extract/Copy/Stereo>Mono paraméter-összeállítás (a név mindig kerüljön a `params`-ba; az engine ellenőrizze a `params.empty()`-t és adjon hibát) — `kick_SampleEditor.cpp:409-418, 468-473, 517-531`; `mt_sample_editor_actions.cpp:636, 732, 751, 762`.
- [ ] **SE-03** (P0, M) `noProcessing` ág takarítása (reader/writer felszabadítás, `_temp` ne keletkezzen) + az eredmény-fájl automatikus `addAsset` + lista-frissítés + kiválasztás — `mt_sample_editor_actions.cpp:244-261, 375-379`; `Data.cpp:1110-1121`.
- [ ] **SE-04** (P0, S) Stereo>Mono „Sum” ×0,5 (vagy limit) — `mt_sample_editor_actions.cpp:783`.
- [ ] **SE-05** (P0, L) Chop-szeletek → asset-szeletek szinkron + Assign „Slice to pad / Make new sample / Crop / Slice type” bekötése, minden program-típusra vagy kifejezett hibaüzenettel — `kick_SampleEditor.cpp:138-155`; minta: `kick_Sampler.cpp:327-335`. Függ: SE-11.
- [ ] **SE-06** (P0, L) Pitch Shift, Time Stretch, Bit Reduce implementáció (offline, háttérszál, undo) — `mt_sample_editor_interface.cpp:207-218`. **Függ: owner-döntés a DSP-könyvtárról/licencről** (lásd §5/2).
- [ ] **SE-07** (P1, M) Érintés-hit-test `mouseDown`-ban (cue, loop, region-fogantyú), nagyobb találati zóna; a Manual-vágás ne indítson fogantyú fölött — `kick_SEViewComponent.cpp:210-308`, `kick_SampleEditorCue.cpp:52-71`; hasonló: Grid Magnify (04 §3).
- [ ] **SE-08** (P1, S) `selectedTool` inicializálása + eszköz-választó UI, vagy az Erase/Listen/Zoom eszközök kivezetése — `kick_SampleEditorData.h:438, 533`.
- [ ] **SE-09** (P1, M) Valódi BPM/beat-detektálás (vagy a BPM-mezők és a „Beats” skála eltávolítása) — `mt_sample_editor_interface.cpp:422-473`; `kick_SEScaleComponent.cpp:96, 196`.
- [ ] **SE-10** (P1, M) Tune fül bekötése: Detect BPM, To Seq, Match BPM, From BPM, Semi, Fine (slice-paraméterekre) — `Options.cpp:1125-1180`, `FromBPMMatch` (`:738-770`). Függ: SE-09, SE-05.
- [ ] **SE-11** (P1, M) Szerkesztés utáni cache-érvénytelenítés: `ASound` újratöltés, a programok/szeletek `loopStart/End` újraszámítása (Delete/Discard után) — `M/engine/audio_file_manager/source/audio_sound.cpp:63-66, 253-275`; `Data.cpp:1110`.
- [ ] **SE-12** (P1, M) A Samples-asset 5 s-os cache-korlát felülvizsgálata (streaming vagy kapcsolható korlát) — `mt_audio_asset_manager.cpp:137, 206, 334`. **Owner-döntés: memória-keret ARMv7-en.** Közös tétel: SA-09, LO-09 export.
- [ ] **SE-13** (P1, S) A mintaváltás ne törölje a projekt undo-ját (külön undo-szakasz/saját UndoManager a minta-műveletekhez) — `mt_sample_editor_interface.cpp:361-369`. A Sampler felől: SA-14.
- [ ] **SE-14** (P1, M) Szerkesztés-biztonság: atomi csere (temp + rename), nem-WAV forrás kezelése (konvertálás + új asset vagy tiltás), maradék `_temp`/undo-fájlok takarítása induláskor, undo-fájl méretkorlát — `mt_sample_editor_actions.cpp:15-25, 244-261, 336-349`; `mt_sample_editor_interface.cpp:17-18`.
- [ ] **SE-15** (P2, S) Normalize: `maxLevel == 0` védelem + célszint; Gain: vágás-figyelmeztetés — `mt_sample_editor_actions.cpp:429`.
- [ ] **SE-16** (P2, M) Blokkos feldolgozás + háttérszál + progress + a `performAction` hibájának megjelenítése — `mt_sample_editor_actions.cpp:394-449`; `Data.cpp:1110-1121`.
- [ ] **SE-17** (P2, S) Chop: Repeat és X FADE bekötése vagy kivezetése; `loopSelectedRegion` — `Options.cpp:913-915`; `Data.cpp:232`.
- [ ] **SE-18** (P2, S) Pad-felengedés újrajátszás vizsgálata; cue-preview/broadcast konstansok (`/60` → `/1000`) — `Options.cpp:1076-1079`; `mt_sample_player.cpp:3-4, 98, 144`.
- [ ] **SE-19** (P2, S) Settings: Timeline Units kezdőérték, Cue Play és második Slice Preview bekötése/eltávolítása; „MIN SAMPLES” → „MIN ms” — `kick_SampleEditorSettings.cpp:15, 28, 45-46`; `Options.h`.

Hiányzó funkciók / hardver:
- [ ] **SE-20** (P1, M) Key 61 4-Q-Link kiosztás Chop/Tune alatt (Start/End/Cue-zoom/finomság), Data Dial = fogantyú-léptetés, +/− = zoom; a Q 5–10 hivatkozások kivezetése — `kick_SampleEditor.cpp:189-198`; `Options.cpp:1011-1031`; `kick_hardware_bridge.h:1529-1541`.
- [ ] **SE-21** (P2, S) PLAY/STOP szemantika a Sample Editben (szekvencia vs előhallgatás) — owner-döntés — `kick_SampleEditor.cpp:170-178`.
- [ ] **SE-22** (P2, M) Holt kód: Trim/Program/Layer/ProcessAction/Assign/SaveSampleDialog bekötése (slice-szintű Start/End/Loop/X-fade/Root/Tune) vagy törlése — `kick_ui.cpp:43-53, 88`. Függ: SE-10.
- [ ] **SE-23** (P2, L) Slice-szintű paraméterek (semi/fine/root/loop/xfade) az `ASoundSlice` property-jeibe — `audio_sound.h:70-`; Függ: SE-05, SE-22.
- [ ] **SE-24** (P2, L) Nem-destruktív szerkesztési modell (másolat-alapú, Revert) — §5/3. Függ: SE-14.
- [ ] **SE-25** (P2, S) Eredmény/hiba toast-ok és progress a Process-műveletekhez. Függ: SE-16.

Tesztek:
- [ ] **SE-26** (P1, M) Host-teszt: Process-műveletek szintetikus WAV-on (reverse, normalize, gain, silence, fade, delete/discard hossz, stereo→mono, extract/copy) — új `Kick/tests/platform/SampleEditorActionsTests.cpp` (CMake bekötés). Függ: SE-01…SE-04.
- [ ] **SE-27** (P2, S) Host-teszt: `splitByThreshold`, zero-snap, `Regions` matek, `ProcessSlice` paraméter-lista — a függvények GUI-mentesítése.
- [ ] **SE-28** (P1, M) MPC-s ellenőrző-lista (§6) felvétele a `docs/AKAI_MPC_KEY61_HW_TEST_CHECKLIST.md`-be, a futtatás eredményével.
- [ ] **SE-29** (P2, S) `KICK_SMOKE` képernyőkép-ellenőrzés a Chop/Tune/Process képernyőkre (tartalmi összehasonlítással).
- [ ] **SE-30** (P1, M) WP-F (v3.10 p. 267–303: Sample Edit, Chop, Pad mód, Sampler, Auto Sampler) végigvétele a `walkthrough/WP-F.md`-ben → a SE/SA TODO bővítése Akai-paritással.
