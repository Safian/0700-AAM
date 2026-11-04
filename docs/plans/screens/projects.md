# Projects (RecentProject) — review és TODO

Állapot: **statikus kódolvasás** (build/futtatás/hardver nélkül; az MPC-hez nem csatlakoztam), 2026-11-04, ág `claude/kick-plans`. A „KÉSZ” itt azt jelenti: van bekötött handler az engine-ig, nem azt, hogy hardveren igazolt. Ahol a régi docs és a kód ellentmond, a **kód az igazság**. Rövidítések: `M/` = `Kick/modules/`, `A/` = `Kick/apps/kick/`, `RP/` = `M/kick_ui/recent_project/`, `bridge` = `A/kick_hardware_bridge.h`, `XML` = `M/engine/project/kick_xml_manager.cpp`. Prioritás: **P0** összeomlás / adatvesztés / biztonság, **P1** hibás vagy hiányzó alapfunkció, **P2** javítás / kényelem. Méret: S ≤ fél nap, M ≤ 2 nap, L > 2 nap. Az indítási folyamat (splash, bridge-override, autoload) közös részét a `04-screen-audit.md` §1 és a `screen-inventory.md` §3 írja le; itt csak a Projects képernyőhöz tartozó részt tárgyalom.

## 1. Áttekintés

**Mit csinál.** Az alkalmazás első képernyője és a projekt-váltó (`RP/kick_RecentProject.{h,cpp}`, 743 + 181 sor): három vízszintesen görgethető sor — **DEMOS** (`/data/demos`, számozás szerint rendezve), **TEMPLATES** (`/data/templates`), **RECENT** (a legutóbbi projektek fordított sorrendben) + jobbra a „New project” gomb; 200 px széles csempék (PNG-bélyegkép vagy mappaikon + dátum / idő). Fejléc: File Browser gomb, **Auto Load** kapcsoló, Back (csak ha már van betöltött projekt). Betöltéskor a `LoadProjectAndAnimation` szinglton (`RP/kick_RecentProject_Utils.cpp`) munkaszálon hívja a motort, közben a `LoadAnimation` (13 PNG-kocka) fut, siker után 200 ms-mal `HomeListView`-ra vált.

**Elérés.**
- Az app indulásakor ez az első képernyő (`kick_AppHome.cpp:262, 281`; a többi képernyő lustán készül: `:284-315`).
- Felső sáv „New File” gomb (`M/kick_ui/dialogs/kick_AppTopbar.cpp:97`) — a felső sáv csak a HomeListView-n és a Menüben látszik.
- MPC Key 61: **nincs dedikált hardveres gomb**. Az indítás után ~1 s-mal (30. képkocka) a híd kényszeríti a HomeListView-t, és ha van `/data/demos/Demo 01/Demo 01.kickPrj`, szinkron betölti (`bridge:671-681`); ezért az MPC-n a Projects képernyő az induláskor 1 s-ig látszik, később csak a felső sávon át érhető el.

**Hardver-kötések a képernyőn.**
| Vezérlő | Mit tesz itt | Kód | Hardveres állapot |
|---|---|---|---|
| Data Dial forgatás | a csempe-kijelölés léptetése a három sor között (demos → templates → recent, egy lineáris index), a recent sor görgetése | `RP/kick_RecentProject.cpp:383-423` | nem mért; **csak akkor jut el, ha nincs betöltött projekt** (B4) |
| Data Dial nyomás / ENTER | a kijelölt csempe „onMouseDown”-ja — de `juce::File()`-lel (B3) | `:376-381` | nem mért |
| Auto Load (érintés) | megerősítő popup, `settings.projects.isAutoLoad` | `:55-74` | nem mért |
| Hosszú nyomás a recent csempén | Load / Remove popup | `:320-333`, `LongPressComp` `:695-742` | nem mért |
| Bank, Play/Stop, Q-Link | nincs kötés | — | — |
Érintés: egy koppintás a **dupla koppintás várakozási idő + 5 ms** után tölt (`RecentItems::mouseUp`, `:661-664`, ~405 ms késés), 500 ms hosszú nyomás popupot nyit (`:628-689`). A modális popupok (Auto Load, Remove) hardverrel nem zárhatók (`bridge:893-905` csak a TC ablaknak ad HW-zárást).

**Betöltési út.** `startLoading(file)` → `isProjectLoading = true`, munkaszál: `ae.loadProject(file)` → `closeProject()` (a jelenlegi projekt **automatikus mentése** `XML:321-328`) → `loadOrCreate` (`XML:186-215`: létező fájl esetén XML-olvasás, hiányzó esetén *létrehozás*; a fájlnév a mappa nevéből `<mappa>/<mappa>.kickPrj`) → asset/program/szekvencia felépítés, **fix `sleep_for` 500 ms** + a `closeProject`-ben 100 + 500 ms (`M/engine/audio_engine/engine/mt_audio_manager.cpp:240, 317, 323`) → `onProjectLoaded` → `RecentProject::onProjectLoaded` a recent listába teszi (`rearrangeProject`) és `currentProject`-et állít. A Demos és Templates könyvtár **csak olvasható** a mentésnél (`XML:243-250, 270-271`).

