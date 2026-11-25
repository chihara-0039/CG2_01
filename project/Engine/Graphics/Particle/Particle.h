#pragma once
#include "MyMath.h"

enum class AdventureEffectKind {
    Step = 1,
    Jump,
    Land,
    Collect,
    Place,
    Undo,
    Key,
    Star
};

// パーティクル1つのデータ構造
struct Particle {
    enum class Type {
        Fall,
        Splash,
        Ring,
        Cylinder,
        Lightning,
        Firework,
        Adventure,
        Dust,
        Accent,
        StormCloud,
        StormRain,
        StormWind
    };
    Type type = Type::Fall;
    Transform transform; // 位置、回転、スケール
    Vector3 velocity;    // 速度
    Vector4 color;       // 色
    int adventure = 0;
    Vector3 origin{}, target{}, baseScale{};
    bool worldAligned = false;
    float shape = 0;
    float phase = 0;
    float initialAlpha = 1.0f;
    bool ambientCloud = false;
    float lifeTime; // 時間(現在)
    float maxTime;  // (最大)
};
