#include "WeatherPresetManager.h"
#include <fstream>
#include <iostream>
#include <filesystem>
#include "json.hpp"

using json = nlohmann::json;

void WeatherPresetManager::LoadPresets() {
    presets_.clear();

    if (!std::filesystem::exists(filepath_)) {
        CreateDefaultPresetsIfEmpty();
        SavePresets();
        return;
    }

    std::ifstream file(filepath_);
    if (file.is_open()) {
        json j;
        try {
            file >> j;
            if (j.is_array()) {
                for (const auto& item : j) {
                    WeatherPreset preset;
                    preset.name = item.value("name", "Unknown");

                    preset.clearColor.x = item.value("clearColor_r", 0.1f);
                    preset.clearColor.y = item.value("clearColor_g", 0.25f);
                    preset.clearColor.z = item.value("clearColor_b", 0.5f);
                    preset.clearColor.w = item.value("clearColor_a", 1.0f);

                    preset.lightIntensity = item.value("lightIntensity", 1.0f);
                    preset.lightColor.x = item.value("lightColor_r", 1.0f);
                    preset.lightColor.y = item.value("lightColor_g", 1.0f);
                    preset.lightColor.z = item.value("lightColor_b", 1.0f);

                    preset.lightDirection.x = item.value("lightDirection_x", 0.5f);
                    preset.lightDirection.y = item.value("lightDirection_y", -1.0f);
                    preset.lightDirection.z = item.value("lightDirection_z", 0.5f);

                    preset.particleEnabled = item.value("particleEnabled", false);
                    preset.particleTexture = item.value("particleTexture", "Resources/UI/inventory/white.png");
                    preset.emitRate = item.value("emitRate", 100.0f);

                    preset.emitSize.x = item.value("emitSize_x", 40.0f);
                    preset.emitSize.y = item.value("emitSize_y", 20.0f);
                    preset.emitSize.z = item.value("emitSize_z", 40.0f);

                    preset.velocity.x = item.value("velocity_x", 0.0f);
                    preset.velocity.y = item.value("velocity_y", -5.0f);
                    preset.velocity.z = item.value("velocity_z", 0.0f);

                    preset.velocityRandom.x = item.value("velocityRandom_x", 1.0f);
                    preset.velocityRandom.y = item.value("velocityRandom_y", 0.5f);
                    preset.velocityRandom.z = item.value("velocityRandom_z", 1.0f);

                    preset.particleSize.x = item.value("particleSize_x", 0.2f);
                    preset.particleSize.y = item.value("particleSize_y", 0.2f);
                    preset.particleSize.z = item.value("particleSize_z", 0.2f);

                    preset.particleLife = item.value("particleLife", 4.0f);

                    preset.particleColor.x = item.value("particleColor_r", 1.0f);
                    preset.particleColor.y = item.value("particleColor_g", 1.0f);
                    preset.particleColor.z = item.value("particleColor_b", 1.0f);
                    preset.particleColor.w = item.value("particleColor_a", 1.0f);

                    presets_.push_back(preset);
                }
            }
        }
        catch (json::parse_error& e) {
            std::cerr << "JSON Parse error: " << e.what() << std::endl;
            CreateDefaultPresetsIfEmpty();
        }
    }
}

