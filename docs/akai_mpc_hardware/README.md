# Akai MPC hardver tudástár

Ide gyűjtjük az Akai MPC (elsősorban a MPC Key 61) hardverrel kapcsolatos tudást: leírások, felhasználói útmutatók, mért viselkedés. A projekt saját Akai-doksijai: `../AKAI_MPC_KEY61_*.md`.

## Hivatalos dokumentumok (letöltve 2026-11-01, `manuals/`)
A PDF-ek **helyben vannak, de nincsenek a git-ben** (`manuals/.gitignore`: `*.pdf`): az inMusic szerzői jogvédett anyaga, és kb. 68 MB. Újra letölthetők az alábbi hivatalos linkekről; a SHA-256 az ellenőrzéshez van.

| Fájl | Verzió | Méret | Forrás | SHA-256 |
|---|---|---|---|---|
| `MPC_Standalone_OS_User_Guide_v3.10.pdf` (528 oldal; ez érvényes az MPC Key 61-re is) | v3.10 | 38,0 MB | https://cdn.inmusicbrands.com/Software/M310PCFIN26OCT/MPC%20Standalone%20OS%20-%20User%20Guide%20-%20v3.10.pdf | `24a7dc7511904cf6c593664ef2e3c128b60ab54c1769a71e08c60dce0530a35a` |
| `MPC_Software_User_Guide_v3.10.pdf` (MPC Software, számítógépes) | v3.10 | 27,7 MB | https://cdn.inmusicbrands.com/Software/M310PCFIN26OCT/MPC%20Software%20-%20User%20Guide%20-%20v3.10.pdf | `17dd7ea6a1023977f92c36b85d4ff8a5a150c3ffa3dd51da30db74b335546610` |
| `MPC_Key_61_Quickstart_Guide_v1.2.pdf` (52 oldal) | v1.2 | 2,2 MB | https://cdn.inmusicbrands.com/akai/M211PCVI/MPC%20Key%2061%20-%20Quickstart%20Guide%20-%20v1.2.pdf | `8cba5e10f477a19d27151c2c881d6e66036ff226d78c098bc301996b2e177e7b` |

Források és hivatkozások: [MPC Manuals](https://www.akaipro.com/mpc-manuals/) (itt a Key 61 és a többi MPC a "MPC Standalone OS – User Guide v3.10"-re mutat), [MPC Key 61 letöltések](https://www.akaipro.com/downloads-and-support/downloads/?product=MPC%20Key%2061), [MPC 3.10 áttekintés](https://www.akaipro.com/mpc3/), [firmware](https://www.akaipro.com/downloads-and-support/downloads/firmware/mpc/). A régebbi közös útmutató (v2.15, X/X SE/Live/Live II/One/One+/Key 61/Key 37/Studio mk2/Touch) is elérhető a Manuals oldalon.

## Szöveg kinyerése (kereséshez)
macOS-en nincs `pdftotext`; PDFKittel:
```swift
import PDFKit
let d = PDFDocument(url: URL(fileURLWithPath: CommandLine.arguments[1]))!
try d.string!.write(toFile: CommandLine.arguments[2], atomically: true, encoding: .utf8)
```
(`swiftc -O pdf2txt.swift -o pdf2txt && ./pdf2txt in.pdf out.txt`). A kinyert szöveget sem commitoljuk (ugyanaz a szerzői jog).

## A gépről a dokumentációból (MPC Key 61 Quickstart v1.2, v3.10 útmutató)
- 44 dedikált funkciógomb, fehér, piros vagy zöld háttérvilágítással; 16 sebesség- és nyomásérzékeny, RGB-világítású pad; 61 billentyű aftertouchhal; 7"-os multitouch kijelző.
- Tap Tempo / Global gomb: fehér = a kiválasztott szekvencia saját tempója, piros = globális tempó (a v3.10 más MPC-knél borostyán).
- Pad-színek: némított pad narancs, kijelölt pad zöld, hozzárendelt minta fényes sárga, hozzárendelés nélküli halvány sárga, felvétel alatt villogó piros.
- Gombfunkciók (`<`/`>` egy lépés, Locate+ előző/következő esemény, `<<`/`>>` egy ütem, Locate+ eleje/vége) a Quickstartban.

- **Main Volume** (Quickstart v1.2, 45. pont; v3.10 útmutató): a gomb a kimenetek és a fejhallgató-kimenet közös hangerejét állítja. A dokumentáció nem mondja, hogy analóg, MIDI vagy szoftveres. Műszaki adatok: (4) TRS kimenet (2 sztereó pár), (1) sztereó fejhallgató-kimenet, (8) CV/Gate. Mérés: nem MIDI és nem input-esemény (lásd a riportot), a Kick alatt nem hat.

## Mért viselkedés
- [KEY61_LED_COLOURS.md](KEY61_LED_COLOURS.md): a gombok LED-protokollja és színei (kamerás mérés, 2026-11-01).