## 2. Funkciók és állapotuk

| Funkció | Állapot | Bizonyíték |
|---|---|---|
| Demos / Templates / Recent csempesorok, bélyegkép, görgetés | RÉSZLEGES | `RP/kick_RecentProject.cpp:228-354, 464-514`; a bélyegkép a **téma** `recent_project/<név>.png`-jét tölti (`:588`, `Kick/assets/*/recent_project/`), user projektnek nincs; a demo/sablon sor csak a konstruktorban olvasódik be (`:15-16`) |
| Recent projekt betöltése (koppintás) | RÉSZLEGES | `:339-348`; ~405 ms késés, nincs hibakezelés (B5) |
| Demo betöltése | RÉSZLEGES | `:253-256`; a `enableRecentProjVTreeEntry` globális jelző (B6) |
| Sablonból új projekt | **BUGOS** | `:158-170, 292-294` — felülírás, hibás index, üres projektmappa (B2) |
| New project (virtuális billentyűzet) | RÉSZLEGES | `:91-104, 107-130`; üres név, lemezen létező név nem ellenőrzött (B7) |
| Hosszú nyomás: Load / Remove | RÉSZLEGES | `:23-53`; Remove csak a listából töröl; a jelenlegi projekt védett (`:39`); beégetett pozíció (`+432`, `:323-328`) |
| Auto Load kapcsoló + indítási betöltés | RÉSZLEGES | `:55-74, 441-456`; MPC-n a bridge felülírja (B8) |
| File Browser gomb | KÉSZ | `:76-78` |
| Data Dial / ENTER | **BUGOS** | `:365-381` (B3, B4) |
| Betöltő animáció | KÉSZ | `RP/kick_RecentProject_Utils.cpp:20-26`; `kick_utils_loadingAnimation.cpp` (`screen-inventory.md` §3.1) |
| Betöltési hiba visszajelzés | **HIÁNYZIK** | `Utils.cpp:42-47`: hiba esetén csak `stopAnimation()` |
| Projekt átnevezése / törlése / duplikálása | HIÁNYZIK (csak a FB Select módjában: rename / delete) | `FB/kick_fileBrowser.cpp:975-1056, 1352-1419` |
| Save As / Save Copy / Save as Template | **HIÁNYZIK** | az engine hibaüzenete hivatkozik rá (`XML:271`), de nincs ilyen függvény vagy UI (`grep -i saveas` a `Kick/modules`-ban csak ezt találja) |
| Mentetlen módosítások kérdése váltáskor | NINCS (csendes automentés) | `XML:321-328`; WP-G G-246 |
| Recent lista karbantartás (max, eltűnt projektek) | RÉSZLEGES | `M/engine/project/kick_settings.cpp:127-134, 167-224`; a nem létező útvonal csempéje üres és nem is töltődik (`RP/kick_RecentProject.cpp:572`: `if( path.isDirectory() )`) |

## 3. Mi nem működik / hibák

**B1 — `LoadProjectAndAnimation::startLoading` nem újrabelépés-biztos, P0.** A függvény sorban `isProjectLoading = true`, `proj = project`, `startThread()`, `startAnimation()` (`RP/kick_RecentProject_Utils.cpp:20-26`) — őr nélkül. A hívók többsége nem védekezik: a FB dupla koppintása (`FB/kick_fileBrowser.cpp:636-642`), a Projects csempék (`:339-348`), az autoload (`:446-455`), a hosszú-nyomás „Load” (`:23-26`). Futó szálon a `startThread()` nem csinál semmit, miközben a `proj` mezőt az UI szál felülírja, a worker pedig olvassa → vagy a *régi*, vagy az *új* projekt töltődik, a `isProjectLoading` pedig az első befejezéskor `false`-ra áll (`:46-47`), miközben a második betöltés még fut. Reprodukció: gyors dupla koppintás két különböző csempére (a ~405 ms várakozás ezt részben elfedi), vagy a Demos / Recent csempe koppintása autoload közben. A bridge saját, szinkron betöltése (`bridge:671-681`) mindezt megkerüli (lásd B8).

