#pragma once
#include <Geode/Geode.hpp>

namespace pf {
    // Cached copy of the mod settings. Updated live when the user edits them.
    struct Config {
        bool enabled = true;
        bool practiceOnly = false;
        bool safeMode = true;
        bool showPath = true;
        bool showStatus = true;
        bool verbose = false;
        int horizon = 180;
        int segment = 3;
        int replanTicks = 120;
        float budgetMs = 2.0f;
        float divergence = 1.5f;
    };

    Config& cfg();
    void initConfig();
}
