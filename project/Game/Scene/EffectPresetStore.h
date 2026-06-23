#pragma once

#include <string>
#include <vector>
#include "ParticleManager.h"

class EffectPresetStore {
public:
    struct PresetNames {
        std::vector<std::string> all;
        std::vector<std::string> showcase;
        std::string status;
    };

    struct HitPreset {
        ParticleManager::HitEffectSettings settings{};
        bool showGpuSphere = true;
        bool mirrorSlash = false;
        bool includeInShowcase = true;
        int burstCount = 1;
        float burstRadius = 0.0f;
    };

    struct StormPreset {
        ParticleManager::StormEffectSettings settings{};
        bool includeInShowcase = true;
    };

    PresetNames LoadHitPresetNames(const std::string& path) const;
    PresetNames LoadStormPresetNames(const std::string& path, const std::string& defaultName) const;

    bool SaveHitPreset(const std::string& path, const std::string& name, const HitPreset& preset, std::string& status) const;
    bool LoadHitPreset(const std::string& path, const std::string& name, HitPreset& preset, std::string& status) const;

    bool SaveStormPreset(const std::string& path, const std::string& name, const StormPreset& preset, std::string& status) const;
    bool LoadStormPreset(const std::string& path, const std::string& defaultName, const std::string& name, StormPreset& preset, std::string& status) const;
};
