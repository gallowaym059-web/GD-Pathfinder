#include "Planner.hpp"
#include "Config.hpp"
#include "Shadow.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <unordered_set>

using namespace geode::prelude;

namespace pf {
    void Plan::advance() {
        ++tick;
        if (stepIdx < steps.size() && ++tickInStep >= steps[stepIdx].ticks) {
            ++stepIdx;
            tickInStep = 0;
        }
    }

    namespace {
        using Clock = std::chrono::steady_clock;
        enum class Adv { Alive, Dead, Cap };

        uint64_t mix(uint64_t h, uint64_t v) {
            h ^= v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
            return h;
        }

        struct Ctx {
            GJBaseGameLayer* L;
            Shadow& g;
            bool dual;
            float dt, capX;
            int seg;
            Clock::time_point deadline;
            int nodes = 0;
            bool hitCap = false;

            std::unordered_set<uint64_t> seen;
            std::vector<Step> path;
            std::vector<Step> bestPath;     // deepest survivor, used if nothing reaches the horizon
            int bestTicks = -1;

            bool timeUp() const { return Clock::now() >= deadline; }

            void load(size_t d) {
                g.p1->loadFromCheckpoint(g.cp1[d]);
                if (dual) g.p2->loadFromCheckpoint(g.cp2[d]);
            }
            void save(size_t d) {
                g.p1->saveToCheckpoint(g.cp1[d]);
                if (dual) g.p2->saveToCheckpoint(g.cp2[d]);
            }
            void setInput(bool down) {
                if (down) {
                    g.p1->pushButton(PlayerButton::Jump);
                    if (dual) g.p2->pushButton(PlayerButton::Jump);
                } else {
                    g.p1->releaseButton(PlayerButton::Jump);
                    if (dual) g.p2->releaseButton(PlayerButton::Jump);
                }
            }

            // Steps the shadows up to n ticks. `used` = ticks that were survived/executed.
            Adv advance(int n, int& used) {
                g.simming = true;
                g.dead = false;
                used = 0;
                for (int i = 0; i < n; ++i) {
                    g.p1->update(dt);
                    L->checkCollisions(g.p1, dt, false);
                    if (!g.dead && dual) {
                        g.p2->update(dt);
                        L->checkCollisions(g.p2, dt, false);
                    }
                    if (g.dead) { g.simming = false; return Adv::Dead; }
                    ++used;
                    if (g.p1->getPositionX() >= capX) { g.simming = false; return Adv::Cap; }
                }
                g.simming = false;
                return Adv::Alive;
            }

            uint64_t key(int ticksDone, bool held) {
                auto* p = g.p1;
                uint64_t h = 1469598103934665603ULL;
                h = mix(h, (uint64_t)ticksDone);
                h = mix(h, (uint64_t)(int64_t)std::llround(p->getPositionY() * 2.0));
                h = mix(h, (uint64_t)(int64_t)std::llround(p->m_yVelocity * 16.0));
                uint64_t flags = (uint64_t)p->m_isShip | ((uint64_t)p->m_isBall << 1) |
                    ((uint64_t)p->m_isBird << 2) | ((uint64_t)p->m_isDart << 3) |
                    ((uint64_t)p->m_isRobot << 4) | ((uint64_t)p->m_isSpider << 5) |
                    ((uint64_t)p->m_isSwing << 6) | ((uint64_t)p->m_isUpsideDown << 7) |
                    ((uint64_t)p->m_isOnGround << 8) | ((uint64_t)held << 9);
                h = mix(h, flags);
                if (dual) {
                    h = mix(h, (uint64_t)(int64_t)std::llround(g.p2->getPositionY() * 2.0));
                    h = mix(h, (uint64_t)(int64_t)std::llround(g.p2->m_yVelocity * 16.0));
                }
                return h;
            }

            void noteDeath(int ticksDone, int survivedInStep, bool act) {
                int total = ticksDone + survivedInStep;
                if (total > bestTicks) {
                    bestTicks = total;
                    bestPath = path;
                    if (survivedInStep > 0) bestPath.push_back({act, survivedInStep});
                }
            }

