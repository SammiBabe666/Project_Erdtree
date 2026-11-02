# The dark-fantasy collection: eight sounds, what each one is for

A sound-design specification, not a preset file. The values that make each sound are in its platform's preset
folder (today only Opus has them: `platforms/opus/presets/<folder>/sound.json`). A later port can build the same
eight from this page and its own preset format.

The collection's rule: every sound uses only erdtree's own controls (choir, strings, space, brass), the OS's
envelope, its octave and one stock effect. No sound copies a melody, a recording or a name from a game. Each one leans
on a different part of the engine, so that the eight are told apart by ear on the same notes.

| # | Name | Folder | Sonic identity | What carries it in erdtree |
|---|---|---|---|---|
| 1 | Crown of Ash | `crown-of-ash` | Majestic, warm, layered brass with a solemn orchestral character. | Brass leads (28000), strings under it (20000), a little choir on top; a moderate attack (the brass scoop and the bow swell both heard); spring. Uses all three sections: the "full orchestra" sound. |
| 2 | The Hollow Belfry | `hollow-belfry` | Dark, resonant, bell-like strike with metallic overtones, decaying in a large space. | Brass at full with choir, no strings; the fastest attack and no sustain, so each note is a strike that only decays. The brass timbre follows the envelope, so the strike is bright and the ring darkens as it fades. Space near full for distance; the delay. Bell-like only: there are no inharmonic partials. |
| 3 | Thornwake | `thornwake` | Sharp, woody, restless, with an organic, thorny edge. | Strings at full, close and dry (space 0: tight detune, the brightest tone, the quickest bow), a brass bite for the attack, short envelope (plucked feel), grid. The brightest sound of the eight. |
| 4 | Vesperglass | `vesperglass` | Fragile, glassy, mournful, shimmering upper register. | The choir alone (its harmonics and its shimmer, nothing else), an octave up, little space so it stays clear; a soft attack, a long fade, the delay. The thinnest and most transparent. |
| 5 | The Sunken Choir | `sunken-choir` | Dense, breathy, vocal-like harmonics in a ruined sacred chamber. | The choir at full with space at full: its breath and shimmer at their deepest and its tone darkest; strings beneath; a slow swell; spring. |
| 6 | Grave of Giants | `grave-of-giants` | Massive, low-register brass and dark, weighty resonances. | Brass near full an octave down with celli under it, darkened by distance; a quick attack, grid. The heaviest and lowest. |
| 7 | Moonless Vigil | `moonless-vigil` | Restrained, haunting, soft, for introspective night passages. | Soft strings and a breath of choir at full distance; a slow swell and a slow fade; spring. Deliberately about 4 dB quieter than the rest. |
| 8 | The Unwritten God | `unwritten-god` | Strange, unstable, otherworldly, with alien grandeur. | Choir and brass at full together, space at full, an octave down; an attack of seconds, so the brass's timbre slides with the rising envelope while the choir breathes against it; the delay. The darkest and slowest to speak. |

## How they differ, measured

From `platforms/opus/tests/preset_audition.py` (PC render of the Opus build; a stand-in envelope and no effect for
these numbers): the held minor chord's spectral centre and the time to reach 90 % of its level.

| Name | Brightness (Hz) | Speaks in (s) | Sections |
|---|---|---|---|
| Crown of Ash | 2491 | 0.10 | choir, strings, brass |
| The Hollow Belfry | 1425 | 0.01 | choir, brass |
| Thornwake | 3364 | 0.05 | strings, brass |
| Vesperglass | 1433 | 0.44 | choir |
| The Sunken Choir | 1277 | 1.36 | choir, strings |
| Grave of Giants | 1364 | 0.08 | strings, brass |
| Moonless Vigil | 2137 | 1.76 | choir, strings |
| The Unwritten God | 632 | 2.85 | choir, strings (a little), brass |

## The effects

Each sound sets one stock effect (spring, delay or grid) with values inside the factory presets' ranges. What each
effect parameter does on the OP-1 was not measured here, and the PC renders use a stand-in, so the effect settings
are a starting point to be tuned by ear on the device.

## Names

The eight names are original working titles chosen for this collection. They are not claims of legal clearance.
They avoid the names of game characters, places, factions, items and spells.
