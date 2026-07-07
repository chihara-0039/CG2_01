#pragma once

#include "MyMath.h"
#include <string>
#include <vector>

struct LevelObjectData {
    std::string type;
    std::string name;
    std::string fileName;
    Transform transform = {
        { 1.0f, 1.0f, 1.0f },
        { 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f }
    };
    std::vector<LevelObjectData> children;
};

struct LevelData {
    std::string name;
    std::vector<LevelObjectData> objects;
};

class LevelDataLoader {
public:
    // Loads a Blender-style level JSON file and converts it into engine-side level data.
    // The loader only parses data; actual Object3d creation is handled by scene/editor code.
    static bool Load(const std::string& filePath, LevelData& outLevelData, std::string* outStatus = nullptr);
};