**B2 — Sablonból új projekt: felülírás és hibás index, P0.** `copyProjIntoProjectDir` (`RP/kick_RecentProject.cpp:158-170`): (a) a cél `projects/<név>/<név>.kickPrj` meglétét **nem** ellenőrzi → a `copyFileTo` felülír (JUCE: a cél törlődik) egy meglévő projektet egy üres sablonnal — adatvesztés. A duplikátum-ellenőrzés csak a `createProject`-ben van, és az is csak a *recent lista* neveivel hasonlít (`:109-124`), nem a lemezzel. (b) `sourceFiles[0]` ellenőrzés nélkül (sablon `.kickPrj` nélkül → üres `File` → a másolás csendben meghiúsul), (c) a `projDir.createDirectory()` a másolás *előtt* fut, hiba esetén **üres projektmappa** marad és a `loadProject` egy új, üres projektet nyit meg ezen a néven. (d) Üres név: `text == ""` → `projects/.kickPrj`-szerű útvonal. A sablonból csak a `.kickPrj` másolódik (nem a mappa): a mellékelt sablonok csak `.kickPrj` + `.png` (`Kick/templates/Template 0N/`), tehát jelenleg működik, minta-hordozó sablonnál nem.

**B3 — Data Dial nyomás a demo / sablon csempén üres útvonallal hív, P1.** `knobClicked()` az `onMouseDown( selectedIndex, juce::File() )`-t hívja (`RP/kick_RecentProject.cpp:376-381`; ugyanígy a `LongPressComp::onLoadTriggered`, `:23-26`). A *projektsor* lambdája az indexet használja (`:339-348`), de a **demo** (`:253-256`: `loadProject( project )`) és a **sablon** lambda (`:292-294`: `openVirtualKeyBoard( project )`) a paramétert használja → `loadProject( File() )`: `loadOrCreate(File())` → a `createDirectory` üres útvonalon várhatóan hibát ad → „Cannot create project directory” mentési-hiba popup (`XML:200`, `kick_AppHome.cpp:320-325`; kódból következtetett, nem futtattam); a sablonnál a billentyűzet üres névvel nyílik és a másolás a B2 szerint üres projektet hoz létre. Hatás: a Data Dial nyomás **csak a recent csempéken működik**. (A csempe saját koppintása a valódi útvonalat adja: `RecentItems::timerCallback`, `:680-686`.)

**B4 — A Data Dial a Projects képernyőig el sem jut, ha van betöltött projekt, P1 (MPC-n ellenőrizendő).** A `KickHome::handleCommand` a kezelőt csak `prjLoaded && !isProjectLoading` esetén engedi tovább (`kick_AppHome.cpp:487`), és ott a képernyő `handleCommand`-ját a `requiresHandleCommand()` védi (`:580`); a `RecentProject` ezt **nem írja felül** (az alap `false`: `M/kick_ui_common/source/kick_IScreen.h:104`, a `RecentProject.h:120-135` nem tartalmazza). A harmadik ág (`:587-591`) csak a *projekt nélküli* eset. Az MPC-n a híd az indításkor mindig betölti a Demo 01-et, így a Projects képernyő (felső sáv „New File” gombbal elérve) mindig „projekt betöltve” állapotban nyílik → a Data Dial nem lép és nem tölt. Kivétel: ha a `getComponentInFocus()` (utoljára megérintett vezérlő) kezeli (`:571-575`) — a csempék nem `KickControlsProcessor`-ok, tehát valószínűleg nem. Javítás: `requiresHandleCommand() { return true; }`. Reprodukció: MPC-n Projects megnyitása (felső sáv), Data Dial forgatás.

**B5 — Betöltési hibák csendben elnyelődnek, P1.** `run()` hiba esetén `stopAnimation()`-t hív, popup / státusz nélkül (`Utils.cpp:42-47`); az engine `loadProject` hamisat ad *bezárási* mentési hibánál (pl. SD-ről betöltött, közben kihúzott projekt: `mt_audio_manager.cpp:202-207`), érvénytelen XML-nél (`XML:198`), vagy ha a mappa nem hozható létre. A felhasználó egy villanást lát. A „Cannot save…”-jellegű hibák közül csak a mentésieket jelzi a `KickHome::onProjectSaveFailed` (`kick_AppHome.cpp:320-325`). Törölt / eltűnt projekt a recent listában: üres csempe, koppintásra `loadOrCreate` **újra létrehozza** az üres projektet a hiányzó mappában (`XML:199-204`: nem létező fájl esetén `createDirectory` + új XML) — a törölt projekt „feltámad” üresen.

**B6 — `enableRecentProjVTreeEntry` globális jelző, P1.** A demo-csempe `enableRecentProjVTreeEntry = false`-t állít (`:254`), a FB Demos-sorából betöltés ugyanígy (`FB/kick_fileBrowser.cpp:639-640`); az `onProjectLoaded` fogyasztja el (`:135-141`). Ha a demo betöltése **meghiúsul**, a jelző `false` marad, és a *következő sikeres, tetszőleges* betöltés nem kerül a recent listába és a `currentProject` sem frissül (ezzel az autoload a régi projektre mutat). Fordítva: az MPC-bridge közvetlen `loadProject( Demo 01 )` hívása (`bridge:677-681`) ezt a jelzőt nem állítja → a Demo 01 **minden indításkor bekerül a recent listába és `currentProject` lesz**, kiszorítva a felhasználó projektjeit (alapérték: 5 bejegyzés, `kick_settings.cpp:100`).

