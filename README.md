# Sound System (VCV Rack)

One module by Haer. It makes anything you feed it sound like it is coming out
of a big physical sound system in a real place: the weight of scoop bins, the
honk of horns, amps being pushed, the room around you and how far you are
from the stack.

20 HP, stereo in and out (mono in is fine; most sound systems are mono anyway).

## Controls

| Control | What it does |
|---|---|
| **SYSTEM** (1-8) | Picks the rig and the place. Changing it glides over about a third of a second. |
| **PUSH** | How hard the rig is driven: louder-feeling bass and highs, the amp limiter clamping, valve grit, drivers starting to strain. |
| **DISTANCE** | From right in front of the stack (more sub, mostly direct sound) to the back of the place (less top, more room). The screen shows metres. |
| **WEIGHT** | The sub bins and bass. Also adds harmonics of the sub so you "feel" it on small speakers, and lets the sub push the rest of the music about. |
| **ROOM** | How much of the space you hear: reflections, tail, room boom, echo. |
| **TOPS** | Horns and tweeters. |
| **CROWD** | A full room: bodies soak up the reverb and the top end. |
| **MIX** | Dry to sound system. |
| **DOOR** + **GATE** | For 8 THROUGH THE WALL: the button opens and shuts the door; a high gate flips it. |

CV inputs (10 V = full knob): SYSTEM (0-10 V steps through all 8), PUSH, DISTANCE, WEIGHT, ROOM.

## The 8 systems

1. **DUB HALL**: sound system in a hall. Huge scoop bins around 45 Hz, valve-warm amps, honky mid horns, a dark hall, things rattling. Mono.
2. **TECHNO CLUB**: clean, punchy modern stacks in a tall concrete room. Tight sub, clear tops, long bright tail.
3. **WAREHOUSE**: a rented rig pushed too hard in brick and steel. Boomy room, long muddy tail, heavy limiting.
4. **BASEMENT**: low ceiling, bodies, a rig too big for the room. Short tail, strong low boom.
5. **KINGSTON STREET**: open air street dance. Very deep scoop, piercing tweeters, no tail, slap echo off the buildings, zinc fences buzzing. Mono.
6. **DISCO LOFT**: warm audiophile horns and a wooden floor. Tight bass, gentle on the amps.
7. **FESTIVAL**: line arrays in a field. 15 to 160 m away, delay towers, air taking the top off, wind moving it around.
8. **THROUGH THE WALL**: the techno club heard from the toilets. Muffled boom, the door rattling, a small tiled room around you. Press DOOR.

## Patch ideas

- Put it on the whole mix at the end of a patch, PUSH at noon, and slowly turn DISTANCE during a breakdown as if you're walking to the bar.
- Gate DOOR from a sequencer on the last bar of a phrase: the full club bursts in, then the door shuts again.
- Slow LFO into SYSTEM CV for a set that drifts from dub hall to warehouse.
- Run only the kick and bass through DUB HALL with WEIGHT high, and keep the hats dry (MIX on a second copy).

## Building

`make` with the Rack SDK (`RACK_DIR=...`). GitHub Actions builds the Mac (Apple Silicon) plugin on every push.
`make test` runs the engine offline and writes `test/soundsystem-demo.wav` (a dry loop, then each of the 8 systems, a distance sweep, the door opening, extremes, and a sweep through all systems).
Panel: `python3 res/make_panel.py` (needs fonttools); preview: `python3 tools/preview.py panel-preview.png`.
