#pragma once
#include <Geode/Geode.hpp>
#include <vector>

namespace pf {
    // Invisible clones of the real player(s) used for look-ahead simulation.
    // State is copied with the game's own PlayerCheckpoint, so every gamemode,
    // gravity, speed, size and orb/pad state is captured by GD itself.
    struct Shadow {
        PlayerObject* p1 = nullptr;
        PlayerObject* p2 = nullptr;
        std::vector<PlayerCheckpoint*> cp1, cp2;   // one slot per search depth
        bool simming = false;                      // true only while stepping a shadow
        bool dead = false;                         // set by the destroyPlayer hook

        void create(GJBaseGameLayer* layer);
        void destroy();
        void ensureDepth(size_t n);
        bool owns(PlayerObject* p) const { return p && (p == p1 || p == p2); }
    };

    Shadow& shadow();
}