**B7 — New project: ellenőrzés a lemez helyett a recent listán, P1.** `createProject` (`RP/kick_RecentProject.cpp:107-130`): a névütközés csak a recent bejegyzések fájlnevével hasonlít (`:109-124`); egy lemezen létező, de már kiesett (>5.) projekt néven „új” projektként *megnyitja a meglévőt* (`loadOrCreate`). Üres név megengedett: a virtuális billentyűzet csak vágja a szóközöket (`M/kick_ui_controls/source/kick_VirtualKeyboard.cpp:597-601`); `getAppDirs(MTProjects) + "/" + ""` = a projektek gyökere → `projects.kickPrj` az `/data/projects` mappában (és `Samples` / `Export` mappa a gyökérben: `kick_settings.cpp:159-165`). Érvénytelen karakterek (`/`, `..`) nincsenek kiszűrve. A névelőtag (`settings.projects.namePrefix`) a beviteli mező kezdőszövege; a 04-screen-audit §8.2 szerint az alapértelmezett érték fogyasztója részleges.

**B8 — Indítási betöltések versengése MPC-n, P1.** (Részletes leírás: `screen-inventory.md` §3.2, `04-screen-audit.md` §1.) A bridge 30. képkockán kényszerített képernyőváltás és szinkron `loadProject` az üzenetszálon (animáció és `isProjectLoading` nélkül), párhuzamosan az autoload-dal (`RP/kick_RecentProject.cpp:441-456`) → két `loadProject` is futhat egyszerre; az engine-nek nincs újrabelépés-védelme a `closeProject()` mutex-én túl (`mt_audio_manager.cpp:196-208`). Új szempont: a bridge-betöltés a `onProjectLoaded`-on át a Demo 01-et recent / currentProject-be teszi (B6), tehát az autoload *soha nem tölti vissza* a felhasználó utolsó projektjét az MPC-n. A **Projects képernyő MPC-n ~1 s-ig látszik indításkor**, így a Demos / Templates / Recent választás nem használható az indító képernyőként.

**B9 — Csempe-késleltetés és halott dupla-koppintás jelző, P2.** Minden koppintás `doubleClickTimeout + 5 ms`-ot vár (`:661-664`), a `isDoubleClicked` mezőt a `mouseDoubleClick` állítja, de sehol nem olvassa (`:667-669, 686`): egyetlen koppintás sem tölt azonnal. Az MPC-n az érintés „dupla koppintás” küszöbe 450 ms (`bridge`), a JUCE alap 400 ms → a tényleges késés ~405 ms vagy több. A hosszú nyomás időzítője (500 ms) és a dupla-koppintás időzítője ugyanazt a `Timer`-t használja; az ágak a `getTimerInterval() == longPressThreshold` összehasonlítással különülnek el (`:674`), tehát egy 495 ms-es dupla-koppintás küszöbnél összeolvadnának.

**B10 — Élettartam-hibák a csempéknél, P2.** `RecentItems::timerCallback` `callAfterDelay( 20, [&]{ onMouseDown( index, path ); } )`-t hív (`:680-686`) — a lambda a csempére hivatkozik; ha a `setNumOptions` (minden `visibilityChanged`-nél: `:215-226, 433`) a csempét a 20 ms alatt megsemmisíti (`recentItems.erase`), használat-megsemmisülés utáni hozzáférés. A `setNumOptions` sorrendje: `recentItems.erase` (a komponensek megszűnnek) *majd* `projectList.clear()` — közben a `listOptions` lógó mutatókat tart (`:215-226`). Kicsi időablak, de ARM-en reális.

**B11 — Hosszú-nyomás popup beégetett pozíció, P2.** `b.setY( b.getY() + 432 )` stb. (`:323-328`, „TODO - Needs to be changed once finalist”) a 1024×600-as elrendezésre; a mobil T-layout (852×393) és a skálázott nézetekben rossz helyen jelenik meg.

**B12 — Auto Load kapcsoló kérdése „mégse” nélkül, P2.** A kapcsoló egy „Automatically load last opened project on startup?” popupot nyit; bármilyen nem-APPLY lezárás (X, Cancel) **kikapcsolja** az autoloadot (`:55-74`). A kikapcsolás is ugyanezt a kérdést teszi fel. Preferences → AUTO SAVE / ON LAUNCH ugyanezt a beállítást másképp kezeli (`04-screen-audit.md` §8.2: az AUTO SAVE konstruktor-felülírás hibája).

