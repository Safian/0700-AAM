# MPC Key 61: gomb-LED protokoll és színek (mért, 2026-11-01)

Mérés: a Kick app az MPC-n (`safian/main` `b8ee63a6`, ARMv7), a LED-ek a Mac kamerájával (Photo Booth) figyelve, sötétített szobában. A gombokat a `/data/kick_cmd` FIFO `btn <NÉV>` parancsaival kapcsoltam. A kamera felbontása a halvány LED-ek finom színeihez kevés: ahol nem volt egyértelmű, "nem eldönthető"-t írok.

## 1. Protokoll (a döntő lelet)
A Port 1-re (controls, `hw:0,0,1`) írt LED-üzenet formátumát a `KICK_AKAI_LED_MODE` kapcsolta át. Teszt: indulás után `btn PLAY`, a STOP LED-nek ki, a PLAY LED-nek be kell kapcsolnia.

| Mód | Üzenet | PLAY be | STOP ki | Eredmény |
|---|---|---|---|---|
| `note` (alap) | `0x90 <note> <érték>`; ki: `0x90 <note> 0` | nem látszott | **nem aludt ki** | **nem működik** |
| `noteoff0` | be: `0x90 <note> <érték>`; ki: `0x80 <note> 0` | nem látszott | **nem aludt ki** | **nem működik** |
| `cc` | `0xB0 <note> <érték>` (a gomb note-száma a CC-szám) | **igen, zöld** | **igen** | **működik** |

Következmény: az eszköz LED-eit a **Control Change** (`0xB0 <gomb-note> <érték>`) vezérli. A `note` módnál a gombok a firmware állapotában ragadnak (innen a korábbi "minden világít" benyomás: az előző futás állapota maradt). A `note` módban a LED-ek a kamerán is kéken derengtek, `cc` módban a nem bekapcsolt gombok sötétek lettek.

## 2. Megfigyelt színek (`cc` mód)
| Gomb (note) | Állapot | Szín |
|---|---|---|
| STOP (81) | bekapcsolva (megállított lejátszás) | fehér, erős |
| PLAY (82) | lejátszás | zöld, erős |
| PLAY START (83) | (alap) | zöld |
| AUTOMATION (75) | 1. nyomás | piros |
| AUTOMATION (75) | 2. nyomás | zöld |
| Pad bank A (35) | indulási állapot | fehér |
| MAIN (52) | indulási állapot | fehér (kamerán nem egyértelmű) |
| Pad-ek | alap, bank A | többszínű (a program/track színe); FULL LEVEL, bank C után egységesen rózsaszín-magenta |

Nem eldönthető a kamerából (felbontás/látószög): REC (73) és OVERDUB (80) két színe, a bank B–D gombok színei, TC (12), METRONOME (112), NOTE REPEAT (11), FULL LEVEL (39), ARP/LATCH és az oktáv-gombok (a bal oldali oszlop nincs a képben), LOCATE (70). Ezekhez közelebbi kamera vagy szemrevételezés kell.

## 3. Javaslat a projektnek
A `KICK_AKAI_LED_MODE` alapértéke legyen `cc`, a `note`/`noteoff0` ág törölhető. Az `akaiLedValue` két színű értékei (0–4) a `cc` módban működnek: az AUTOMATION piros/zöld váltása igazolja, hogy a második szín elérhető.

## 4. Kiegészítés (kamerás és fotós mérés, `cc` mód, 2026-11-01)
| Gomb | Szín / állapot |
|---|---|
| STOP | fehér-kékes, erős |
| PLAY | zöld (lejátszáskor) |
| PLAY START | zöld, mindig (a Kick nem vezérli) |
| `<`, `>`, `<<`, `>>`, `–`, `+` | kék-ciánkék, mindig (a Kick nem vezérli) |
| REC, OVERDUB | halvány szürke; a Kick soha nem vezérli (hiányzik) |
| MIX / PAD MIX | fehér / piros |
| MUTE / PAD MUTE | fehér / piros |
| GRID / STEP SEQ | világít (a két állapot színe nem különíthető el) |
| AUTOMATION | zöld = Read, piros = Write, sötét = Off |
| KEYBOARD CONTROL | rózsaszínes-piros |
| SAMPLE EDIT / SAMPLER | piros árnyalat / fehér (nem biztos) |
| Pad bank A–D | mindig csak a kiválasztott, fehér |
| TAP TEMPO (GLOBAL), LATCH, TOUCH FX, TOUCH STRIP | kékes-fehér, bekapcsolva |
| OCT + | zöldes-fehér, bekapcsolva |
| SHIFT | bal mindig világít (piros felirat), jobb csak Shift-nyomásra |
| Q-Link 1 LED | piros, mindig |
| Oktáv-kijelző (−3 … +3) | két piros pont (−1, 0), mindig |
| Kerekek | kék háttérvilágítás, mindig |
