#pragma once

class IScene {
public:
    virtual ~IScene() = default;
    virtual void Initialize() = 0;
    virtual void Update() = 0;
    virtual void Draw() = 0;
    virtual void DrawShadow() {}

    // --- 追加：ImGuiなどのUIを表示するための関数 ---
    virtual void DrawUI() {}

    virtual bool IsFinished() const = 0;
};