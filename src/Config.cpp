#include "Config.hpp"

using namespace geode::prelude;

namespace pf {
    Config& cfg() { static Config c; return c; }

    namespace {
        void bindBool(char const* key, bool& ref) {
            ref = Mod::get()->getSettingValue<bool>(key);
            listenForSettingChanges<bool>(key, [&ref](bool v) { ref = v; });
        }
        void bindInt(char const* key, int& ref) {
            ref = (int)Mod::get()->getSettingValue<int64_t>(key);
            listenForSettingChanges<int64_t>(key, [&ref](int64_t v) { ref = (int)v; });
        }
        void bindFloat(char const* key, float& ref) {
            ref = (float)Mod::get()->getSettingValue<double>(key);
            listenForSettingChanges<double>(key, [&ref](double v) { ref = (float)v; });
        }
    }

    void initConfig() {
        auto& c = cfg();
        bindBool("enabled", c.enabled);
        bindBool("practice-only", c.practiceOnly);
        bindBool("safe-mode", c.safeMode);
        bindBool("show-path", c.showPath);
        bindBool("show-status", c.showStatus);
        bindBool("verbose", c.verbose);
        bindInt("horizon", c.horizon);
        bindInt("segment", c.segment);
        bindInt("replan-ticks", c.replanTicks);
        bindFloat("budget-ms", c.budgetMs);
        bindFloat("divergence", c.divergence);
    }
}

$execute { pf::initConfig(); }
