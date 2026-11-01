# Reggeli átnézés: éjszakai döntések és dummy elemek

A felhasználó utasítása: ne álljunk meg apró döntés miatt; ha UI-elem kell, **dummyt** teszünk be (működő hely, látható jelölés, minimális viselkedés), reggel véglegesítjük. Minden worker és az orchestrator ide jegyzi (sorokat hozzáfűzve):

| Dátum | Terület | Döntés / dummy | Indok | Hol (fájl) | Reggeli teendő |
|---|---|---|---|---|---|

## Már meghozott döntések (round 1)
- Sustain és pitch bend csak az élő bemenetre hat (Behringer-elv): megvalósítva (`noteProcess(msgs, live)`).
- RECALL: tick vagy wall-clock eseményenként; megállítva rögzített hosszú felvétel a track hosszára hajtogatva.
- Song módban a seek tiltott.
- Globális TC kapcsoló csak az Apply-ra érvényesül.
- `KICK_TOUCH_SINGLE_TAP_LISTBOX` CMake opció (alap: ki); az MPC-s build ezt BE kell kapcsolja.

| 2026-11-01 | Akai LED protokoll (G4) | DUMMY: futásidejű kapcsoló `KICK_AKAI_LED_MODE=note\|cc\|noteoff0` (alap: note = mai viselkedés); az első snd_rawmidi_write hiba egyszer logolva | Gombok LED-je bekapcsolva marad, bár `0x90 <note> 0x00` megy ki; a valós protokollt csak az MPC-n lehet eldönteni | `Kick/apps/kick/hardware/led_map.h`, `adapters/akai_mpc_key61_adapter.h` (`start()`) | MPC-n: `KICK_AKAI_LED_MODE=cc ./Kick`, majd `noteoff0`; melyiknél alszik el a STOP/PLAY LED indításkor; a nyertes legyen az alap, a kapcsoló törölhető |
