# Kick Piano - UI design mockup (munkacím)

Statikus, böngészőben megnyitható design-tanulmány egy zongora-CLAP pluginhoz. **Nincs hangmotor**; a minimális JS
(`ui.js`) csak tooltipet, vezérlő-állapotokat és billentyű/MIDI-demót ad. Külső függőség nincs (rendszerfont, nincs CDN).
Nyisd meg: `index.html` (élő iframe-előnézetek + paletta + állapot-minták).

## Fájlok

| Fájl | Tartalom |
|---|---|
| `index.html` | Összekötő oldal: 5 variáns előnézettel, paletta, tipó/térköz, vezérlő-állapotok |
| `main-dark.html` | 1. Fő nézet, sötét, 960x600 (a demobar-ban 640/960/1280 szélesség váltható) |
| `main-light.html` | 2. Világos téma (itt a "mintakészlet töltése" sáv aktív állapotban látszik) |
| `compact.html` | 3. Kompakt 640x400: fülek (Instrument / Mics / Pedals / Tone / Room) + rövid billentyűzet |
| `loading.html` | 4. Első indítás / minták letöltése (Downloading / Paused / Error állapot a demobar-ból) |
| `about.html` | 5. About / credits: verzió, AGPL-3.0-or-later, mintaattribúció (helyőrzők), "Built with" |
| `style.css`, `ui.js` | Közös stílus (témák CSS-változókkal) és demó-viselkedés |
| `build.py` | A HTML-ek generátora (88 billentyű geometria, SVG-gombívek). Nem kötelező; a HTML kézzel is szerkeszthető |

A sötét/világos váltó és a méretgombok a lap tetején lévő *demobar*-ban vannak (nem a plugin része).
`?embed` paraméter elrejti a demobart (az index előnézetei használják). `Esc` az About-ról visszalép.

## Design-döntések

- **Rokonság a Kickkel**: a paletta a `Kick/assets/Default/theme.xml`-ből jön: grafit háttér (`#151515`-`#363636` skála),
  borostyán akcentus `#FFB608`, cián `#00AAEE` (Kick `SELECTION_1`), zöld/piros a mérőkre. Betű: Myriad Pro (a Kick téma fontja)
  -> rendszer UI fontok; értékek monospace, tabuláris számokkal.
- **Akcentus-szabály**: borostyán = érték / aktív / kiválasztott; cián = bipoláris érték, MIDI, fókusz; piros csak clip/hiba.
- **Mélység**: finom felső fény + belső árnyék (panel, bemélyedt "well", domború kupak), nincs erős gradiens.
- **Térköz**: 4 px rács; panel padding 8/12, gap 8 (>=1100 px-nél 12); vezérlők 28-32 px, gomb 38-60 px (a konténer szélességéből skálázva `cqw`).
- **Rács**: 24 oszlop; felső sor 8/8/8 (Instrument, Mic mix, Pedals), alsó 5/6/4/5/4 (Tone, Velocity, Tuning, Room & release, Comp/EQ).
- **Billentyűzet**: 88 billentyű (A0-C8), C-jelölés, középső C pont; lenyomott = velocity-szín (5 sáv: kék, türkiz, sárga, narancs, piros =
  a Kick pad-LED palettából), pedállal tartott = borostyán keret + alsó csík; sustain-jelző + pedálmélység; MIDI LED, utolsó üzenet, 16 csatornás aktivitás.
- **Állapotok** (minden vezérlőn): normál, hover (borostyán keret), aktív/húzás, billentyűzetes fókusz (2 px cián gyűrű), tiltott (38% opacitás, tooltip magyarázza).
  Tiltott minta: Electric (nincs telepítve), Pedal noise (a mintakészlet nem tartalmazza).
- **Billentyűzet-kezelés**: minden vezérlő `role=slider/radio/tab` + `tabindex`; nyilak (Shift = durva), PgUp/PgDn, Home/End, Enter = alaphelyzet
  (dupla katt is); billentyűzet: nyilak mozgatnak, Space/Enter játszik, PgUp/PgDn oktáv; tooltip hoverre és fókuszra, Esc zárja.
  `prefers-reduced-motion` kikapcsolja az animációkat.
- **Reszponzív 640-1280**: `@container` lekérdezések a plugin saját szélessége szerint. <840 px: márkanév/RAM el, a panelek 2 oszlopra
  törnek és a fő terület görgethető; a tényleges plugin ez alatt a kompakt (fülekes) elrendezésre váltson.
- **Widget-lista**: gomb (normál/primary/ikon/danger), toggle-LED gomb, panel-kapcsoló, gomb-forgató (unipoláris + bipoláris ív), függőleges/vízszintes/mini
  fader (opcionális mérővel), rádió-kártya (hangszer), legördülő, stepper (tuning Hz + cent), velocity-görbe szerkesztő (húzható pont + presetek + élő bejövő velocity),
  fülek, progress sáv (csíkozott), állapotsáv (OK/töltés), billentyűzet, MIDI-aktivitás, tooltip, badge, táblázat (credits).

