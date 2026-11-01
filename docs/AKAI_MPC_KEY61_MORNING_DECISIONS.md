# Reggeli átnézés: éjszakai döntések és dummy elemek

A felhasználó utasítása: ne álljunk meg apró döntés miatt; ha UI-elem kell, **dummyt** teszünk be (működő hely, látható jelölés, minimális viselkedés), reggel véglegesítjük. Minden worker és az orchestrator ide jegyzi (sorokat hozzáfűzve):

| Dátum | Terület | Döntés / dummy | Indok | Hol (fájl) | Reggeli teendő |
|---|---|---|---|---|---|
| r5 J1 | Mentes / pan (G10) | FELULIRVA: a mentes az elo fat nem modositja; `KickXML::patchSnapshot` a szerializalt masolatba irja a `panBases` base pant (`MTAudioManager`); read-only helyen Autosave/Close a `prepareSave` elott kilep | Az elo fa visszairasa hallhato pan-ugrast okozott Read modban autosave-nel | `kick_xml_manager.h/.cpp`, `mt_audio_manager.cpp` | Jovahagyas; ismert: `relativizeSamplePaths` meg az elo fan fut (kulon vizsgalat) |
| r2 A | Nemitott track (G5) | Mute mellett note-off, pitch wheel, CC64, CC123 atmegy (`mt_mute_filter.h`) | Ne ragadjon hang/pedal mute kozben | `mt_audio_track.cpp`, `mt_mute_filter.h` | Jovahagyas |

## Már meghozott döntések (round 1)
- Sustain és pitch bend csak az élő bemenetre hat (Behringer-elv): megvalósítva (`noteProcess(msgs, live)`).
- RECALL: tick vagy wall-clock eseményenként; megállítva rögzített hosszú felvétel a track hosszára hajtogatva.
- Song módban a seek tiltott.
- Globális TC kapcsoló csak az Apply-ra érvényesül.
- `KICK_TOUCH_SINGLE_TAP_LISTBOX` CMake opció (alap: ki); az MPC-s build ezt BE kell kapcsolja.

| 2026-11-01 | Akai LED protokoll (G4) | DUMMY: futásidejű kapcsoló `KICK_AKAI_LED_MODE=note\|cc\|noteoff0` (alap: note = mai viselkedés); az első snd_rawmidi_write hiba egyszer logolva | Gombok LED-je bekapcsolva marad, bár `0x90 <note> 0x00` megy ki; a valós protokollt csak az MPC-n lehet eldönteni | `Kick/apps/kick/hardware/led_map.h`, `adapters/akai_mpc_key61_adapter.h` (`start()`) | MPC-n: `KICK_AKAI_LED_MODE=cc ./Kick`, majd `noteoff0`; melyiknél alszik el a STOP/PLAY LED indításkor; a nyertes legyen az alap, a kapcsoló törölhető |
| 2026-11-01 | Pan CC10 mentés közben (R2 follow-up 1) | ELAVULT (J1): a `isSavingProject()` jelző és a CC10-tiltás törölve, mert a mentés már nem ír az élő fába | nincs visszaírás, ezért nincs mit elnyomni | - | - |
| 2026-11-01 | `isEffectivelyMute` figyelmeztetés (R2 follow-up 2) | A változó törölve a `MTTrack::onTick` régiójában | Nem használta semmi; a némítás-szűrés (`mutefilter::passesWhileMuted`) élő bemenetnél (mt_audio_track.cpp ~783) és az arp-nál már `isTrackEffectivelyMuted()`-ot hív | `Kick/modules/engine/audio_engine/engine/mt_audio_track.cpp` | L |
| 2026-11-01 | Bridge lebegő `this` a callAsync-ben (R2 follow-up 3) | `std::shared_ptr<std::atomic<bool>> m_alive` (stop() hamisra állítja), a lambdák másolatban fogják; az érintés-lambda is ellenőriz | A `m_stopping` a megsemmisülő objektumban él, a késői lambda felszabadított memóriát olvasna; a megosztott flag túléli a hidat | `Kick/apps/kick/kick_hardware_bridge.h` | L |
