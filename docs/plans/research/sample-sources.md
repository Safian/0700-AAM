# Zongoraminta-források kutatása (AGPL-3.0 CLAP zongora-plugin)

Dátum: 2026-11-04. Módszer: csak webes olvasás; a licencszövegeket az eredeti oldalról / a projekt
repójából kérdeztem le (idézőjeles sorok = a lekért oldal szövege). Ahol nem sikerült elsődleges
forrásig jutni: **NEM ELLENŐRIZVE**. Ez nem jogi tanács.

## 0. Összefoglaló döntés

1. **Alap: Salamander Grand Piano V3 (Yamaha C5, 16 velocity-réteg, release-ek) — a szerző 2022-03-04 óta
   public domainnek nyilvánította; a korábbi CC-BY 3.0 megadás visszavonhatatlan, így mindkét esetben
   szabadon terjeszthető, kereskedelmileg is.** Kényelmi okból érdemes az Accurate-Salamander (CC-BY, utóhangolt,
   kiegyenlített dinamika) változatot is megnézni, de annak a hozzáadott munkája CC-BY, tehát kreditet kér.
2. Kiegészítő hangszerek tiszta licenccel: Upright Piano KW (CC0), VCSL Keys (CC0: 3 zongora, 2 upright, csembaló).
3. Elektromos zongora (Rhodes/Wurlitzer): **nincs ellenőrzötten CC0/kereskedelmileg szabad** készlet (jRhodes3 = CC-BY-NC az audióra).
4. AI-útvonal (C) a fő hanghoz **nem életszerű**: a nyílt zongora-neurális modellek kutatási prototípusok, a nagy generatív
   modellek (MusicGen) súlyai NC-licencűek, a kimenet szerzői jogi státusza bizonytalan.
5. Saját felvétel (B) jó hosszú távú upgrade (pl. pedál-rezonancia, round-robin, saját hangszer), de napokban/hetekben
   mérhető munka; **gyári/digitális zongora mintázása terjesztésre: jogi csapda** (lásd 3. fejezet).

## 1. Szabad licencű meglévő készletek

### 1.1 Összehasonlító táblázat

