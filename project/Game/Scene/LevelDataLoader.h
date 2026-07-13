#pragma once

#include "MyMath.h"
#include <string>
#include <vector>

// レベルJSON内の1オブジェクト分のデータ。
struct LevelObjectData {
    std::string type;
    std::string name;
    std::string fileName;
    // Blender軸変換と親子階層を適用済みのワールド変換。
    // 利用側はJSON階層を意識せずObject3dへそのまま渡せる。
    Transform transform = {
        { 1.0f, 1.0f, 1.0f },
        { 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f }
    };
    std::vector<LevelObjectData> children;
};

// 1レベルファイル全体のデータ。
struct LevelData {
    std::string name;
    std::vector<LevelObjectData> objects;
};

// Blender由来のレベルJSONをエンジン側データへ変換するローダー。
class LevelDataLoader {
public:
    // JSONの読み込みと変換だけを行い、Object3d生成はシーン/エディタ側に委ねる。
    static bool Load(const std::string& filePath, LevelData& outLevelData, std::string* outStatus = nullptr);
};