**B13 — Bélyegkép-logika, P2.** `setProperties` (`:567-595`): ha a projektmappa gyökerében van *bármilyen* `.png`, a csempe képmódba vált, és a képet a **téma** `recent_project/<png-név>` útvonalán keresi (`:588`; `KickImage::setImage` → `Theme::getDrawable`, `M/kick_ui_controls/source/kick_image.cpp:50-56`) — user-projektnél üres kép, a felirat a png neve. Felhasználói projektnek nincs pillanatkép-generálás.

## 4. Tervben / hiányzik (Akai MPC Standalone-hoz képest)

Forrás: `WP-G.md` (G-001 – G-009 Menu, G-033 – G-043 Project ablak, G-154 – G-157 New Project, G-174 – G-203 Save ablak, G-246), `known-issues-and-roadmap.md`, `04-screen-audit.md`.
- **Save ablak és Save As** (G-174 – G-186): Project / Project As, Track, Drum Program, Sample, Track as MIDI / Pattern (G-188), Audio Mixdown (G-190, G-191); mentési hely választó (Internal, MPC Documents, USB, SD: G-197), File Name mező (G-201), **Save as Template** jelölő (G-202). A Kickben csak a hardveres SAVE gomb mentheti a *meglévő* helyre (`kick_AppHome.cpp:544-546`) — **demo / sablon szerkesztése nem menthető** (`XML:270-271`, a hibaüzenet „Please use Save As…”-ra hivatkozik, ami nincs).
- **New Project viselkedés** (G-154 – G-157): Empty / Factory / User Auto Load; New Project Dialog módja (Off / Demo / Recent / Demo+Template+Recent); User Template és User Auto Load File kiválasztása. A Kick Preferences ON LAUNCH listája (Selector / New Project / Last Used) **fogyasztó nélkül** (`04-screen-audit.md` §8.2).
- **Project ablak** (G-033 – G-043): a projekt mintái és sávjai, Purge, Memory and Streaming, Disk Streaming (G-035 – G-038) — nincs.
- **Betöltéskor kérdés a jelenlegi projekt bezárásáról** (G-246): a Kick csendben ment és bezár.
- **Indítóképernyő** (`04-screen-audit.md` §1): nincs splash / engine-betöltés kijelzés; a hardver- / hang-init hibák láthatatlanok; közös indítási szolgáltatás (autoload + Demo 01 + bridge) terve még nincs.
- **Recent lista**: az Akai-ban *Project Defaults* (Recent Projects száma) a Preferences-ben; a Kick `maxRecentProjects` (alapérték 5) elérhető a Preferences-ben (`04-screen-audit.md` §8.2: KÉSZ).
- **Cloud / hálózati projektek**: nincs; a `cloud-sources.md` csak mintaforrásról szól. Ha a FB `FileBrowserSource`-t kap (FB-26), a Projects „Cloud” sora külön döntés.

## 5. Javaslataim

1. **Egyetlen betöltési belépési pont.** `ProjectLoader::request( path, source )` (source = User / Demo / Template / Auto / Bridge): egyetlen sorbaállítás-védelem (ha fut, a kérés elutasítva vagy sorba téve), a `enableRecentProjVTreeEntry` globális jelző helyett a *forrás* dönt a recentbe vételről, egy helyen jelenik meg a hibaüzenet (ok: „invalid XML”, „cannot create directory”, „save of previous project failed”). A bridge és az autoload is ezen át menjen, így a B1, B5, B6, B8 egy helyen oldódik. Indoklás: ma 5 különböző hívási út van `startLoading` / `loadProject` felé.
2. **Projektfájl-ellenőrzés a betöltés előtt.** A hiányzó / törölt projektet ne „támassza fel” a `loadOrCreate`; jelezze („Project not found — remove from list?”), és a recent lista szűrje ki az eltűnt útvonalakat.
3. **Save As / Save Copy / Save as Template** az engine-ben (`projectManager::saveAs` — a mostani `rename()` törött: lokális `dir`/`project` üres `File`-ok, `M/engine/project/project.cpp:46-59`), UI a Menüben / a Projects fejlécében (MENU-07). Indoklás: demo / sablon szerkesztése most mentés nélkül elvész.
4. **Projekt-műveletek a Projects képernyőn**: hosszú nyomás popup bővítése (Rename, Duplicate, Delete, Info), a FB Select módjától függetlenül.
5. **Betöltési idő**: a három beégetett `sleep_for` (500 + 100 + 500 ms) eltávolítása / eseményre cserélése (`mt_audio_manager.cpp:240, 317, 323`), valódi folyamatjelző a 13-kockás animáció mellett. Indoklás: MPC-n minden projektváltás > 1 s várakozást tartalmaz.
6. **HW-ergonómia**: Data Dial a csempéken (B3, B4 javítása után) + Shift = sor váltása (Demos / Templates / Recent), hosszú nyomás helyett Shift+ENTER = popup; az Auto Load és Remove modális ablakok HW-zárása (`modal_dialog_keys.h` minta).
7. **Bélyegkép**: a projekt mentésekor kis pillanatkép (a képernyőről, a híd `snapshot` parancsának mintájára) a projektmappába; a csempe ezt töltse a projektből, ne a témából.
8. **Auto Load** egyszerű kapcsoló legyen (kérdés nélkül), vagy valódi „Mégse”; a Preferences ON LAUNCH-sal egy helyen kezelve.
9. **Tesztelhetőség**: a recent-lista logika (`KickProjectsDefault`: `checkAlreadyLoadOrNot`, `refreshMaxNumOfRecentProj`, `removeProject`, `renameProject`) tiszta, JUCE-ValueTree-s egység — host-teszt; a sablon-másolás és az új-projekt névvalidáció tmp-könyvtáras teszt.
10. **Cloud**: ha a FB kap `FileBrowserSource`-t (FB-26), a Projects „Open from…” belépő a FB-n át megy — saját felhő-kód a Projects-ben nem kell.

