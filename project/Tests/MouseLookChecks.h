#pragma once
#include "ExpeditionPhysicsChecks.h"
#include "GameplayCameraController.h"
#include <fstream>
#include <vector>
#include <limits>

// 自分のウィンドウの四辺で視点を動かし、戻し処理とAlt/UI解除を実際のWin32座標で確認する。
inline bool VerifyMouseLook(WinApp &window, Player &player, const std::string &output) {
    // 再生APIだけで境界値とトリガー遷移を検証し、内部バッファへ直接アクセスしない。
    Input replay;
    replay.ApplyPlaybackFrame(2, std::numeric_limits<float>::quiet_NaN(), {DIK_W});
    bool playback = replay.GetGamePadState().leftStickX == 1 && replay.GetGamePadState().leftStickY == 0 &&
                    replay.TriggerKey(DIK_W);
    replay.ApplyPlaybackFrame(0, 0, {DIK_W});
    playback = playback && replay.PushKey(DIK_W) && !replay.TriggerKey(DIK_W);
    playback = replay.ApplyPlaybackMouse(10, 20, 0) && playback;
    playback = !replay.ApplyPlaybackMouse(100, 200, 9) && playback && replay.GetMouseState().x == 10 &&
               replay.GetMouseState().buttons[0];
    POINT saved{};
    GetCursorPos(&saved);
    SetForegroundWindow(window.GetHwnd());
    Input probe;
    probe.Initialize(&window);
    GameplayCameraController controller;
    controller.Initialize();
    Camera camera;
    camera.Update();
    const RECT region = probe.CameraMouseRegion();
    const POINT center{(region.left + region.right) / 2, (region.top + region.bottom) / 2};
    auto position = [&](POINT p) {
        ClientToScreen(window.GetHwnd(), &p);
        SetCursorPos(p.x, p.y);
    };
    auto current = [&]() {
        POINT p{};
        GetCursorPos(&p);
        ScreenToClient(window.GetHwnd(), &p);
        return p;
    };
    const bool foreground = GetForegroundWindow() == window.GetHwnd();
    const bool usable = region.right - region.left >= 64 && region.bottom - region.top >= 64;
    InputTestPlayback::Advance(probe, 0, 0);
    position(center);
    controller.Update(&probe, &camera, &window, &player);
    bool rotation = true, recenter = true;
    // 水平は複数回転に相当する量を加え、上下は本来の仰角制限内で検査する。
    for (int edge = 0; edge < 4; ++edge) {
        for (int frame = 0; frame < (edge < 2 ? 40 : 4); ++frame) {
            const POINT p = edge == 0   ? POINT{region.left + 1, center.y}
                            : edge == 1 ? POINT{region.right - 2, center.y}
                            : edge == 2 ? POINT{center.x, region.top + 1}
                                        : POINT{center.x, region.bottom - 2};
            position(p);
            const float angle = controller.GetAngle();
            const LONG dx = edge == 0 ? -20 : edge == 1 ? 20 : 0;
            InputTestPlayback::Mouse(probe, dx, edge == 2 ? -2 : edge == 3 ? 2 : 0);
            controller.Update(&probe, &camera, &window, &player);
            const auto after = current();
            recenter = recenter && std::abs(after.x - center.x) <= 1 && std::abs(after.y - center.y) <= 1;
            rotation = rotation && std::abs(controller.GetAngle() - angle - dx * .004f) < .0001f;
        }
    }
    InputTestPlayback::Advance(probe, 0, 0, {DIK_LMENU});
    position({region.left + 1, center.y});
    const float angle = controller.GetAngle();
    InputTestPlayback::Mouse(probe, 80, 0);
    controller.Update(&probe, &camera, &window, &player);
    const bool alt = std::abs(current().x - region.left - 1) <= 1 && controller.GetAngle() == angle;
    InputTestPlayback::Advance(probe, 0, 0);
    bool ui = true;
#if defined(USE_IMGUI) && !defined(NDEBUG)
    const bool oldCapture = ImGui::GetIO().WantCaptureMouse;
    ImGui::GetIO().WantCaptureMouse = true;
    InputTestPlayback::Mouse(probe, 80, 0);
    controller.Update(&probe, &camera, &window, &player);
    ui = controller.GetAngle() == angle && std::abs(current().x - region.left - 1) <= 1;
    ImGui::GetIO().WantCaptureMouse = oldCapture;
#endif
#if defined(NDEBUG) && !defined(DEVELOPMENT)
    constexpr bool release = true;
#else
    constexpr bool release = false;
#endif
    SendMessageW(window.GetHwnd(), WM_SETCURSOR, reinterpret_cast<WPARAM>(window.GetHwnd()),
                 MAKELPARAM(HTCLIENT, WM_MOUSEMOVE));
    const bool cursor = (GetCursor() != nullptr) ==
                        MouseLookPolicy::Visible(release, foreground, (GetAsyncKeyState(VK_MENU) & 0x8000) != 0);
    UINT count = 0;
    GetRegisteredRawInputDevices(nullptr, &count, sizeof(RAWINPUTDEVICE));
    std::vector<RAWINPUTDEVICE> devices(count);
    bool registered = false;
    if (count &&
        GetRegisteredRawInputDevices(devices.data(), &count, sizeof(RAWINPUTDEVICE)) != static_cast<UINT>(-1)) {
        for (const auto &device : devices) {
            registered =
                registered || (device.usUsagePage == 1 && device.usUsage == 2 && device.hwndTarget == window.GetHwnd());
        }
    }
    SetCursorPos(saved.x, saved.y);
    const bool ok = playback && foreground && usable && rotation && recenter && alt && ui && cursor && registered;
    std::ofstream(output) << std::boolalpha << "{\"success\":" << ok << ",\"foreground\":" << foreground
                          << ",\"fourEdgesRecenter\":" << recenter << ",\"continuousRotation\":" << rotation
                          << ",\"altReleases\":" << alt << ",\"uiBlocksCamera\":" << ui
                          << ",\"cursorPolicy\":" << cursor << ",\"rawInputRegistered\":" << registered
                          << ",\"playbackValidation\":" << playback << "}";
    return ok;
}
