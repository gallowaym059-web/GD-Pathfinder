#include "Shadow.hpp"

using namespace geode::prelude;

namespace pf {
    Shadow& shadow() { static Shadow s; return s; }

    static PlayerObject* makeShadowPlayer(GJBaseGameLayer* layer, bool second) {
        auto p = PlayerObject::create(1, 1, layer, layer->m_objectLayer, false);
        p->retain();
        p->setVisible(false);
        p->m_playEffects = false;
        p->m_isSecondPlayer = second;
        return p;
    }

    static PlayerCheckpoint* makeCheckpoint() {
        auto c = PlayerCheckpoint::create();
        c->retain();
        return c;
    }

    void Shadow::create(GJBaseGameLayer* layer) {
        destroy();
        p1 = makeShadowPlayer(layer, false);
        p2 = makeShadowPlayer(layer, true);
    }

    void Shadow::destroy() {
        if (p1) { p1->release(); p1 = nullptr; }
        if (p2) { p2->release(); p2 = nullptr; }
        for (auto c : cp1) c->release();
        for (auto c : cp2) c->release();
        cp1.clear();
        cp2.clear();
        simming = dead = false;
    }

    void Shadow::ensureDepth(size_t n) {
        while (cp1.size() < n) {
            cp1.push_back(makeCheckpoint());
            cp2.push_back(makeCheckpoint());
        }
    }
}