## 6. Hardveres és unit-tesztek

**Ami ma fedi.**
- `AppSmoke.py` (`kick-app-smoke`, `kick-app-startup`): valódi alkalmazás-indítás, CLAP-állapot perzisztencia, projekt-életciklus (PASS: full KICK app) — a Projects képernyő *képernyőképe* is készíthető (`A/Main.cpp:152-198`, `RecentProject`), de csak külön környezeti változóval.
- `M/engine/project/kick_xml_manager.cpp` mentési / zárási útja a smoke-teszten át érintett; külön egységteszt nincs.
- Nincs teszt: recent-lista logika, sablon-másolás, névvalidáció, hosszú-nyomás popup, Data Dial navigáció, bridge indítási betöltés.
- A `known-issues` §1 szerint igazolt: a Piano projekt betöltése a FB-ből nem omlik össze (ASAN Mac + MPC), a projektváltás összeomlás javítva (8ea4af11). A Projects képernyő önálló hardveres igazolása nincs.

**MPC-n ellenőrzendők.**
1. Indítás: a Projects képernyő hány ms-ig látszik, kezelhető-e (érintés); autoload + Demo 01 dupla betöltés (`kick.log` `[PROJ] Project load begin` sorok száma); mi kerül a recent listába és a `currentProject`-be egy hideg indítás után (`/data/prefs/preferences.settingsprj`).
2. Data Dial a csempéken projekt betöltött és nem betöltött állapotban (B3, B4).
3. Sablon → új projekt azonos névvel kétszer (B2); üres név (B7); `/data/projects` tartalma utána.
4. Hosszú nyomás popup helye 1024×600-on (B11), Remove a jelenlegi projekten.
5. Törölt projekt a recent listában (FB-ből törölve, majd Projects megnyitása): üres csempe, koppintás (B5).
6. Betöltési idő nagy projektnél (a 500 + 100 + 500 ms fix várakozás); animáció folyamatossága.
7. Demo betöltése és a SAVE gomb: „UNSAVED: Cannot save over factory demo/template…” állapotsor / popup (nincs Save As).
8. Auto Load popup érintéssel és anélkül (X-re kikapcsol-e).

## 7. TODO

Függőségek: PRJ-05, PRJ-06, PRJ-07 ← PRJ-01 (közös betöltési belépési pont); PRJ-12 ← engine `saveAs`; PRJ-18 ← Settings ON LAUNCH bekötése (`04-screen-audit.md` §8.4/3).

*Hibák*
- [ ] **PRJ-01** (P0, S) `LoadProjectAndAnimation::startLoading`: újrabelépés-védelem (`isProjectLoading` esetén elutasít / sorba tesz), a `proj` mező átadása a szálnak zár alatt; a hívók (FB dupla koppintás, csempék, autoload, long-press Load) közös belépési ponton át — `RP/kick_RecentProject_Utils.cpp:20-48`. Kapcsolódik: FB-15.
- [ ] **PRJ-02** (P0, S) Sablon → projekt: a cél létezésének ellenőrzése a *lemezen* (nincs felülírás), `sourceFiles.isEmpty()` védelem, üres név tiltása, hiba esetén a félkész projektmappa törlése — `RP/kick_RecentProject.cpp:158-170`.
- [ ] **PRJ-03** (P1, S) Data Dial / long-press Load: a csempe saját `path`-ját adja át (`recentItems[selectedIndex]` útvonal), ne `juce::File()`-t — `:376-381, 23-26`.
- [x] **PRJ-04** (P1, S) `requiresHandleCommand() { return true; }` a `RecentProject`-ben (Data Dial / ENTER projekt betöltött állapotban) — `kick_RecentProject.h:120-135`, `kick_AppHome.cpp:580`. MPC-n ellenőrizendő.  
  **R-6:** `requiresHandleCommand() { return true; }`; the Data Dial / ENTER reach the tiles with a project open. MPC check: PRJ-25.
