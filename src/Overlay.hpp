#pragma once
#include "Planner.hpp"

namespace pf {
    // Predicted-path line + tiny status label.
    struct Overlay {
        cocos2d::CCDrawNode* draw = nullptr;
        cocos2d::CCLabelBMFont* label = nullptr;

        void init(PlayLayer* layer);
        void reset() { draw = nullptr; label = nullptr; }
        void showPlan(Plan const& plan);
        void showStatus(Verdict v, int nodes, float ms);
        void clearPath();
    };
}