Megjegyzés a méretről: a `640` gomb a fő nézetnél magasabb ablakot ad (720 px), mert a szekciók ott görgetnek.

## Leképezés a Kick UI-technikára - kockázatok

A Kick UI JUCE-alapú (`juce::Component`, `KickLooknFeel : LookAndFeel_V4`, `KickSlider/KickRotary`, `KickGainSlider`), SVG-skin + `theme.xml`
(színkulcs-ID-k, `Images` map, betűméret-térkép XS-XL), fix pixeles layout-makrókkal, `mt::MTProperties` multitouch-réteggel és `juce::Timer`-es animációval.
A VST/CLAP-burkoló szerkesztő (`apps/kickvst`) jelenleg fix 572x357-es.

1. **Reszponzív layout / container query** - nincs megfelelője; a `Component::resized()`-ben kézzel kell `FlexBox`/`Grid`-del és breakpointokkal újraszámolni
   (és a kompakt <-> teljes elrendezésváltást is). A mostani fix pixeles `#define`-ok (`gMargin`, `KnobWidth`...) átírandók arányosra. Plugin-ablak átméretezés
   (`setResizable` + host-méret CLAP `gui.resize`) még nincs bekötve. **Legnagyobb kockázat.**
2. **Téma-rendszer** - a `theme.xml` színkulcsai nem fedik a mockup tokenjeit (panel-gradiens, inset, világos téma, velocity-sáv színek). Új ID-k és egy
   *light* téma kell; a mockup CSS-változói közvetlenül leképezhetők új `KickColorID` csoportokra.
3. **Gradiens, belső árnyék, finom mélység** - JUCE-ban `ColourGradient`/`DropShadow` + előrenderelt `Image` cache kell; per-paint rajzolva drága, nagy ablaknál
   észrevehető CPU (a Kick a hardveren ARM-on fut). Ajánlott: statikus rétegek cache-elt `Image`-be.
4. **Gombok (knob) ívvel** - a mostani `drawRotarySlider` SVG-hátteret használ; a bipoláris ív + érték-ív + kupak `juce::Path` + `PathStrokeType` kell (egyszerű, de új kód).
5. **Velocity-görbe szerkesztő** - egyedi `Component` (húzható pont, kvadratikus görbe, élő jelölő); nincs megfelelő kész Kick-widget; a DSP-oldalon a görbe LUT-tá alakítása is kell.
6. **88 billentyű + velocity-szín + sustain-tartott állapot** - a `KeyboardComponent` a `juce::MidiKeyboardState`-re épül; 3 állapot (lenyomva/tartva/jelölő) és velocity-szín
   egyedi `drawWhiteNote/drawBlackNote` kell; a tartott állapot a motor sustain-állapotát igényli az UI-szálra (lock-free üzenet, nem a MIDI-állapot).
7. **Mérők és élő kijelzők** (mikrofon-mérők, voices/CPU/RAM, MIDI-aktivitás) - `Timer` (30-60 Hz) + atomikus értékek a hangmotorból; a CPU/RAM és a voices számlálók forrása még nem definiált.
8. **Betöltés/letöltés képernyő** - hálózati letöltés, ellenőrzőösszeg, folytatás, mappaválasztás: a UI-nál nagyobb munka (háttérszál, megszakítható `URL::downloadToFile`,
   hiba-állapotok); a `KickProgressBar` csak egyszerű százalékot rajzol, a csíkozott animáció és a tétel-lista új komponens.
9. **Tipográfia** - Myriad Pro licenc/csomagolás (jelenleg a téma tölti be fájlból); a mockup rendszerfont-fallbackot használ, JUCE-ban a font-metrikák és a kerning eltérnek -
   a szövegek hossza (pl. előbeállítás-nevek) vágást/ellipszist kell, hogy kapjon.
10. **Tooltip + billentyűzet-navigáció/accessibility** - JUCE-ban a fókusz-sorrend (`FocusTraverser`) és a `AccessibilityHandler` külön kell; a CSS `:focus-visible` gyűrű
    kézi rajzolás. A Kick érintés-központú (multitouch), a hover állapot hardveren nincs - a tooltipnek touch-ra másik viselkedés kell (hosszú nyomás).
11. **Placeholderek** - a mintaattribúció (CC-BY 3.0), "Built with" lista, build hash, verzió valódi adatai a mintakészlet és a függőségek véglegesítése után töltendők ki; a
    mockupban szereplő hangszer-adatok (GB, velocity-rétegek, Electric) csak példák.