| Készlet | Licenc | Kereskedelmi / újraterj. / átalakítás | Méret | Minőség (rétegek, release, hangolás) | Letöltés |
|---|---|---|---|---|---|
| **Salamander Grand V3** (Alexander Holm, Yamaha C5) | CC BY 3.0 (SFZ-katalógus, GitHub README, archive.org); a szerző oldalán: "As of 4.3.2022, this is now public domain!" | Igen / igen / igen. CC BY 3.0: kredit + licenc-URI + változtatások jelölése. PD-nyilatkozat után a szerző szerint a kredit "technically not required" | 394 MB (44.1k/16 WAV), 707 MiB (SFZ+FLAC), 1.18 GiB (48k/24 WAV), SF2 296 MiB | 16 velocity-réteg, 2×AKG c414 AB, ~12 cm a húrok felett, 48k/24, minden kisharmadot mintáz; hammer-noise release (kromatikus, 1 réteg), string-resonance release (kisharmadonként, 3 réteg); pedálzaj (csak on/off); V3-ban "Natural" és "Retuned" hangolás kulcskapcsolóval | https://freepats.zenvoid.org/Piano/acoustic-grand-piano.html ; https://archive.org/details/SalamanderGrandPianoV3 ; https://github.com/sfzinstruments/SalamanderGrandPiano |
| **Accurate-Salamander V6.2a** (Chisato Yamauchi) | CC-BY ("same licence as the original version"); módosításokat dokumentálja | Igen (kredit) | ~0.8 GB FLAC / ~1.6 GB WAV bankonként (4 bank: Accurate, Twilight, Moonlight, Noct) | 48k/24, bankonként 641 minta × 16 réteg; mind a 88 hang egyenként utóhangolva (2025-07-08, hangoló), dinamikai kiegyenlítés, egységes note-on időzítés, féligpedál (Disklavier-hez kalibrálva) | https://www.ir.isas.jaxa.jp/~cyamauch/AccurateSalamander/ |
| **Upright Piano KW** (Freepats) | CC0 1.0 ("Published under the terms of the Creative Commons CC0 1.0 public domain dedication.") | Igen / igen / igen | 32 MiB (SFZ+FLAC), 69 MiB WAV | Kawai upright, nappaliban, Zoom H1-gyel; **csak 2 velocity-réteg**, basszusban loopok; zajos/kis dinamika | https://freepats.zenvoid.org/Piano/acoustic-grand-piano.html |
| **VCSL Keys** (Versilian, Peter Eastman patchek) | CC0 ("No royalties, attribution, or credit are required ... commercial and non-commercial works without any restrictions") | Igen / igen / igen | 680 MB (+680 MB telepítő) | 3 nagy zongora (K, K legacy, S Model B), 2 upright, 5 csembaló; release-ek mindegyikben, pedál-minták "S" és "Knight"-on; réteg-szám az oldalon nincs | https://versilian-studios.com/vcsl-keys/ ; https://versilian-studios.com/Distro/VCSL_Keys.zip |
| **Headroom Piano** (Bengt Nilsson, Yamaha C3) | CC BY 4.0 | Igen (kredit) | 156 MB FLAC / 875 MB WAV | 5 velocity-réteg, close + Decca-tree mikrofon; RR/release-minta a README szerint nincs említve | https://sfzinstruments.github.io/pianos/headroom_piano ; https://github.com/sfzinstruments (BengtNilsson.HeadroomPiano) |
| **LivingRoom Upright Micro** (KeyPleezer) | CC BY-SA 4.0 | Igen, de share-alike (lásd 1.3) | 107 MB | "free micro version" | https://sfzinstruments.github.io/pianos/livingroom_upright_micro_sfz |
| **YDP Grand Piano** (Freepats, Zenph/OLPC Disklavier-minták) | CC BY 3.0 | Igen (kredit) | 36 MiB SF2 | réteg-szám nincs megadva; kis | https://freepats.zenvoid.org/Piano/acoustic-grand-piano.html |
| **Splendid Grand Piano** (AKAI Steinway, SFZ: kinwie) | A katalógus "Public Domain"; README: "released as public domain in early 2000 by Akai company" | **Kockázatos**: a PD-állítás másodkézből való, Akai-forrásból **NEM ELLENŐRIZVE** | 77 MB | 4 (+1 szintetikus) velocity-réteg, magas hangok csak lefogott módban | https://github.com/sfzinstruments/SplendidGrandPiano ; https://sfzinstruments.github.io/pianos/splendid_grand_piano |
| **Piano in 162** (Ivy Audio, Steinway B) | Az SFZ-katalógus oldalán **nincs licenc**; másodlagos források szerint ingyenes személyes és kereskedelmi használatra; újraterjesztés **NEM ELLENŐRIZVE / nincs engedve** | Nem terjeszthető csomagban | 4.9 GB | 5 dinamikai szint, 2 RR, pedál on/off minták, 2 mikrofon; Ivy Audio oldal: torrent-letöltés | https://sfzinstruments.github.io/pianos/in_162 ; https://ivyaudio.com/Piano-in-162 |
| **Maestro Concert Grand** (Mats Helgesson, Yamaha CF-3) | Egyedi: "All rights reserved ... You may not modify and spread this soundfont without the author's written permission" | **Nem** terjeszthető | 268.5 MB | 5 réteg | https://raw.githubusercontent.com/sfzinstruments/MatsHelgesson.MaestroConcertGrandPiano/master/mcgreadme.txt |
| **Iowa MIS Steinway B** | Oldalon: "may be downloaded and used for any projects, without restrictions." (formális licenc nincs) | Használható; terjeszthetőségről formális szöveg nincs, harmadik fél (Zynthian) szerint "must not be sold ... 'as is'" — **NEM ELLENŐRIZVE** elsődlegesből | kicsi | csak pp/mf/ff, 44.1k/16, 2×Neumann KM84, nem anechoic | https://theremin.music.uiowa.edu/MIS.html ; https://theremin.music.uiowa.edu/MISpiano.html |
| **Pianobook-csomagok** | Csomagonként eltérő; a FAQ szerint felhasználás kompozícióban szabad, de a minták **nem adhatók tovább** | **Nem** | – | – | https://pianobook.co.uk/faq |
| **jRhodes3c / 3d** (Rhodes Mk I Stage 73) | Minták: CC-BY-NC (3d), 3c: CC-BY-NC-SA 4.0; a többi CC0 | Kereskedelmi terjesztésre **nem** | 18 MB / 92 MB | 5 réteg, hurkolt/hurkolatlan | https://sfzinstruments.github.io/pianos/jrhodes3d |
| **Greg Sullivan E-Pianos** | CC BY 3.0 | Igen (kredit) | 21.5 MB | az oldal nem mondja meg a modelleket | https://sfzinstruments.github.io/pianos/greg_sullivan_e-pianos |

A teljes katalógus: https://sfzinstruments.github.io/pianos (22 sor). A Freepats-on nincs külön Rhodes/Wurlitzer
(főoldal: Piano = Acoustic Grand, Honky-tonk, Synth Piano).