- [x] **PRJ-05** (P1, M) Betöltési hiba popup / státusz (ok-kód: érvénytelen XML, mappa nem hozható létre, előző projekt mentése sikertelen); a hiányzó projekt ne támadjon fel üresen — `Utils.cpp:42-47`, `XML:186-215`, `mt_audio_manager.cpp:196-215`. Függ: PRJ-01.  
  **R-6:** `loadProject(file, createIfMissing)`: a missing project is an error (`getProjectLoadError()`), never created, the open project stays; the load path shows the reason in a dialog and offers to remove a vanished Recent entry (smoke: `kick-projects-smoke`).
- [x] **PRJ-06** (P1, S) `enableRecentProjVTreeEntry` globális jelző kiváltása a betöltés *forrásával* (User / Demo / Template / Bridge), hogy hiba után ne ragadjon be, és a bridge-betöltött Demo 01 ne kerüljön a recent listába / `currentProject`-be — `kick_RecentProject.cpp:3-4, 135-141, 253-256`, `FB:639-640`, `bridge:677-681`. Függ: PRJ-01.  
  **R-6:** `LoadSource` (User / Auto / Demo / Template / Bridge) travels with the request; Recent / last project are updated in `LoadProjectAndAnimation::finished` on the message thread, `enableRecentProjVTreeEntry` is gone.
- [x] **PRJ-07** (P1, M) Indítási betöltés egyesítése: a bridge kényszerített képernyőváltása + szinkron Demo-betöltés és az autoload egyetlen szolgáltatásba, animációval és `isProjectLoading`-gal — `bridge:671-691`, `RP/kick_RecentProject.cpp:441-456`; lásd `04-screen-audit.md` §1. Függ: PRJ-01, PRJ-06.  
  **R-6:** Start-up decision from W2-A (`requestStartupProject`), plus the "New Project" on-launch mode (PRJ-18). MPC check: PRJ-25 item 1.
- [ ] **PRJ-08** (P1, M) New project: üres / érvénytelen karakterű / lemezen létező név tiltása (nem a recent listán); trim, hossz-korlát; közös a FB-vel — `RP/kick_RecentProject.cpp:107-130`. Kapcsolódik: FB-16.
- [x] **PRJ-09** (P2, S) Csempe-késleltetés: azonnali betöltés dupla-koppintás-várakozás nélkül (vagy első koppintás = kijelöl, második = betölt), a halott `isDoubleClicked` mező törlése — `:628-689`.  
  **R-6:** A tap loads at once (20 ms for the border); the dead double-tap state is removed; `isDragging` is reset on every touch (a drag used to disable the tile for good).
- [x] **PRJ-10** (P2, S) Hosszú-nyomás popup relatív pozíció a csempéhez — `:320-333`.  
  **R-6:** Menu next to the tile, clamped inside the screen (S-D + R-6: a column of text buttons).
- [ ] **PRJ-11** (P2, S) Csempe-élettartam: `SafePointer` a `callAfterDelay` lambdában; a `setNumOptions` sorrendje (előbb `projectList.clear()`) — `:215-226, 680-686`.
- [x] **PRJ-12** (P2, S) Auto Load kapcsoló: valódi „Mégse” vagy kérdés nélküli kapcsoló — `:55-74`.  
  **R-6:** Tap on Auto Load opens the ON LAUNCH popup (Projects screen / last project / new project from a template); Cancel changes nothing.
- [x] **PRJ-13** (P2, S) Bélyegkép: ne a témából keressen user projektre; hiányzó kép esetén ikon — `:567-595`.  
  **R-6:** User projects show their own `thumbnail.png` (file image); factory demos / templates keep the theme picture; no picture = icon + date; a missing folder is marked "not found".

*Hiányzó funkciók*
- [x] **PRJ-14** (P1, L) Save As / Save Copy / Save as Template: engine `projectManager::saveAs` (+ a törött `rename()` javítása/eltávolítása), UI a Menüben / Projects fejlécben; demo / sablon szerkesztés menthetővé tétele — `XML:243-271`, `M/engine/project/project.cpp:46-59`; WP-G G-174 – G-186, G-202.  
  **R-6:** `KickXML::saveAs` / `MTAudioInterface::saveProjectAs`: folder copy (background task, progress, cancel) + live state; Save As switches (the copy is reloaded), Save a Copy / Save as Template do not; the broken `rename()` is not used (Rename is `kick_project::rename`). Demo / template: Save opens Save As. Smoke + host tests.
- [x] **PRJ-15** (P1, M) Projekt-műveletek a Projects képernyőn: Rename, Duplicate, Delete, Info (méret, dátum, programok száma) — hosszú-nyomás popup bővítés; FB Select módjától függetlenül.  
  **R-6:** Long press / Shift+ENTER tile menu: Load, Rename, Duplicate, Delete (confirmation, the open project and factory folders are protected), Info, Remove from list; demos: Load / Save a copy / Info; templates: New project / Info. `kick_project_ops.h` host tests.
