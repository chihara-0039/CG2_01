#pragma once

class MyGame;

class MyGameRenderer {
public:
    void Draw(MyGame& game);

private:
    void RenderScene(MyGame& game);
    void DrawSkybox(MyGame& game);
    bool IsPlayerHiddenByWall(const MyGame& game) const;
};