### 1.2 A Salamander licencének pontos állása
- GitHub README: "This work is licensed under a Creative Commons Attribution 3.0 Unported License." Forrás: https://archive.org/details/SalamanderGrandPianoV3 ; kéri, hogy a származékot a repóban jelezzék. https://github.com/sfzinstruments/SalamanderGrandPiano
- A szerző oldala: "As of 4.3.2022, this is now public domain!" és "Have fun with it, it's yours and noones!" Formális licenc (CC0) neve **nincs** megadva; 2026 januári kommentje: a kredit "technically not required now", a felhasználó lelkiismeretére bízva. https://rytmenpinne.wordpress.com/sounds-and-such/salamander-grandpiano/
- A GitHub README viszont továbbra is CC BY 3.0-t ír (az utolsó frissítés 2021-12-20), az abcjs-soundfonts repó idézi a PD-állítást: https://github.com/educandu/abcjs-soundfonts
- Gyakorlati következtetés: a CC BY 3.0 megadása visszavonhatatlan ("Our licenses are irrevocable" — CC); a PD-nyilatkozat csak bővít. **Mindkét út biztonságos; kreditet adj (README + About + NOTICE), és hivatkozz mindkét forrásra.**
- Hangolás/mapping: az SFZ-átdolgozást kinwie készítette (ARIA-kiterjesztések) — mi a saját zóna-/réteg-motorunkat használjuk, tehát ezt nem kell átvenni.

### 1.3 AGPL-3.0 kompatibilitás (terjesztési forma)
- AGPL §5: "Inclusion of a covered work in an aggregate does not cause this License to apply to the other parts of the aggregate" (aggregate = "separate and independent works ... not combined ... to form a larger program"; a gyűjtemény "not used to limit the access or legal rights of the compilation's users beyond what the individual works permit"). Forrás: https://www.gnu.org/licenses/agpl-3.0.txt
- CC BY 3.0 §4(a): "You may not sublicense the Work" és "This Section 4(a) applies to the Work as incorporated in a Collection, but this does not require the Collection apart from the Work itself to be made subject to the terms of this License." Forrás: https://creativecommons.org/licenses/by/3.0/legalcode.txt
  → **A minták külön fájlokként, külön LICENSE/NOTICE-szal terjeszthetők a plugin mellett (aggregátum/gyűjtemény). Ne olvaszd bele a mintát a bináris AGPL-os műbe** (pl. `include_bytes!`-szal egyetlen .clap-ba, mert akkor a CC-BY mű AGPL alá "sublicenszelődne"). Ne alkalmazz DRM-et/titkosítást a mintákon (§4(a) "effective technological measures"). Saját formátumra (FLAC, 24→16 bit, vágás) konvertálás: **Adaptation**, jelölni kell a változtatást (§3(b)/§4(b)(iv)) — a Salamander esetén a PD-nyilatkozat ezt feleslegessé teszi, de jelöljük.
- FSF: a CC BY 4.0 "is compatible with all versions of the GNU GPL" (de "like all CC licenses, it should not be used on software"); CC BY-SA 4.0 "is one-way compatible with the GNU GPL version 3"; CC0: "Both public domain works and the lax license provided by CC0 are compatible with the GNU GPL." Forrás: https://www.gnu.org/licenses/license-list.html . A CC BY 3.0-ról az FSF-lista nem szól (nem találtam) — **NEM ELLENŐRIZVE**. AGPL §13 engedi a GPLv3-as művel való kombinálást. CC maga: "our licenses are currently not compatible with the major software licenses" (kód-terjesztési szabályok hiánya miatt): https://creativecommons.org/faq/ → ezért a minták maradjanak adatként, külön csomagolva.
- CC BY-SA 4.0 (LivingRoom): egy-irányú GPLv3-kompatibilis, de a share-alike miatt a származék-minták SA alatt maradnak; a BY-SA→AGPL kompatibilitás **NEM ELLENŐRIZVE** → kerüld.
- Hosting: első indításkori letöltésnél a letöltő kód lehet AGPL; a mintacsomag külön URL-ről (archive.org / freepats mirror / saját release), SHA-256-tal rögzítve. A harmadik fél tárhelyét ne terheld (a szerző szerint "others are welcome to mirror" — a Salamander oldalán).

