#include "MyGame.h"

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    MyGame* game = new MyGame();

    // 1. 初期化
    game->Initialize();

    // 2. メインループ
    while (game->IsRunning()) {
        game->Update();
        game->Draw();
    }

    // 3. 解放
    game->Finalize();
    delete game;

    return 0;
}