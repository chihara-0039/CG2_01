#include "GameBgmController.h"

// シーン別BGMを準備し、初期値と必要なリソースを設定する。
void GameBgmController::Initialize() {
    if (initialized_) {
        return;
    }
    sound_.Initialize();
    gameBgmData_ = sound_.SoundLoadFile("Resources/Sound/gamePlay.mp3");
    state_ = State::Stopped;
    initialized_ = true;
}

// シーン別BGMが保持するオブジェクトとリソースを解放する。
void GameBgmController::Finalize() {
    if (!initialized_) {
        return;
    }
    sound_.BGMStop();
    sound_.Finalize();
    state_ = State::Stopped;
    initialized_ = false;
}

// シーンに合う曲を選び、再生と音量遷移を更新する。
void GameBgmController::Update(bool shouldPlayGameBgm) {
    if (!initialized_) {
        return;
    }
    const State nextState = shouldPlayGameBgm ? State::PlayingGame : State::Stopped;
    if (nextState == state_) {
        return;
    }
    sound_.BGMStop();
    state_ = nextState;
    if (state_ == State::PlayingGame) {
        sound_.BGMPlay(gameBgmData_, volume_);
    }
}
