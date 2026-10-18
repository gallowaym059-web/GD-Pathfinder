// AI Pathfinder – hooks and the per-tick driver.
// See README.md for the architecture; Planner.cpp holds the search.
#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>

#include "Config.hpp"
#include "Shadow.hpp"
#include "Planner.hpp"
#include "Overlay.hpp"

#include <algorithm>
#include <limits>

using namespace geode::prelude;

namespace {
    struct Runtime {
        pf::Plan plan;
        pf::Overlay overlay;
        std::vector<float> portalX;   // objects that change mode: search must stop before them
        bool held = false;            // input state we last sent
        int sinceReplan = 0;
        bool warnedPlatformer = false;
    } rt;

    bool isModePortal(GameObjectType t) {
        switch (t) {
            case GameObjectType::CubePortal:
            case GameObjectType::ShipPortal:
            case GameObjectType::BallPortal:
            case GameObjectType::UfoPortal:
            case GameObjectType::WavePortal:
            case GameObjectType::RobotPortal:
            case GameObjectType::SpiderPortal:
            case GameObjectType::SwingPortal:
            case GameObjectType::DualPortal:
            case GameObjectType::SoloPortal:
            case GameObjectType::TeleportPortal:
                return true;
            default:
                return false;
        }
    }

    // Shadows do not apply mode switches to the level, so we never search *through*
    // one. We stop just before it and re-plan once the real player has switched.
    float nextCapX(float playerX) {
        auto it = std::lower_bound(rt.portalX.begin(), rt.portalX.end(), playerX - 30.f);
        if (it == rt.portalX.end()) return std::numeric_limits<float>::infinity();
        return std::max(*it - 20.f, playerX + 12.f);
    }

    void resetRuntime() {
        rt.plan = pf::Plan{};
        rt.held = false;
        rt.sinceReplan = 0;
        rt.overlay.clearPath();
    }
}

class $modify(PFPlayLayer, PlayLayer) {
    void destroyPlayer(PlayerObject* p, GameObject* o) {
        auto& g = pf::shadow();
        if (g.simming && g.owns(p)) { g.dead = true; return; }   // shadow died, real player is safe
        PlayLayer::destroyPlayer(p, o);
    }

    void setupHasCompleted() {
        PlayLayer::setupHasCompleted();
        pf::shadow().create(this);
        rt.overlay.init(this);
        resetRuntime();

        rt.portalX.clear();
        for (auto* obj : CCArrayExt<GameObject*>(m_objects))
            if (isModePortal(obj->m_objectType)) rt.portalX.push_back(obj->getPositionX());
        std::sort(rt.portalX.begin(), rt.portalX.end());

        if (pf::cfg().safeMode) m_isTestMode = true;
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        resetRuntime();
        if (pf::cfg().safeMode) m_isTestMode = true;
    }

    void loadFromCheckpoint(CheckpointObject* cp) {
        PlayLayer::loadFromCheckpoint(cp);
        resetRuntime();
    }

    void onQuit() {
        pf::shadow().destroy();
        rt.overlay.reset();
        PlayLayer::onQuit();
    }
};

// Shadows must never change global level state when they touch portals or triggers.
class $modify(PFBase, GJBaseGameLayer) {
    void toggleDualMode(GameObject* o, bool dual, PlayerObject* p, bool fx) {
        if (pf::shadow().simming) return;
        GJBaseGameLayer::toggleDualMode(o, dual, p, fx);
    }
    void toggleFlipped(bool flip, bool fx) {
        if (pf::shadow().simming) return;
        GJBaseGameLayer::toggleFlipped(flip, fx);
    }
    void playerWillSwitchMode(PlayerObject* p, GameObject* o) {
        if (pf::shadow().simming) return;
        GJBaseGameLayer::playerWillSwitchMode(p, o);
    }
    void updateDualGround(PlayerObject* p, int mode, bool instant, float dur) {
        if (pf::shadow().simming) return;
        GJBaseGameLayer::updateDualGround(p, mode, instant, dur);
    }
    void playerTouchedTrigger(PlayerObject* p, EffectGameObject* o) {
        if (pf::shadow().simming) return;
        GJBaseGameLayer::playerTouchedTrigger(p, o);
    }

    bool active(PlayLayer* pl) {
        auto& c = pf::cfg();
        if (!c.enabled || !pf::shadow().p1) return false;
        if (m_player1->m_isDead) return false;
        if (c.practiceOnly && !pl->m_isPracticeMode) return false;
        if (m_isPlatformer) {   // left/right movement is not modelled yet
            if (!rt.warnedPlatformer) {
                rt.warnedPlatformer = true;
                log::warn("Pathfinder: platformer levels are not supported yet");
            }
            return false;
        }
        return true;
    }

    void drive(float dt) {
        auto& c = pf::cfg();

        // 1. Has reality drifted away from the prediction?
        auto& plan = rt.plan;
        if (plan.valid && plan.tick > 0 && plan.tick <= plan.total()) {
            auto expect = plan.path[plan.tick - 1];
            if (ccpDistance(expect, m_player1->getPosition()) > c.divergence) plan.valid = false;
        }

        // 2. Re-plan only when needed; most ticks cost zero simulation.
        int minRunway = plan.capped ? 2 : std::max(8, c.horizon / 3);
        bool need = !plan.valid || plan.remaining() <= minRunway || ++rt.sinceReplan >= c.replanTicks;
        if (need) {
            pf::PlanRequest req{this, rt.held, dt, nextCapX(m_player1->getPositionX())};
            auto res = pf::planAhead(req);
            rt.plan = std::move(res.plan);
            rt.sinceReplan = 0;
            rt.overlay.showPlan(rt.plan);
            rt.overlay.showStatus(res.verdict, res.nodes, res.ms);
            if (c.verbose)
                log::debug("plan: {} steps, {} ticks, {} nodes, {:.2f} ms", rt.plan.steps.size(),
                           rt.plan.total(), res.nodes, res.ms);
        }

        // 3. Execute this tick's input.
        auto& p = rt.plan;
        bool want = (p.valid && p.remaining() > 0 && p.stepIdx < p.steps.size()) ? p.currentHold() : rt.held;
        if (want != rt.held) {
            rt.held = want;
            this->handleButton(want, 1, true);
        }
        if (p.valid) p.advance();
    }

    void processCommands(float dt, bool isHalfTick, bool isLastTick) {
        auto pl = PlayLayer::get();
        if (pl && static_cast<GJBaseGameLayer*>(pl) == this && active(pl)) drive(dt);
        GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
    }
};