            // Prefers keeping the current input, so a change only happens when it is
            // forced -> the latest viable tick -> frame-perfect.
            bool dfs(size_t d, bool cur, int ticksDone, int maxTicks) {
                if (ticksDone >= maxTicks) return true;
                bool order[2] = {cur, !cur};
                for (bool act : order) {
                    if (timeUp()) return false;
                    ++nodes;
                    load(d);
                    setInput(act);
                    int n = std::min(d == 0 ? 1 : seg, maxTicks - ticksDone);
                    int used = 0;
                    auto r = advance(n, used);
                    if (r == Adv::Dead) { noteDeath(ticksDone, used, act); continue; }
                    path.push_back({act, used});
                    if (r == Adv::Cap) { hitCap = true; return true; }
                    if (!seen.insert(key(ticksDone + n, act)).second) { path.pop_back(); continue; }
                    save(d + 1);
                    if (dfs(d + 1, act, ticksDone + n, maxTicks)) return true;
                    path.pop_back();
                }
                return false;
            }
        };

        std::vector<Step> merged(std::vector<Step> const& in) {
            std::vector<Step> out;
            for (auto& s : in) {
                if (s.ticks <= 0) continue;
                if (!out.empty() && out.back().hold == s.hold) out.back().ticks += s.ticks;
                else out.push_back(s);
            }
            return out;
        }

        // Replays a plan tick by tick to record the predicted path.
        void trace(Ctx& c, Plan& plan, bool currentHeld) {
            c.load(0);
            c.g.simming = true;
            c.g.dead = false;
            bool stop = false;
            for (auto& s : plan.steps) {
                c.setInput(s.hold);
                for (int i = 0; i < s.ticks && !stop; ++i) {
                    c.g.p1->update(c.dt);
                    c.L->checkCollisions(c.g.p1, c.dt, false);
                    if (c.dual) {
                        c.g.p2->update(c.dt);
                        c.L->checkCollisions(c.g.p2, c.dt, false);
                    }
                    if (c.g.dead) { stop = true; break; }
                    plan.path.push_back(c.g.p1->getPosition());
                    plan.pathHold.push_back(s.hold);
                    if (c.g.p1->getPositionX() >= c.capX) { stop = true; break; }
                }
                if (stop) break;
            }
            c.g.simming = false;
            (void)currentHeld;
        }
    }

    PlanResult planAhead(PlanRequest const& req) {
        auto& config = cfg();
        auto& g = shadow();
        auto* L = req.layer;
        PlanResult res;

        bool dual = L->m_gameState.m_isDualMode && L->m_player2;
        int horizon = std::max(config.horizon, 20);
        int seg = std::max(config.segment, 1);

        auto t0 = Clock::now();
        Ctx c{L, g, dual, req.dt, req.capX, seg,
              t0 + std::chrono::microseconds((int64_t)(config.budgetMs * 1000.f))};

        g.ensureDepth((size_t)(horizon / seg) + 6);

        // Global state shadows might disturb; restored below.
        auto portal1 = L->m_gameState.m_lastActivatedPortal1;
        auto portal2 = L->m_gameState.m_lastActivatedPortal2;

        L->m_player1->saveToCheckpoint(g.cp1[0]);
        if (dual) L->m_player2->saveToCheckpoint(g.cp2[0]);

        // Iterative deepening: a shallow answer always exists before the deep one.
        int stages[3] = {horizon / 3, (horizon * 2) / 3, horizon};
        std::vector<Step> best;
        int bestStage = -1;
        for (int i = 0; i < 3; ++i) {
            c.seen.clear();
            c.path.clear();
            if (!c.dfs(0, req.currentlyHeld, 0, stages[i])) break;
            best = c.path;
            bestStage = i;
            if (c.hitCap) break;       // deeper stages would hit the same portal
        }

        bool found = bestStage >= 0;
        if (!found) best = c.bestPath;        // doomed: take the longest survivor
        if (best.empty()) best.push_back({req.currentlyHeld, 1});

        res.plan.steps = merged(best);
        res.plan.capped = c.hitCap;
        res.plan.complete = found && bestStage == 2;
        trace(c, res.plan, req.currentlyHeld);
        res.plan.valid = res.plan.total() > 0;

        L->m_gameState.m_lastActivatedPortal1 = portal1;
        L->m_gameState.m_lastActivatedPortal2 = portal2;

        bool changes = false;
        for (auto& s : res.plan.steps) if (s.hold != req.currentlyHeld) { changes = true; break; }
        res.verdict = !found ? Verdict::NoPath : (changes ? Verdict::Dodging : Verdict::Clear);
        res.nodes = c.nodes;
        res.ms = std::chrono::duration<float, std::milli>(Clock::now() - t0).count();
        return res;
    }
}
