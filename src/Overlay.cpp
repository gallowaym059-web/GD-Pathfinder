#include "Overlay.hpp"
#include "Config.hpp"

#include <cstdio>

using namespace geode::prelude;

namespace pf {
    void Overlay::init(PlayLayer* layer) {
        draw = CCDrawNode::create();
        layer->m_objectLayer->addChild(draw, 999);

        label = CCLabelBMFont::create("", "bigFont.fnt");
        label->setScale(0.32f);
        label->setAnchorPoint({0.f, 1.f});
        auto win = CCDirector::get()->getWinSize();
        label->setPosition({6.f, win.height - 6.f});
        label->setOpacity(190);
        layer->addChild(label, 1000);
    }

    void Overlay::clearPath() { if (draw) draw->clear(); }

    void Overlay::showPlan(Plan const& plan) {
        if (!draw) return;
        draw->clear();
        if (!cfg().showPath) return;
        ccColor4F hold{0.25f, 1.f, 0.45f, 0.95f};
        ccColor4F rel{1.f, 1.f, 1.f, 0.45f};
        for (size_t i = 1; i < plan.path.size(); ++i)
            draw->drawSegment(plan.path[i - 1], plan.path[i], 0.8f, plan.pathHold[i] ? hold : rel);
    }

    void Overlay::showStatus(Verdict v, int nodes, float ms) {
        if (!label) return;
        if (!cfg().showStatus) { label->setString(""); return; }
        char buf[96];
        char const* name = v == Verdict::Clear ? "Clear" : v == Verdict::Dodging ? "Dodging" : "No Path";
        std::snprintf(buf, sizeof buf, "Pathfinder: %s  (%d nodes, %.2f ms)", name, nodes, ms);
        label->setString(buf);
        label->setColor(v == Verdict::NoPath ? ccColor3B{255, 90, 90}
                      : v == Verdict::Dodging ? ccColor3B{255, 220, 90}
                      : ccColor3B{120, 255, 140});
    }
}
