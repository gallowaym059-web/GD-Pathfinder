# Changelog

## v0.3.0
- Full rewrite into modules: `Config`, `Shadow`, `Planner`, `Overlay`, `main`.
- Plan reuse: the best input timeline is stored and replayed, so most ticks cost
  zero simulation. Re-search only on low runway, divergence, or a timer.
- Divergence detection compares the real player to the predicted path.
- Iterative deepening (1/3, 2/3, full horizon) so a usable answer always exists.
- Wall-clock time budget (ms) instead of a node count.
- Mode-portal / dual-portal / teleport-portal aware horizon: the search stops
  before a portal and re-plans after the real player has switched mode.
- Predicted-path overlay and status label.
- Practice-only option, platformer detection, checkpoint/reset handling.
- New README, about page, CI for Android64 and Windows.

## v0.2.0
- DFS over input timelines using game checkpoints for state cloning.
- Dual-mode support, global side-effect blocking for shadows.

## v0.1.0
- Initial prototype with fixed delay/hold plans.