### 1.4 Minőségi vélemény (rövid, forrás-alapú)
- Salamander: a katalógus és a README szerint a legjobb szabad kiindulás 16 réteggel és release-ekkel. Ismert gyengeségek (README/fórumok, **másodlagos**): alapértelmezett `amp_veltrack` 73% (halk játék hangosabb), kisharmadonkénti mintavételezés (pitch-nyújtás), pedál-lenyomott hang-minta nincs a README-ben (csak pedálzaj + string-resonance release) — **NEM ELLENŐRIZVE a fájlokban**. Az Accurate-Salamander V6 ezeket (hangolás, dinamika-egyenletesség) javítja.
- Upright KW / VCSL Keys: jó kiegészítő hangszínek, nem versenyeznek a Salamanderrel dinamikában (KW: 2 réteg).

## 2. Mesterséges / AI-útvonal

| Terület | Licenc / státusz | Minőség és használhatóság |
|---|---|---|
| DDSP (Magenta) | Apache-2.0 (https://github.com/magenta/ddsp) | Hangszínátvitelre jó; piano-specifikus kutatás: Frontiers 2023 hibrid DDSP-zongora **csak a harmonikus részt modellezi**, "the noise components (hammer-string collision, soundboard vibration) are not included"; 168 hangon tanítva; a szerzők nem állítanak realizmust (https://www.frontiersin.org/articles/10.3389/frsip.2023.1276748) |
| ddsp-piano (lrenault) | kód Apache-2.0; súlyok licence nincs külön megadva; tanítóadat MAESTRO v3 | MAESTRO: **CC BY-NC-SA 4.0** (https://magenta.tensorflow.org/datasets/maestro) → a tanított súlyok kereskedelmi/AGPL-terjesztése jogilag **bizonytalan, NEM ELLENŐRIZVE**; README: "frequency estimation with differentiable oscillators is still an unsolved issue" (https://github.com/lrenault/ddsp-piano) |
| Fizikai modellezés | Pianoteq: zárt, kereskedelmi (https://www.modartt.com/morphing). Nyílt: FigBug/piano, digitális hullámvezető, GPL-3.0 (https://github.com/figbug/piano); Faust-STK piano (régi) | A GPL-3.0 kód AGPL §13 szerint kombinálható. Hangminőségi mérés/hallgatási teszt a README-ben **nincs**; 43 csillag, kiadás nincs → prototípus. Pianoteq-szint (SoS szerint versenyképes a nagy mintakészletekkel) 10+ évnyi zárt fejlesztés |
| MusicGen / AudioCraft | kód MIT; **súlyok CC-BY-NC 4.0** (https://github.com/facebookresearch/audiocraft) | A kimenet licencét elsődleges forrás nem mondja ki; közösségi olvasat: NC-hatás a kimenetre is — **NEM ELLENŐRIZVE**. Kereskedelmi csomagba nem használható. Egyszerű zongora-hangmintához (izolált, hangmagasság-pontos, velocity-ra vezérelt hang) nem való |
| Stable Audio Open 1.0 | Stability AI Community License; <$1M éves bevétel esetén kereskedelmi; "non-sublicensable"; "Powered by Stability AI" kötelező terjesztéskor; a kimeneteket nem használhatod más alapmodellek javítására; "You own any outputs ..." (https://stability.ai/community-license-agreement). Tanítóadat: Freesound/FMA, CC0/CC BY/CC Sampling+ (https://huggingface.co/stabilityai/stable-audio-open-1.0) | Kártya: "better at generating sound effects and field recordings than music", max 47 s, 44.1 kHz; hangszerminőségről nincs adat. Pitch-pontos, 88-hangos, velocity-rétegezett készlet generálása nem megbízható; a sublicense-tilalom ütközik a harmadik felek szabad terjesztésével |
| Generált kimenet szerzői joga | USA (USCO, 2025-01-29): a prompt önmagában nem elég szerzőséghez (https://copyrightalliance.org/ai-report-part-2-copyrightability/ , másodlagos összefoglaló; az elsődleges PDF-et nem sikerült kiolvasni) | Ha nincs emberi szerzőség, a kimenetre nem lehet szerzői jogot igényelni → nem tudsz érvényes licencet adni rá; EU/HU jog **NEM ELLENŐRIZVE** |

Őszinte értékelés: **jó, produkciós zongorához a valódi felvételhez képest ma nem működik** (nyílt, ellenőrzött,
kereskedelmileg tiszta út nincs). Ésszerű AI-szerep: **segédeszköz** a feldolgozásban (zajtalanítás, loop-keresés, hangolás-/
hangerő-kiegyenlítés, hiányzó velocity-rétegek interpolálása a PD-s Salamanderből) — a tanítóadat ekkor tiszta (PD).

## 3. Saját felvétel

Mi kell hozzá (mérce: az Accurate-Salamander bankja = 641 minta × 16 réteg, 48k/24; az eredeti Salamander minden kisharmadot mintáz):
- Hangszer: jól karbantartott, hangolt akusztikus zongora (hangoló kell a felvétel előtt és közben); a Disklavier-szerű MIDI-vezérelt hangszerrel a velocity ismételhető és automatizálható (a Salamander-utód is Disklavierhez kalibrált féligpedált használ; Zenph/OLPC YDP-minták is Disklavierből). Rendes akusztikus teremmel/csendes szobával.
- Mikrofon: minimum 1 pár kondenzátor (a Salamander: 2×AKG c414 AB, ~12 cm a húrok felett); opcionálisan Decca-tree/szoba-mikrofonok (Headroom: close + Decca tree).
- Szoftver: DAW + szkript az automatikus note-triggerhez (MIDI), vágás/normalizálás, zajszűrés, loop (a lecsengő hangoknál), hangolás-javítás, zóna-leképezés a saját motorhoz (SFZ-mentes is lehet).
- Mérték (**BECSLÉS, saját mérnöki becslés, nem forrás**): 88 billentyű × 8-16 réteg × 2-3 RR ≈ 1500–4000 felvétel + release-ek + pedál-lenyomott változat; felvétel 2-4 nap automatikus hangszerrel (több kézzel); utómunka (szétvágás, hangerő-/hangolás-kiegyenlítés, zónák, loopok, QA) 2-6 hét egy fejlesztőnek; további költség: terem/hangszer-bérlés, hangoló, mikrofonok.
- Licenc: a saját felvétel a miénk (CC0/CC-BY-SA/bármi), a legtisztább lehetőség — **ha a hangszer akusztikus**.
- **Jogi csapda: digitális zongora vagy szintetizátor mintázása terjesztésre.** Pianobook szabálya: "Digital synths cannot be resampled and redistributed without the manufactures express permission", beleértve a digitális zongorákat is, "where the sound is a sample"; "Even if the model has been discontinued, if the company is still operating- they still own the rights to the samples."; elfogadott: akusztikus hangszer vagy generátor-szintézis (https://www.pianobook.co.uk/?p=4987). Ez egy projekt (Pianobook) saját szabálya, nem jogszabály, de az iparági gyakorlat: gyári hang (Yamaha CFX stb.) = a gyártó felvétele → tilos újraterjeszteni. A Yamaha saját álláspontját nem találtam: **NEM ELLENŐRIZVE** — a gyártó hangszer-kézikönyvét/EULA-t kell olvasni. A Disklavier csak mechanikus-akusztikus (zongora hang), tehát az felvétele rendben van (az akusztikus hang nem az elektronika "minta").

## 4. Ajánlott döntés (rangsor)

| # | Út | Kockázat | Teendő |
|---|---|---|---|
| 1 | **A + D (alap): Salamander V3/Accurate + saját zóna-motor, CC0 kiegészítők (Upright KW, VCSL Keys)** | Alacsony. CC-BY-kredit, külön fájl/csomag, nincs DRM; Splendid/Piano-in-162/Maestro kerülendő | NOTICE + About-kredit (Holm, Yamauchi, mindkét forrás-URL, a PD-nyilatkozat dátuma); a PD-nyilatkozatot archiváld (web.archive.org snapshot) és a repóba tegyél másolatot a CC BY 3.0 szövegével; mintacsomag külön letöltés SHA-256-tal; döntsd el, V3 vagy Accurate V6.2a (CC-BY!) — a V3 PD-bb |
| 2 | **D: hibrid** — A + saját rövid felvétel a hiányzóra (pedál-lenyomott rezonancia, extra RR, upright/electromechanical, saját akusztikus Rhodes/Wurlitzer) | Alacsony-közepes (munka) | felvételi terv, a saját anyag CC0-ként a repóba |
| 3 | **B: teljes saját felvétel** | Közepes (idő, minőség) | csak ha egyedi hangszín kell vagy a Salamander-C5 minősége nem elég |
| 4 | **C: AI / modellezés mint fő hangforrás** | Magas (minőség + licenc + szerzői jog) | csak kutatási mellékág; ne blokkolja a kiadást. A segédeszköz-használat (zajtalanítás, interpoláció) rendben |

Nyitott kérdések: (1) a Salamander V3 minta-fájljain a pedál-lenyomott minták megléte; (2) Splendid PD-státusza Akai-forrásból; (3) EU/HU szerzői jog AI-kimenetre; (4) CC BY 3.0 ↔ AGPL kompatibilitás formális FSF-állásfoglalása — jogász véleménye kell kereskedelmi/nagy terjesztés előtt.
