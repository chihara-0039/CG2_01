#pragma once

#include "MyMath.h"
#include <string>
#include <vector>

struct WeatherPreset {
    std::string name;

    // Environment/Lighting
    Vector4 clearColor = {0.1f, 0.25f, 0.5f, 1.0f};
    float lightIntensity = 1.0f;
    Vector3 lightColor = {1.0f, 1.0f, 1.0f};
    Vector3 lightDirection = {0.5f, -1.0f, 0.5f};

    // Particle
    bool particleEnabled = false;
    std::string particleTexture = "Resources/UI/inventory/white.png";
    float emitRate = 100.0f;
    Vector3 emitSize = {40.0f, 20.0f, 40.0f};
    Vector3 velocity = {0.0f, -5.0f, 0.0f};
    Vector3 velocityRandom = {1.0f, 0.5f, 1.0f};
    Vector3 particleSize = {0.2f, 0.2f, 0.2f};
    float particleLife = 4.0f;
    Vector4 particleColor = {1.0f, 1.0f, 1.0f, 1.0f};
};

class WeatherPresetManager {
public:
    static WeatherPresetManager& GetInstance() {
        static WeatherPresetManager instance;
        return instance;
    }

    void LoadPresets();
    void SavePresets();

    const std::vector<WeatherPreset>& GetPresets() const { return presets_; }
    std::vector<WeatherPreset>& GetPresets() { return presets_; }
    
    // プリセットを名前で検索
    WeatherPreset* GetPresetByName(const std::string& name);

    // デフォルトプリセットをいくつか追加する
    void CreateDefaultPresetsIfEmpty();

private:
    WeatherPresetManager() = default;
    ~WeatherPresetManager() = default;
    
    std::vector<WeatherPreset> presets_;
    const std::string filepath_ = "Resources/presets/weather_presets.json";
};
