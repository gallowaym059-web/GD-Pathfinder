# AI Pathfinder

**A predictive auto-dodge assistant for Geometry Dash (Geode, 2.2081).**

Pathfinder looks into the future instead of reacting to the present. It clones
your player into invisible *shadows*, simulates the next few seconds with the
game's own physics and collision code, and plays the input timeline that
survives. Think of it as Show Trajectory that also presses the button for you.

> **Status: alpha.** The code was written against the real 2.2081 Geode
> bindings but has not been compiled or play-tested yet. Expect to fix a few
> build errors and tune behaviour. See [Known limitations](#known-limitations).

---

## Features

| Area | What it does |
|---|---|
| **All gamemodes** | Cube, ship, ball, UFO, wave, robot, spider, swing. One generic search handles all of them, no per-mode logic. |
| **All gameplay objects** | Spikes, blocks, slopes, saws, orbs, pads, dash orbs, gravity / speed / size portals. Resolved by the game's own collision code. |
| **Dual mode** | Both players are simulated with the same shared input. |
| **Frame-perfect** | The first decision is a single 240 TPS tick, and the search prefers *keeping* the current input, so clicks land on the last tick that still works. |
| **Fast** | Plans are stored and replayed. Most ticks cost zero simulation. Searches have a hard millisecond budget. |
| **Self-correcting** | The real player is compared to the predicted path every tick. Drift triggers an instant re-plan. |
| **Predicted path overlay** | Green while holding, white while released, like Show Trajectory. |
| **Configured in settings** | No in-game menu. Everything lives in the mod's settings page. |

---

## How it works

```
 every physics tick (processCommands hook)
 │
 ├─ drift check ── real position vs. predicted position ── too far? ──┐
 │                                                                    │
 ├─ need a plan? (none / low runway / drift / timer) ─────────────────┤
 │                                                                    ▼
 │                                              ┌──────────────────────────────┐
 │                                              │ planAhead()                  │
 │                                              │  1. save real state into     │
 │                                              │     PlayerCheckpoint slots   │
 │                                              │  2. iterative-deepening DFS  │
 │                                              │     (H/3 → 2H/3 → H ticks)   │
 │                                              │  3. each node: load state,   │
 │                                              │     set input, step shadow,  │
 │                                              │     checkCollisions          │
 │                                              │  4. prune deaths + repeats   │
 │                                              │  5. replay winner → path     │
 │                                              └──────────────┬───────────────┘
 │                                                             │ Plan
 └─ apply this tick's input via handleButton ◄─────────────────┘
```

**State cloning.** Shadows are `PlayerObject`s restored with GD's own
`PlayerCheckpoint` (`saveToCheckpoint` / `loadFromCheckpoint`). Gamemode,
gravity, speed, size, velocity and orb/pad state all come along for free.

**Why clicks are frame-perfect.** At each node the search tries *the current
input first*, then the opposite. A button change therefore only appears in the
final plan when keeping the old input leads to death, which is the last
tick where the change still works. The first node advances exactly one tick, so
that timing is not rounded to a coarser step.

**Search.** Depth-first over input timelines, in steps of `Decision Step` ticks.
Dead branches are pruned, and a hash of `(tick, y, velocity, mode flags, held)`
rejects states already explored. If nothing survives the whole horizon, the
longest-surviving timeline is used, so it fights to the end.

**Shadows are isolated.** While a shadow is being simulated, hooks block
anything that would change the real level: dual/mirror toggles, mode switches,
camera changes and touch triggers. A shadow's death is swallowed in
`destroyPlayer`.

**Portals.** Shadows cannot apply a gamemode switch to the level, so the search
horizon stops just before the next mode, dual or teleport portal. Pathfinder
re-plans once the real player has passed through it.

---

## Settings

| Setting | Default | Meaning |
|---|---|---|
| Enable Pathfinder | on | Master switch. |
| Practice Mode Only | off | Assist only in practice mode. |
| Safe Mode | on | Marks runs as test runs: no progress saved or submitted. |
| Look-ahead | 180 ticks | How far a plan must survive (240 ticks = 1 s). |
| Decision Step | 3 ticks | Input granularity after the first tick. |
| Time Budget | 2.0 ms | Max CPU per search. |
| Forced Re-plan Interval | 120 ticks | Periodic refresh to catch unmodelled effects. |
| Divergence Tolerance | 1.5 units | Drift that invalidates a plan. |
| Show Predicted Path | on | Draw the plan. |
| Show Status Label | on | Clear / Dodging / No Path + cost. |
| Verbose Logging | off | Per-search log lines. |

**Tuning tips**

- *Stutters?* Lower Time Budget or Look-ahead.
- *Dies in tight spam sections?* Raise Time Budget, lower Decision Step to 1 or 2.
- *Status flickers "No Path"?* The section may be impossible, or the budget is too small for it.
- *Constant re-planning in the log?* Moving objects or triggers are causing drift. Raise Divergence Tolerance.

---

## Building

Requirements: the [Geode SDK](https://docs.geode-sdk.org/getting-started/) and
CLI, with `GEODE_SDK` set.

```bash
geode build                       # desktop
geode build --platform android64  # Android64
```

Or push to GitHub. `.github/workflows/build.yml` builds Android64 and Windows
and uploads the `.geode` packages as artifacts.

Before building, edit `id`, `developer` and `repository` in `mod.json`.

### Project layout

```
mod.json            metadata + settings
src/Config.*        cached settings, live-updated
src/Shadow.*        shadow players + checkpoint slot pool
src/Planner.*       the search (DFS, hashing, deepening, path replay)
src/Overlay.*       predicted path + status label
src/main.cpp        hooks and the per-tick driver
```

---

## Known limitations

- **Not yet compiled or tested.** Field and function names were checked against
  the 2.2081 bindings, but behaviour is unverified. The `GameObjectType::*`
  names were written from memory.
- **No platformer mode.** Left/right movement is not modelled. Pathfinder
  disables itself there and logs a warning.
- **Mode portals end the horizon** instead of being simulated through. Plans
  are short right around them.
- **Triggers and moving objects** are not simulated. The divergence check and
  the forced re-plan timer are the safety net.
- **Orb / pad animations** may twitch slightly when a shadow touches them.
- **Mid-step input timing.** The real game queues touches with timestamps.
  Pathfinder injects through `handleButton` on the exact tick, so it does not
  use sub-tick timing.
- **`isHalfTick`** semantics of `processCommands` are not special-cased. If
  results look off on odd TPS settings, start there.
- **Shadow vs. real step order** may differ slightly from the real loop. If
  the log shows constant divergence on flat ground, adding the missing per-step
  calls (for example `postCollision`) to `Ctx::advance` is the fix.

## Roadmap

- Simulate through mode portals by snapshotting the relevant `GJGameState` fields
- Move-trigger / moving-platform prediction
- Platformer support
- Robustness mode: prefer plans that tolerate ±1 tick of input jitter
- Per-gamemode tuning profiles

---

## Fair-play note

Pathfinder is a bot in all but name. It is meant for practice, testing,
tool-assisted experimentation and fun. **Safe Mode keeps assisted runs from
being saved or submitted. Don't submit assisted runs as legitimate
completions.**

## License

MIT, see [LICENSE](LICENSE).