void WeatherPresetManager::SavePresets() {
    // ディレクトリが存在しなければ作成
    std::filesystem::path dir = std::filesystem::path(filepath_).parent_path();
    if (!std::filesystem::exists(dir)) {
        std::filesystem::create_directories(dir);
    }

    json j = json::array();
    for (const auto& preset : presets_) {
        json item;
        item["name"] = preset.name;
        item["clearColor_r"] = preset.clearColor.x;
        item["clearColor_g"] = preset.clearColor.y;
        item["clearColor_b"] = preset.clearColor.z;
        item["clearColor_a"] = preset.clearColor.w;
        
        item["lightIntensity"] = preset.lightIntensity;
        item["lightColor_r"] = preset.lightColor.x;
        item["lightColor_g"] = preset.lightColor.y;
        item["lightColor_b"] = preset.lightColor.z;

        item["lightDirection_x"] = preset.lightDirection.x;
        item["lightDirection_y"] = preset.lightDirection.y;
        item["lightDirection_z"] = preset.lightDirection.z;

        item["particleEnabled"] = preset.particleEnabled;
        item["particleTexture"] = preset.particleTexture;
        item["emitRate"] = preset.emitRate;
        item["emitSize_x"] = preset.emitSize.x;
        item["emitSize_y"] = preset.emitSize.y;
        item["emitSize_z"] = preset.emitSize.z;
        
        item["velocity_x"] = preset.velocity.x;
        item["velocity_y"] = preset.velocity.y;
        item["velocity_z"] = preset.velocity.z;

        item["velocityRandom_x"] = preset.velocityRandom.x;
        item["velocityRandom_y"] = preset.velocityRandom.y;
        item["velocityRandom_z"] = preset.velocityRandom.z;

        item["particleSize_x"] = preset.particleSize.x;
        item["particleSize_y"] = preset.particleSize.y;
        item["particleSize_z"] = preset.particleSize.z;

        item["particleLife"] = preset.particleLife;

        item["particleColor_r"] = preset.particleColor.x;
        item["particleColor_g"] = preset.particleColor.y;
        item["particleColor_b"] = preset.particleColor.z;
        item["particleColor_a"] = preset.particleColor.w;

        j.push_back(item);
    }

    std::ofstream file(filepath_);
    if (file.is_open()) {
        file << j.dump(4);
    }
}

WeatherPreset* WeatherPresetManager::GetPresetByName(const std::string& name) {
    for (auto& preset : presets_) {
        if (preset.name == name) {
            return &preset;
        }
    }
    return nullptr;
}

void WeatherPresetManager::CreateDefaultPresetsIfEmpty() {
    if (presets_.empty()) {
        {
            WeatherPreset sunny;
            sunny.name = "Sunny (Default)";
            sunny.clearColor = {0.1f, 0.4f, 0.8f, 1.0f}; // 青空
            sunny.lightIntensity = 1.0f;
            sunny.lightColor = {1.0f, 1.0f, 0.9f};
            sunny.lightDirection = {0.5f, -1.0f, 0.5f};
            sunny.particleEnabled = false;
            presets_.push_back(sunny);
        }
        {
            WeatherPreset rain;
            rain.name = "Heavy Rain";
            rain.clearColor = {0.2f, 0.2f, 0.25f, 1.0f}; // 暗い雲
            rain.lightIntensity = 0.6f;
            rain.lightColor = {0.8f, 0.8f, 0.9f};
            rain.lightDirection = {0.0f, -1.0f, 0.0f};
            rain.particleEnabled = true;
            // 雨っぽい設定
            rain.particleTexture = "Resources/UI/inventory/white.png";
            rain.emitRate = 300.0f;
            rain.emitSize = {60.0f, 2.0f, 60.0f}; // 天井付近から降らす
            rain.velocity = {0.0f, -20.0f, 0.0f}; // 高速で落下
            rain.velocityRandom = {1.0f, 2.0f, 1.0f};
            rain.particleSize = {0.05f, 0.5f, 0.05f}; // 細長い
            rain.particleColor = {0.7f, 0.8f, 1.0f, 0.6f};
            presets_.push_back(rain);
        }
        {
            WeatherPreset snow;
            snow.name = "Snowy";
            snow.clearColor = {0.8f, 0.85f, 0.9f, 1.0f}; // 白っぽい空
            snow.lightIntensity = 0.8f;
            snow.lightColor = {0.9f, 0.95f, 1.0f};
            snow.lightDirection = {0.5f, -1.0f, 0.2f};
            snow.particleEnabled = true;
            snow.particleTexture = "Resources/UI/inventory/white.png";
            snow.emitRate = 150.0f;
            snow.emitSize = {60.0f, 20.0f, 60.0f}; // 空間全体から発生
            snow.velocity = {-1.0f, -3.0f, 0.5f}; // ふわふわ斜めに落ちる
            snow.velocityRandom = {1.5f, 1.0f, 1.5f};
            snow.particleSize = {0.3f, 0.3f, 0.3f}; 
            snow.particleColor = {1.0f, 1.0f, 1.0f, 0.8f};
            presets_.push_back(snow);
        }
    }
}
