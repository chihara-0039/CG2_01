#pragma once
#include "Player.h"
#include "Input.h"
#include "StageMap.h"
#include "../Game/Stage/RotationIsland.h"
#include <cstring>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <cmath>

// A separate Input instance drives CPU-only Player physics. Real device state is untouched.
class InputTestPlayback {
  public:
    // 実機の入力を変更せず、検証用マウスの移動とボタン状態を設定する。
    static void Mouse(Input &input, LONG x, LONG y, int button = -1) { input.ApplyPlaybackMouse(x, y, button); }
    // 前回入力を残し、検証用のキーとスティックを次のフレームの状態へ進める。
    static void Advance(Input &input, float x, float z, std::initializer_list<BYTE> keys = {}) {
        input.ApplyPlaybackFrame(x, z, keys);
    }
};

// 独立した入力とプレイヤーを使い、移動・梯子・回転・回収などの実処理を検証する。
inline bool VerifyExpeditionPlayerPhysics(const std::string &output) {
    std::ofstream report(output);
    report << "{\"tests\":[";
    bool all = true;
    bool first = true;
    auto tick = [](StageMap &map, Player &player, Input &input, float x, float z, std::initializer_list<BYTE> keys) {
        InputTestPlayback::Advance(input, x, z, keys);
        map.Update(1.0f / 60.0f, player.GetPosition());
        player.Update(&input, map, 0, Math::MakeIdentity4x4(), nullptr, true);
        return !player.ConsumeFailure() && player.GetPosition().y > -2.0f;
    };
    auto move = [&](StageMap &map, Player &player, Input &input, Vector3 goal, bool jump) {
        for (int frame = 0; frame < 150; ++frame) {
            const auto p = player.GetPosition();
            const float dx = goal.x - p.x, dz = goal.z - p.z;
            const float x = std::clamp(dx * 1.5f, -.95f, .95f), z = std::clamp(dz * 1.5f, -.95f, .95f);
            if (!tick(map, player, input, x, z,
                      jump && frame == 0 ? std::initializer_list<BYTE>{DIK_SPACE} : std::initializer_list<BYTE>{})) {
                return false;
            }
            const auto actual = player.GetPosition();
            if (std::abs(actual.x - goal.x) < .12f && std::abs(actual.z - goal.z) < .12f &&
                std::abs(actual.y - goal.y) < .1f && player.IsGrounded()) {
                return true;
            }
        }
        return false;
    };
    auto trial = [&](const char *name, const std::function<bool(StageMap &, Player &, Input &)> &action) {
        StageMap map;
        map.LoadFromFile("Resources/Stages/expeditionGrandTour.txt");
        Player player;
        player.SetRespawnPosition({6, 1.5f, 6});
        player.Respawn();
        Input script;
        const bool ok = action(map, player, script);
        all = all && ok;
        if (!first) {
            report << ",";
        }
        first = false;
        const auto p = player.GetPosition();
        report << "{\"name\":\"" << name << "\",\"success\":" << (ok ? "true" : "false") << ",\"end\":[" << p.x << ","
               << p.y << "," << p.z << "]}";
    };
    const auto coyoteStart = [&](StageMap &map, Player &p, Input &in) {
        map.Initialize(14, 8, 12);
        for (int x = 1; x <= 4; ++x) {
            for (int z = 3; z <= 7; ++z) {
                map.SetBlock(x, 1, z, BlockType::Ground);
            }
        }
        p.SetRespawnPosition({4, 1.5f, 5});
        p.Respawn();
        for (int f = 0; f < 4; ++f) {
            tick(map, p, in, 0, 0, {});
        }
        for (int f = 0; f < 30 && p.IsGrounded(); ++f) {
            tick(map, p, in, 1, 0, {});
        }
        return !p.IsGrounded() && p.GetPosition().x > 4.4f;
    };
    trial("coyote jump after leaving a ledge", [&](StageMap &map, Player &p, Input &in) {
        if (!coyoteStart(map, p, in)) {
            return false;
        }
        for (int f = 0; f < 3; ++f) {
            tick(map, p, in, 0, 0, {});
        }
        const float y = p.GetPosition().y;
        tick(map, p, in, 0, 0, {DIK_SPACE});
        return p.GetPosition().y > y + .15f && !p.IsGrounded();
    });
    trial("expired coyote window cannot jump", [&](StageMap &map, Player &p, Input &in) {
        if (!coyoteStart(map, p, in)) {
            return false;
        }
        for (int f = 0; f < 8; ++f) {
            tick(map, p, in, 0, 0, {});
        }
        const float y = p.GetPosition().y;
        tick(map, p, in, 0, 0, {DIK_SPACE});
        return p.GetPosition().y < y;
    });
    trial("coyote jump cannot be repeated in midair", [&](StageMap &map, Player &p, Input &in) {
        if (!coyoteStart(map, p, in)) {
            return false;
        }
        tick(map, p, in, 0, 0, {DIK_SPACE});
        tick(map, p, in, 0, 0, {});
        const float y = p.GetPosition().y;
        tick(map, p, in, 0, 0, {DIK_SPACE});
        return p.GetPosition().y - y < .18f;
    });
    trial("respawn clears coyote permission", [&](StageMap &map, Player &p, Input &in) {
        if (!coyoteStart(map, p, in)) {
            return false;
        }
        p.SetRespawnPosition({10, 3, 5});
        p.Respawn();
        tick(map, p, in, 0, 0, {});
        const float y = p.GetPosition().y;
        tick(map, p, in, 0, 0, {DIK_SPACE});
        return p.GetPosition().y < y;
    });
    trial("collectible keys auto collect and survive retry", [&](StageMap &map, Player &p, Input &in) {
        map.Initialize(8, 5, 8);
        for (int x = 0; x < 8; ++x) {
            for (int z = 0; z < 8; ++z) {
                map.SetBlock(x, 0, z, BlockType::Ground);
            }
        }
        map.SetBlock(3, 1, 3, BlockType::Key, 1);
        map.SetBlock(4, 1, 3, BlockType::Key, 1);
        p.SetRespawnPosition({3, .5f, 3});
        p.Respawn();
        if (!tick(map, p, in, 0, 0, {}) || p.GetCollectedLevelKeys() != 1) {
            return false;
        }
        p.SetPosition({4, .5f, 3});
        if (!tick(map, p, in, 0, 0, {}) || p.GetCollectedLevelKeys() != 2 || p.HasKey()) {
            return false;
        }
        p.Respawn(true);
        const bool kept = p.GetCollectedLevelKeys() == 2 && map.GetCell(3, 1, 3)->type == BlockType::None;
        p.Respawn();
        return kept && p.GetCollectedLevelKeys() == 0;
    });
    trial("rotation trial north route, restore and eastern goal", [&](StageMap &map, Player &p, Input &in) {
        if (!map.LoadFromFile("Resources/Stages/rotationTrial.txt")) {
            return false;
        }
        p.SetRespawnPosition({5, 1.5f, 11});
        p.Respawn();
        if (!move(map, p, in, {8, 1.5f, 12}, false)) {
            return false;
        }
        if (!RotationIsland::Apply(map, p.GetPosition(), false)) {
            return false;
        }
        if (!move(map, p, in, {9, 1.5f, 12}, false) || !move(map, p, in, {11, 1.5f, 12}, true) ||
            !move(map, p, in, {15, 1.5f, 12}, false) || !move(map, p, in, {15, 1.5f, 8}, false) ||
            !move(map, p, in, {15, 1.5f, 6}, true) || !move(map, p, in, {17, 1.5f, 4}, false)) {
            return false;
        }
        if (!move(map, p, in, {15, 1.5f, 6}, false) || !move(map, p, in, {15, 1.5f, 8}, true) ||
            !move(map, p, in, {15, 1.5f, 12}, false) || !move(map, p, in, {11, 1.5f, 12}, false) ||
            !move(map, p, in, {9, 1.5f, 12}, true)) {
            return false;
        }
        if (!RotationIsland::Apply(map, p.GetPosition(), true)) {
            return false;
        }
        return move(map, p, in, {11, 1.5f, 12}, true) && move(map, p, in, {19, 1.5f, 12}, false) &&
               move(map, p, in, {21, 1.5f, 12}, true) && move(map, p, in, {23, 1.5f, 12}, false) &&
               p.GetCollectedLevelKeys() == 3;
    });
    trial("campaign 01 bridge and first ladder to goal", [&](StageMap &map, Player &p, Input &in) {
        if (!map.LoadFromFile("Resources/Stages/beginnerTrail.txt")) {
            return false;
        }
        p.SetRespawnPosition({4, .5f, 5});
        p.Respawn();
        for (const Vector3 target :
             {Vector3{8, .5f, 6}, Vector3{8, .5f, 9}, Vector3{13, .5f, 9}, Vector3{14, .65f, 9}}) {
            if (!move(map, p, in, target, false)) {
                return false;
            }
        }
        for (int frame = 0; frame < 100 && (p.GetPosition().y < 2.49f || p.IsOnLadder()); ++frame) {
            if (!tick(map, p, in, .95f, 0, {})) {
                return false;
            }
        }
        return move(map, p, in, {16, 2.5f, 9}, false) && move(map, p, in, {18, 2.5f, 10}, false) &&
               move(map, p, in, {19, 2.5f, 14}, false);
    });
    trial("campaign 02 key and P rooms to goal", [&](StageMap &map, Player &p, Input &in) {
        if (!map.LoadFromFile("Resources/Stages/mechanismGardens.txt")) {
            return false;
        }
        p.SetRespawnPosition({7, 1.5f, 5});
        p.Respawn();
        if (!tick(map, p, in, 0, 0, {DIK_F}) || !p.HasKey()) {
            return false;
        }
        for (const Vector3 target : {Vector3{7, 1.5f, 12}, Vector3{7, 1.5f, 16}, Vector3{7, 1.5f, 10},
                                     Vector3{20, 1.5f, 10}, Vector3{20, 1.5f, 6}}) {
            if (!move(map, p, in, target, false)) {
                return false;
            }
        }
        if (!tick(map, p, in, 0, 0, {DIK_F}) || !map.IsPSwitchActive()) {
            return false;
        }
        for (const Vector3 target : {Vector3{24, 1.5f, 6}, Vector3{24, 1.5f, 16}, Vector3{24, 1.5f, 12},
                                     Vector3{28, 1.5f, 12}, Vector3{28, 1.5f, 20}}) {
            if (!move(map, p, in, target, false)) {
                return false;
            }
        }
        return !p.HasKey();
    });
    trial("campaign 03 stable crossing and summit ladder", [&](StageMap &map, Player &p, Input &in) {
        if (!map.LoadFromFile("Resources/Stages/skylineRelay.txt")) {
            return false;
        }
        p.SetRespawnPosition({6, 2.5f, 6});
        p.Respawn();
        for (const Vector3 target :
             {Vector3{10, 2.5f, 20}, Vector3{10, 2.5f, 8}, Vector3{22, 2.5f, 8}, Vector3{25, 2.5f, 12},
              Vector3{32, 2.5f, 12}, Vector3{32, 2.5f, 21}, Vector3{28, 2.5f, 21}, Vector3{28, 2.65f, 22}}) {
            if (!move(map, p, in, target, false)) {
                return false;
            }
        }
        for (int frame = 0; frame < 150 && (p.GetPosition().y < 5.49f || p.IsOnLadder()); ++frame) {
            if (!tick(map, p, in, 0, .95f, {})) {
                return false;
            }
        }
        return move(map, p, in, {28, 5.5f, 25}, false) && move(map, p, in, {28, 5.5f, 27}, false);
    });
    trial("no-build crumbling crossing", [&](StageMap &map, Player &p, Input &in) {
        p.SetRespawnPosition({16, 1.5f, 8});
        p.Respawn();
        for (int i = 0; i < 3; ++i) {
            tick(map, p, in, 0, 0, {});
        }
        for (int x : {18, 20, 22}) {
            if (!move(map, p, in, {static_cast<float>(x), 1.5f, 8}, true)) {
                return false;
            }
        }
        return move(map, p, in, {24, 1.5f, 8}, false);
    });
    trial("no-build west crossing", [&](StageMap &map, Player &p, Input &in) {
        p.SetRespawnPosition({23, 3.5f, 28});
        p.Respawn();
        for (int i = 0; i < 3; ++i) {
            tick(map, p, in, 0, 0, {});
        }
        if (!move(map, p, in, {22, 3.5f, 28}, false)) {
            return false;
        }
        for (int x : {20, 18, 16}) {
            if (!move(map, p, in, {static_cast<float>(x), 3.5f, 28}, true)) {
                return false;
            }
        }
        return move(map, p, in, {15, 3.5f, 28}, false);
    });
    // どのカメラ方向からも同じ上端へ入り、入力を保持したまま地面まで降りられる。
    for (int view = 0; view < 8; ++view) {
        trial(("camera-relative ladder entry " + std::to_string(view)).c_str(),
              [&](StageMap &map, Player &p, Input &in) {
                  map.Initialize(9, 8, 9);
                  for (int x = 0; x < 9; ++x) {
                      for (int z = 0; z < 9; ++z) {
                          map.SetBlock(x, 0, z, BlockType::Ground);
                      }
                  }
                  for (int y = 1; y <= 3; ++y) {
                      map.SetBlock(4, y, 4, BlockType::Ladder);
                  }
                  map.SetBlock(4, 3, 5, BlockType::Ground);
                  p.SetRespawnPosition({4, 3.5f, 5});
                  p.Respawn();
                  const float yaw = view * .7853981634f;
                  const float x = std::sin(yaw), z = -std::cos(yaw);
                  for (int frame = 0; frame < 100; ++frame) {
                      InputTestPlayback::Advance(in, x, z);
                      p.Update(&in, map, yaw, Math::MakeIdentity4x4(), nullptr, true);
                      const auto position = p.GetPosition();
                      if (frame == 0 && position.y >= 3.5f) {
                          return false;
                      }
                      if (position.y < .7f && position.z < 3.5f) {
                          return true;
                      }
                  }
                  return false;
              });
    }
    // 昇降中もカメラ基準で入力を解釈する。横向きでD登攀・A下降の実キー入力も含む。
    for (int view = 0; view < 8; ++view) {
        trial(("camera-relative climbing " + std::to_string(view)).c_str(), [&](StageMap &map, Player &p, Input &in) {
            map.Initialize(9, 8, 9);
            for (int y = 1; y <= 4; ++y) {
                map.SetBlock(4, y, 4, BlockType::Ladder);
            }
            map.SetBlock(4, 4, 5, BlockType::Ground);
            p.SetRespawnPosition({4, 1.3f, 4});
            p.Respawn();
            const float yaw = view * .7853981634f;
            for (int frame = 0; frame < 12; ++frame) {
                if (view == 6) {
                    InputTestPlayback::Advance(in, 0, 0, {DIK_D});
                } else {
                    InputTestPlayback::Advance(in, -std::sin(yaw), std::cos(yaw));
                }
                p.Update(&in, map, yaw, Math::MakeIdentity4x4(), nullptr, true);
            }
            const float raised = p.GetPosition().y;
            if (raised < 1.8f) {
                return false;
            }
            for (int frame = 0; frame < 12; ++frame) {
                if (view == 6) {
                    InputTestPlayback::Advance(in, 0, 0, {DIK_A});
                } else {
                    InputTestPlayback::Advance(in, std::sin(yaw), -std::cos(yaw));
                }
                p.Update(&in, map, yaw, Math::MakeIdentity4x4(), nullptr, true);
            }
            const auto end = p.GetPosition();
            return end.y < raised - .5f && std::abs(end.x - 4) < .01f && std::abs(end.z - 4) < .01f;
        });
    }
    trial("fixed ladder ascent", [&](StageMap &map, Player &p, Input &in) {
        p.SetRespawnPosition({29, 1.5f, 13});
        p.Respawn();
        return move(map, p, in, {29, 3.5f, 16}, false);
    });
    trial("fixed ladder descent to ground", [&](StageMap &map, Player &p, Input &in) {
        p.SetRespawnPosition({29, 3.5f, 15});
        p.Respawn();
        return move(map, p, in, {29, 1.5f, 13}, false);
    });
    trial("no-build timed crossing", [&](StageMap &map, Player &p, Input &in) {
        p.SetRespawnPosition({29, 3.5f, 16});
        p.Respawn();
        map.Update(1.6f, p.GetPosition());
        for (int i = 0; i < 3; ++i) {
            tick(map, p, in, 0, 0, {});
        }
        for (int z : {18, 20, 22}) {
            if (!move(map, p, in, {29, 3.5f, static_cast<float>(z)}, true)) {
                return false;
            }
        }
        return move(map, p, in, {29, 3.5f, 24}, false);
    });
    trial("moving ferry ride and dismount", [&](StageMap &map, Player &p, Input &in) {
        map.Update(4.71238898f, {25, 3.5f, 17});
        p.SetRespawnPosition({25, 3.5f, 17});
        p.Respawn();
        bool arrived = false;
        for (int frame = 0; frame < 240; ++frame) {
            if (!tick(map, p, in, 0, 0, {})) {
                return false;
            }
            if (p.GetPosition().z > 22.65f) {
                arrived = true;
                break;
            }
        }
        return arrived && move(map, p, in, {25, 3.5f, 24}, false);
    });
    trial("key interaction and gate traversal", [&](StageMap &map, Player &p, Input &in) {
        p.SetRespawnPosition({13, 3.5f, 27});
        p.Respawn();
        if (!tick(map, p, in, 0, 0, {DIK_F}) || !p.HasKey()) {
            return false;
        }
        if (!move(map, p, in, {13, 3.5f, 24}, false) || !move(map, p, in, {6, 3.5f, 24}, false)) {
            return false;
        }
        return move(map, p, in, {6, 3.5f, 28}, false) && map.GetCell(6, 4, 25)->type == BlockType::None;
    });
    trial("held key preserves a second world key", [&](StageMap &map, Player &p, Input &in) {
        p.SetRespawnPosition({13, 3.5f, 27});
        p.Respawn();
        p.SetHasKey(true);
        return tick(map, p, in, 0, 0, {DIK_F}) && p.HasKey() && map.GetCell(13, 4, 27)->type == BlockType::Key;
    });
    trial("ladder exit probes do not spend a key", [&](StageMap &map, Player &p, Input &in) {
        map.Initialize(8, 6, 8);
        map.SetBlock(3, 1, 3, BlockType::Ladder);
        map.SetBlock(3, 2, 3, BlockType::Ladder);
        map.SetBlock(3, 2, 4, BlockType::Wall);
        map.SetBlock(3, 3, 4, BlockType::KeyBlock);
        p.SetRespawnPosition({3, 2.49f, 3});
        p.Respawn();
        p.SetHasKey(true);
        for (int frame = 0; frame < 30; ++frame) {
            if (!tick(map, p, in, 0, .95f, {})) {
                return false;
            }
        }
        return p.HasKey() && map.GetCell(3, 3, 4)->type == BlockType::KeyBlock &&
               std::abs(p.GetPosition().z - 3) < .01f;
    });
    trial("blocked door exit preserves player and key", [&](StageMap &map, Player &p, Input &in) {
        map.SetBlock(6, 3, 12, BlockType::KeyBlock);
        p.SetRespawnPosition({5, 3.5f, 33});
        p.Respawn();
        p.SetHasKey(true);
        if (!tick(map, p, in, 0, 0, {DIK_F})) {
            return false;
        }
        return p.HasKey() && map.GetCell(6, 3, 12)->type == BlockType::KeyBlock &&
               std::abs(p.GetPosition().x - 5) < .1f && std::abs(p.GetPosition().z - 33) < .1f;
    });
    trial("retry retains removed key and can still open gate", [&](StageMap &map, Player &p, Input &in) {
        p.SetRespawnPosition({13, 3.5f, 27});
        p.Respawn();
        if (!tick(map, p, in, 0, 0, {DIK_F}) || !p.HasKey() || map.GetCell(13, 4, 27)->type != BlockType::None) {
            return false;
        }
        p.SetRespawnPosition({6, 3.5f, 24});
        p.Respawn(true);
        if (!p.HasKey()) {
            return false;
        }
        if (!move(map, p, in, {6, 3.5f, 28}, false) || p.HasKey()) {
            return false;
        }
        p.SetHasKey(true);
        p.Respawn();
        return !p.HasKey(); // Explicit new-stage/reset behaviour still clears the key.
    });
    trial("P-switch interaction and gate traversal", [&](StageMap &map, Player &p, Input &in) {
        p.SetRespawnPosition({12, 3.5f, 27});
        p.Respawn();
        if (!tick(map, p, in, 0, 0, {DIK_F}) || !map.IsPSwitchActive()) {
            return false;
        }
        return move(map, p, in, {12, 3.5f, 32}, false);
    });
    trial("opened P-block does not bypass an adjacent wall", [&](StageMap &map, Player &p, Input &in) {
        map.Initialize(8, 5, 8);
        for (int x = 0; x < 8; ++x) {
            for (int z = 0; z < 8; ++z) {
                map.SetBlock(x, 0, z, BlockType::Ground);
            }
        }
        map.SetBlock(3, 1, 3, BlockType::PBlock, 1);
        map.SetBlock(4, 1, 3, BlockType::Wall);
        map.SetBlock(4, 2, 3, BlockType::Wall);
        map.SetPSwitchActive(1);
        p.SetRespawnPosition({3, .5f, 3});
        p.Respawn();
        for (int frame = 0; frame < 40; ++frame) {
            if (!tick(map, p, in, .95f, 0, {}) || p.GetPosition().x > 3.151f) {
                return false;
            }
        }
        return true;
    });
    trial("ON-OFF interaction", [&](StageMap &map, Player &p, Input &in) {
        p.SetRespawnPosition({25, 3.5f, 31});
        p.Respawn();
        const bool before = map.IsOnState();
        return tick(map, p, in, 0, 0, {DIK_F}) && before != map.IsOnState();
    });
    trial("checkpoint retry stays under a low ceiling", [&](StageMap &map, Player &p, Input &in) {
        map.Initialize(8, 6, 8);
        for (int x = 1; x <= 6; ++x) {
            for (int z = 1; z <= 6; ++z) {
                map.SetBlock(x, 0, z, BlockType::Ground);
                map.SetBlock(x, 3, z, BlockType::Wall);
            }
        }
        map.SetBlock(3, 1, 3, BlockType::Checkpoint);
        p.SetRespawnPosition({3, .5f, 3});
        p.Respawn();
        if (!tick(map, p, in, 0, 0, {}) || std::abs(p.GetRespawnPosition().y - .5f) > .01f) {
            return false;
        }
        p.SetPosition({3, -6, 3});
        p.Respawn(true);
        return move(map, p, in, {5, .5f, 3}, false);
    });
    trial("first-person heading matches movement", [&](StageMap &map, Player &p, Input &in) {
        if (!tick(map, p, in, .95f, 0, {})) {
            return false;
        }
        return std::sin(p.GetFacingYaw()) > .99f && std::abs(std::cos(p.GetFacingYaw())) < .01f;
    });
    trial("door interaction", [&](StageMap &map, Player &p, Input &in) {
        p.SetRespawnPosition({5, 3.5f, 33});
        p.Respawn();
        if (!tick(map, p, in, 0, 0, {DIK_F})) {
            return false;
        }
        return std::abs(p.GetPosition().x - 6) < .1f && std::abs(p.GetPosition().z - 12) < .1f;
    });
    trial("fixed summit ladder", [&](StageMap &map, Player &p, Input &in) {
        p.SetRespawnPosition({17, 3.5f, 23});
        p.Respawn();
        if (!move(map, p, in, {18, 3.5f, 23}, false)) {
            return false;
        }
        bool onSummit = false;
        for (int i = 0; i < 80; ++i) {
            if (!tick(map, p, in, 0, -.95f, {})) {
                return false;
            }
            if (p.GetPosition().y > 6.4f && !p.IsOnLadder()) {
                onSummit = true;
                break;
            }
        }
        return onSummit && move(map, p, in, {19, 6.5f, 20}, false) && move(map, p, in, {19, 6.5f, 18}, false);
    });
    report
        << "],\"success\":" << (all ? "true" : "false")
        << ",\"method\":\"actual Player::Update and StageMap::Update with synthetic input; independent starts, not a continuous clear\"}";
    return all;
}
