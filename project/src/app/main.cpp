#include "MyGame.h"
#include "D3DResourceLeakChecker.h"
#include <memory>
#include <Windows.h>
#include <exception>

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    D3DResourceLeakChecker leakChecker;
    std::unique_ptr<MyGame> game = std::make_unique<MyGame>();
    try {
        // 初期化
        game->Initialize();
    }
    catch (const std::exception& e) {
        MessageBoxA(nullptr, e.what(), "Initialization Error", MB_ICONERROR | MB_OK);
        return -1;
    }
    catch (...) {
        MessageBoxA(nullptr, "Unknown error during initialization.", "Initialization Error", MB_ICONERROR | MB_OK);
        return -1;
    }
    // メインループ
    while (game->IsRunning()) {
        game->Update();
        game->Draw();
    }
    // 解放処理
    game->Finalize();
    return 0;
}