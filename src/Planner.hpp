#pragma once
#include <Geode/Geode.hpp>
#include <cstdint>
#include <limits>
#include <vector>

namespace pf {
    // One run of identical input: hold (true) or release (false) for N ticks.
    struct Step { bool hold; int ticks; };

    struct Plan {
        std::vector<Step> steps;
        std::vector<cocos2d::CCPoint> path;   // predicted P1 position after each tick
        std::vector<uint8_t> pathHold;        // input held on each predicted tick
        size_t stepIdx = 0;
        int tickInStep = 0;
        int tick = 0;                         // ticks already executed
        bool valid = false;
        bool capped = false;                  // ended at a portal, not at the horizon
        bool complete = false;                // reached the full requested horizon

        int total() const { return (int)path.size(); }
        int remaining() const { return total() - tick; }
        bool currentHold() const { return steps[stepIdx].hold; }
        void advance();
    };

    enum class Verdict { Clear, Dodging, NoPath };

    struct PlanRequest {
        GJBaseGameLayer* layer;
        bool currentlyHeld;
        float dt;      // real physics step, so timewarp / TPS changes are respected
        float capX;    // search stops here (next mode portal), or +inf
    };

    struct PlanResult {
        Plan plan;
        Verdict verdict = Verdict::Clear;
        int nodes = 0;
        float ms = 0.f;
    };

    PlanResult planAhead(PlanRequest const& req);
}