- [x] **PRJ-16** (P1, M) Mentetlen módosítások kérdése projektváltáskor / New Project előtt (G-246); automentés beállítástól függően — `XML:321-328`.  
  **R-6:** `kick_unsaved.h` gate (Save / Don't save / Cancel) before a switch, New project, load and quit; asks when the engine flag is set and auto save does not cover it (a demo always asks); Don't save = `discardProjectChangesOnClose`. Smoke covers the engine side.
- [x] **PRJ-17** (P2, M) Recent lista: eltűnt útvonalak kiszűrése / jelölése; demo / sablon sor újraolvasása megjelenéskor; üres sor placeholder — `kick_settings.cpp:167-224`, `RP/kick_RecentProject.cpp:15-16`.  
  **R-6:** The three rows are read again each time the screen shows; a vanished folder stays as a marked tile; placeholders for an empty row.
- [x] **PRJ-18** (P2, M) New Project viselkedés és Template / Auto Load File kiválasztás (ON LAUNCH, G-154 – G-157). Függ: Settings ON LAUNCH bekötés.  
  **R-6:** ON LAUNCH wired on the Projects screen (the Settings screen is not touched): `onLaunch` = Selector / Last Used / New Project, `launchTemplate`; naming style / prefix from Project Defaults.
- [x] **PRJ-19** (P2, M) Pillanatkép-generálás mentéskor a projektmappába (+ csempe-betöltés onnan).  
  **R-6:** `project_actions::writeThumbnail` after Save / Save As (400 px PNG next to the project file); the tile loads it. MPC check: the snapshot of a hidden window.

*Javaslatok*
- [x] **PRJ-20** (P2, S) Betöltési idő: a fix `sleep_for` (500 + 100 + 500 ms) kiváltása eseménnyel + valódi folyamatjelző — `mt_audio_manager.cpp:240, 317, 323`.  
  **R-6:** The three fixed sleeps are replaced by `settleAudioThread` (completed audio device callbacks, `AudioDevice::completedCallbacks`). MPC check: load time and no crash on repeated switches (PRJ-25).
- [x] **PRJ-21** (P2, S) HW-ergonómia: Shift+Dial sorváltás, Shift+ENTER popup, Auto Load / Remove ablak HW-zárása (`modal_dialog_keys.h`).  
  **R-6:** Shift+Dial = row switch, Shift+ENTER = tile menu, the menu takes Dial / ENTER / BACK; dialogs close from the hardware through the bridge keys (S-D).

*Tesztek*
- [ ] **PRJ-22** (P1, M) `KickProjectsDefault` recent-logika host-teszt (checkAlreadyLoadOrNot, refreshMax, remove, rename, fordított sorrend). 
- [ ] **PRJ-23** (P1, M) Sablon-másolás / új projekt tmp-könyvtár teszt (név-ütközés, üres név, hiányzó `.kickPrj`, félkész mappa takarítása). Függ: PRJ-02, PRJ-08.
- [x] **PRJ-24** (P2, M) AppSmoke bővítés: hibás / hiányzó projektútvonal betöltése (hibaüzenet), kétszeri gyors betöltés (PRJ-01), Projects képernyőkép (`KICK_SMOKE_SCREENS=RecentProject`).  
  **R-6:** `ProjectsBrowserSmoke.py` (`kick-projects-smoke`): missing project, Save As / copy, Don't save, demo, queued double / quick loads, Recent updates, screenshots (`kick-browser-smoke`).
- [ ] **PRJ-25** (P1, M) MPC hardveres ellenőrzőlista végrehajtása (6. szakasz) — a hardveres session végezze.

## 8. R-6 hardware checklist (what only the MPC can show)
1. Start-up: Projects screen, then the last project / Demo 01 / a new project, per ON LAUNCH (tap Auto Load); `/data/kick.log` shows one `Project load begin` per start.
2. Load time of a big project and ten project switches in a row (the fixed sleeps are gone; `settleAudioThread` waits for the audio callbacks): no crash, no stuck audio.
3. Tile menu: long press and Shift+ENTER; Dial / ENTER / BACK inside the menu; Rename, Duplicate (progress dialog on an SD project), Delete (the open project and demos are refused), Info.
4. Unsaved changes: edit, switch project -> Save / Don't save / X; same on a demo (Save = Save As); same when quitting; auto save on = no question.
5. Save As on the MPC (name keyboard, progress, the copy opens); thumbnail.png appears next to the project and the tile shows it (snapshot of the hidden window).
6. Project on an SD card: pull the card, open the Projects screen: the tile says "not found"; tap offers to remove it.
