#include "MyGame.h"
#include "LifecycleTrace.h"
#include "D3DResourceLeakChecker.h"
#include <Windows.h>
#include <exception>
#include <memory>

namespace {

class GameFinalizeGuard {
  public:
    explicit GameFinalizeGuard(MyGame *game) : game_(game) {}

    // 例外や早期終了でもゲームの終了処理を実行する。
    ~GameFinalizeGuard() {
        if (game_ && active_) {
            game_->Finalize();
        }
    }

    void Dismiss() { active_ = false; }

  private:
    MyGame *game_ = nullptr;
    bool active_ = true;
};

} // namespace

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    LifecycleTrace::Mark("entry");
    D3DResourceLeakChecker leakChecker;
    std::unique_ptr<MyGame> game = std::make_unique<MyGame>();

    try {
        game->Initialize();
        LifecycleTrace::Mark("initialized");
    } catch (const std::exception &e) {
        MessageBoxA(nullptr, e.what(), "Initialization Error", MB_ICONERROR | MB_OK);
        return -1;
    } catch (...) {
        MessageBoxA(nullptr, "Unknown error during initialization.", "Initialization Error", MB_ICONERROR | MB_OK);
        return -1;
    }

    GameFinalizeGuard finalizeGuard(game.get());

    try {
        int frames = 0;
        char smoke[2]{};
        const bool lifecycleSmoke = GetEnvironmentVariableA("CG2_LIFECYCLE_SMOKE", smoke, 2) == 1 && smoke[0] == '1';
        while (game->IsRunning()) {
            game->Update();
            game->Draw();
            if (++frames == 1) {
                LifecycleTrace::Mark("first-frame");
            }
            if (lifecycleSmoke && frames == 3) {
                PostQuitMessage(0);
            }
        }
    } catch (const std::exception &e) {
        MessageBoxA(nullptr, e.what(), "Runtime Error", MB_ICONERROR | MB_OK);
        return -1;
    } catch (...) {
        MessageBoxA(nullptr, "Unknown error during runtime.", "Runtime Error", MB_ICONERROR | MB_OK);
        return -1;
    }

    LifecycleTrace::Mark("shutdown-start");
    game->Finalize();
    LifecycleTrace::Mark("shutdown-complete");
    finalizeGuard.Dismiss();
    game.reset();
    LifecycleTrace::Mark("game-destroyed");
    return 0;
}
