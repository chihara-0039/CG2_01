#include "StageMap.h"
#include "../Engine/Base/MouseLookPolicy.h"
#include "../Engine/Graphics/Particle/AdventureParticlePath.h"
#include "../Game/Stage/RotationIsland.h"
#include "../Engine/Utility/CameraVisibility.h"
#include "../Game/Stage/StagePSwitchVisualController.h"
#include "../Game/Stage/PlacementPreviewState.h"
#include "../Game/Player/LadderTraversal.h"
#include "../Game/UI/InventoryNavigation.h"
#include "../Game/Scene/JournalResultTimeline.h"
#include "../Game/Scene/StarGetTimeline.h"
#include <cassert>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <chrono>
#include <utility>
#include <set>
#include <queue>

// マップ保存・拡張・回転・選択などの回帰テストを実行する。
int main() {
    // 表示ポリシー・画面四辺・移動量の累積と消費を、OSのカーソル位置と切り離して確認する。
    assert(MouseLookPolicy::Visible(false, true, false));
    assert(!MouseLookPolicy::Visible(true, true, false));
    assert(MouseLookPolicy::Visible(true, true, true));
    assert(MouseLookPolicy::Visible(true, false, false));
    const auto area = MouseLookPolicy::Region(1920, 1080, true);
    assert(area.left == 288 && area.top == 38 && area.right == 1536);
    assert(!MouseLookPolicy::Contains(area, area.left - 1, 100));
    const long cx = (area.left + area.right) / 2, cy = (area.top + area.bottom) / 2;
    assert(!MouseLookPolicy::NearEdge(area, cx, cy));
    assert(MouseLookPolicy::NearEdge(area, area.left, cy));
    assert(MouseLookPolicy::NearEdge(area, area.right - 1, cy));
    assert(MouseLookPolicy::NearEdge(area, cx, area.top));
    assert(MouseLookPolicy::NearEdge(area, cx, area.bottom - 1));
    MouseLookPolicy::RelativeMotion motion;
    for (int i = 0; i < 100; ++i) {
        motion.Add(12, -4);
    }
    motion.AddWheel(120);
    const auto delta = motion.Consume(true);
    assert(delta.x == 1200 && delta.y == -400 && delta.wheel == 120 && delta.received);
    assert(!motion.Consume(true).received);
    motion.Add(500, 500);
    assert(!motion.Consume(false).received && !motion.Consume(true).received);
    std::cout
        << "PASS: cursor policy, viewport edges and relative input survive cursor limits and reset after focus loss\n";

    // 異なるフレーム間隔でも吸い込みの端点が一致し、途中で不正な座標を作らない。
    for (int fps : {30, 60, 144}) {
        const Vector3 start{3, 2, -1}, target{-2, 4, 5};
        const auto first = AdventureParticlePath::Sample(start, target, 0, 1);
        const auto last = AdventureParticlePath::Sample(start, target, 1, 1);
        assert(std::abs(first.x - start.x) < .001f && std::abs(last.x - target.x) < .001f);
        assert(std::abs(last.y - target.y) < .001f && std::abs(last.z - target.z) < .001f);
        for (int i = 0; i <= fps; ++i) {
            const auto p = AdventureParticlePath::Sample(start, target, static_cast<float>(i) / fps, 1);
            assert(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z));
            assert(p.y >= 2 && p.y <= 4.51f);
        }
    }
    std::cout << "PASS: collection spiral endpoints and bounded motion at 30/60/144 FPS\n";
    // 組立・取消の端点と時間刻みへの依存を確認し、取消直後に袋へ飛ばないことも保証する。
    for (int fps : {30, 60, 144}) {
        const Vector3 start{3, 2, -1}, target{-2, 4, 5};
        for (bool undo : {false, true}) {
            const auto sample = [&](float t) {
                return undo ? AdventureParticlePath::Undo(start, target, t, .7f)
                            : AdventureParticlePath::Assemble(start, target, t);
            };
            for (const float t : {-.5f, 0.0f, 1.0f, 1.5f}) {
                const auto p = sample(t);
                const auto expected = t <= 0 ? start : target;
                assert(std::abs(p.x - expected.x) < .001f && std::abs(p.y - expected.y) < .001f &&
                       std::abs(p.z - expected.z) < .001f);
            }
            for (int i = 0; i <= fps; ++i) {
                const auto p = sample(static_cast<float>(i) / fps);
                assert(std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z));
                assert(p.x >= -2.66f && p.x <= 3.66f && p.y >= 2 && p.y <= 4.61f && p.z >= -1.66f && p.z <= 5.66f);
            }
        }
        const auto loosened = AdventureParticlePath::Undo(start, target, .25f, .7f);
        assert(std::hypot(loosened.x - start.x, loosened.z - start.z) < .66f);
    }
    std::cout << "PASS: assembly and undo endpoints, bounds and release phase at 30/60/144 FPS\n";

    for (int fps : {30, 60, 144}) {
        for (int i = 0; i <= fps * 4; ++i) {
            const auto frame = StarGet::Evaluate(static_cast<float>(i) / fps);
            assert(std::isfinite(frame.scale) && frame.scale > 0);
            assert(frame.lift >= 0 && frame.lift <= 1 && frame.hop >= -.0001f && frame.hop <= .301f);
        }
    }
    assert(StarGet::Evaluate(0).caption == 0);
    assert(StarGet::Evaluate(1).lift == 1 && StarGet::Evaluate(1).spin == 0);
    assert(StarGet::Evaluate(StarGet::kDuration).hop < .001f);
    std::cout << "PASS: star reception, jump landing and held pose have finite bounded transforms at 30/60/144 FPS\n";
    for (const auto *filename : {"rotationBridge.txt", "rotationRelay.txt"}) {
        StageMap course;
        assert(course.LoadFromFile(std::string("project/Resources/Stages/") + filename));
        const auto regions = course.GetRotationRegions();
        assert(!regions.empty());
        assert(!course.AddRotationRegion(regions.front()));
        assert(!course.AddRotationRegion({0, 0, 4, {0, 2, 8}}));
        assert(!course.UpdateRotationRegion(0, {0, 0, 4, {0, 2, 8}}));
        assert(course.GetRotationRegions()[0].x == regions[0].x);
        assert(course.UpdateRotationRegion(0, regions[0]));
        const auto footprint = [&](int startX, int targetX) {
            std::queue<int> pending;
            std::set<int> seen;
            pending.push(startX);
            seen.insert(startX);
            while (!pending.empty()) {
                const int x = pending.front();
                pending.pop();
                if (x == targetX) {
                    return true;
                }
                for (const int step : {-2, -1, 1, 2}) {
                    const auto *cell = course.GetCell(x + step, 1, 12);
                    if (cell && cell->isSolid && seen.insert(x + step).second) {
                        pending.push(x + step);
                    }
                }
            }
            return false;
        };
        if (regions.size() == 3) {
            assert(!footprint(5, 53));
            for (const auto &r : regions) {
                assert(!RotationIsland::Apply(course, {static_cast<float>(r.x + 4), 1.5f, 12}, false, r));
                assert(RotationIsland::Apply(course, {static_cast<float>(r.control.x), 1.5f, 12}, false, r));
            }
            assert(footprint(5, 53)); // Traversable with one-cell gaps, no placed blocks.
        } else {
            assert(footprint(5, 25));
            const auto &r = regions.front();
            assert(RotationIsland::Apply(course, {8, 1.5f, 12}, false, r));
            assert(course.GetCell(15, 1, 8)->isSolid);
            assert(RotationIsland::Apply(course, {8, 1.5f, 12}, true, r));
            assert(footprint(5, 25));
        }
        const auto saved = std::filesystem::temp_directory_path() / "CG2_rotation_course_roundtrip.txt";
        course.SaveToFile(saved.string());
        StageMap restored;
        assert(restored.LoadFromFile(saved.string()));
        assert(restored.GetRotationRegions().size() == regions.size());
        assert(restored.GetRotationRegions()[0].alternate.x == regions[0].alternate.x);
        assert(restored.Expand(restored.GetWidth() + 3, 10, 29));
        assert(restored.GetRotationRegions().size() == regions.size());
        std::filesystem::remove(saved);
    }
    std::cout
        << "PASS: editable rotation metadata roundtrip/expansion, overlap rejection, three independent bridges and no-placement route topology\n";
    for (const int fps : {30, 60, 144}) {
        int impacts = 0;
        for (int i = 1; i <= fps * 4; ++i) {
            const float before = static_cast<float>(i - 1) / fps;
            const float after = static_cast<float>(i) / fps;
            if (JournalResult::Crossed(before, after, JournalResult::kImpact)) {
                ++impacts;
            }
            const auto frame = JournalResult::Evaluate(after);
            assert(frame.open >= 0 && frame.open <= 1);
            assert(frame.ink == (after >= JournalResult::kImpact));
            assert(frame.ready == (after >= JournalResult::kReady));
        }
        assert(impacts == 1);
    }
    assert(JournalResult::Evaluate(1.6f).open == 1);
    assert(std::abs(JournalResult::Evaluate(JournalResult::kImpact).stampZ + .36f) < .001f);
    assert(!JournalResult::Evaluate(JournalResult::kReady).hand);
    for (const int fps : {30, 60, 144}) {
        JournalResult::Inspection book;
        assert(!book.Toggle(false) && book.Open() == 1);
        assert(book.Toggle(true));
        for (int i = 0; i < fps; ++i) {
            book.Advance(1.0f / fps);
        }
        assert(book.Open() == 0);
        assert(book.Toggle(true));
        book.Advance(.3f);
        const float halfway = book.Open();
        assert(halfway > 0 && halfway < 1);
        assert(book.Toggle(true) && book.Open() == halfway);
        book.Advance(-1);
        assert(book.Open() == halfway);
        book.Advance(1);
        assert(book.Open() == 0);
        assert(book.Toggle(true));
        book.Advance(1);
        assert(book.Open() == 1 && JournalResult::Evaluate(20).ink);
    }
    std::cout
        << "PASS: journal inspection waits for stamp, closes/reopens and reverses continuously at 30/60/144 FPS\n";
    std::cout
        << "PASS: journal opens before stamp, ink persists, impact fires once at 30/60/144 FPS, controls wait for withdrawal\n";
    // A pulled-back camera and a maximum-width stage must keep every corner inside far.
    for (const Vector3 camera : {Vector3{20, 150, -180}, Vector3{600, 400, -500}, Vector3{6, 12, -18}}) {
        for (const Vector3 maximum : {Vector3{40, 12, 40}, Vector3{256, 16, 256}}) {
            const Vector3 minimum{-0.5f, -0.5f, -0.5f};
            const float farClip = CameraVisibility::RequiredFarClip(camera, minimum, maximum);
            for (float x : {minimum.x, maximum.x}) {
                for (float y : {minimum.y, maximum.y}) {
                    for (float z : {minimum.z, maximum.z}) {
                        const float dx = x - camera.x, dy = y - camera.y, dz = z - camera.z;
                        assert(std::sqrt(dx * dx + dy * dy + dz * dz) < farClip);
                    }
                }
            }
        }
    }
    {
        StageMap rotor;
        assert(rotor.LoadFromFile("project/Resources/Stages/rotationTrial.txt"));
        const Vector3 operatorPosition{8, 1.5f, 12};
        assert(rotor.GetCell(15, 1, 8)->type == BlockType::None);
        assert(!RotationIsland::Apply(rotor, {15, 1.5f, 12}, false));
        assert(RotationIsland::Apply(rotor, operatorPosition, false));
        assert(rotor.GetCell(15, 1, 8)->type != BlockType::None);
        assert(RotationIsland::Apply(rotor, operatorPosition, true));
        assert(rotor.GetCell(15, 1, 8)->type == BlockType::None);
        rotor.RemoveBlock(15, 2, 12);
        for (int turn = 0; turn < 4; ++turn) {
            assert(RotationIsland::Apply(rotor, operatorPosition, false));
        }
        assert(rotor.GetCell(15, 2, 12)->type == BlockType::None);
        assert(rotor.GetCell(16, 2, 4)->type == BlockType::BubblePickup);
        std::cout << "PASS: rotation safety, inverse, four-turn restoration, collected bubble stays removed\n";
    }
    StageMap map;
    map.Initialize(8, 4, 8);
    map.SetBlock(7, 3, 7, BlockType::Door, 9);
    map.GetCell(7, 3, 7)->doorTargetIndex = {2, 1, 3};
    map.GetCell(7, 3, 7)->rotationY = 1.57f;
    map.SetWeatherPresetName("Snowy");
    map.SetVisualStyle("Miniature");
    map.GetCustomPart(1)->name = "KEPT PART";
    assert(map.Expand(128, 32, 128));
    assert(map.GetWidth() == 128 && map.GetHeight() == 32 && map.GetDepth() == 128);
    const auto *cell = map.GetCell(7, 3, 7);
    assert(cell->type == BlockType::Door && cell->variant == 9);
    assert(cell->doorTargetIndex.z == 3 && cell->rotationY == 1.57f);
    assert(map.GetCell(127, 31, 127)->type == BlockType::None);
    assert(map.GetWeatherPresetName() == "Snowy" && map.GetCustomPart(1)->name == "KEPT PART");
    assert(map.GetVisualStyle() == "Miniature");
    assert(!map.Expand(64, 32, 128));
    assert(!map.Expand(256, 256, 256));
    assert(!map.Expand(-1, 4, 8));
    assert(map.GetWidth() == 128 && map.GetCell(7, 3, 7)->variant == 9);
    const auto testDirectory =
        std::filesystem::temp_directory_path() /
        ("cg2-editor-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    assert(std::filesystem::create_directory(testDirectory));
    const auto path = testDirectory / "stage.txt";
    map.SaveToFile(path.string());
    StageMap loaded;
    loaded.LoadFromFile(path.string());
    assert(loaded.GetWidth() == 128 && loaded.GetHeight() == 32 && loaded.GetDepth() == 128);
    assert(loaded.GetCell(7, 3, 7)->type == BlockType::Door);
    assert(loaded.GetCustomPart(1)->name == "KEPT PART");
    assert(loaded.GetVisualStyle() == "Miniature");
    {
        std::ofstream invalid(path);
        invalid << "2147483647 2147483647 2147483647\n";
    }
    loaded.LoadFromFile(path.string());
    assert(loaded.GetWidth() == 128 && loaded.GetCell(7, 3, 7)->type == BlockType::Door);
    std::filesystem::remove(path);
    std::filesystem::remove(testDirectory);
    // Real grid collider: test climbing onto a landing and returning from it in all rotations.
    for (int quarter = 0; quarter < 4; ++quarter) {
        StageMap ladderMap;
        ladderMap.Initialize(7, 8, 7);
        const float angle = quarter * 1.57079632679f;
        const int dx = static_cast<int>(std::round(std::sin(angle)));
        const int dz = static_cast<int>(std::round(std::cos(angle)));
        for (int y = 0; y <= 2; ++y) {
            ladderMap.SetBlock(3, y, 3, BlockType::Ladder);
            ladderMap.GetCell(3, y, 3)->rotationY = angle;
        }
        ladderMap.SetBlock(3 + dx, 2, 3 + dz, BlockType::Ground);
        auto collision = [&](const Vector3 &pos) {
            for (int y = static_cast<int>(std::floor(pos.y + .5f));
                 y <= static_cast<int>(std::floor(pos.y + 1.6f + .5f - .0001f)); ++y) {
                for (int z = static_cast<int>(std::floor(pos.z - .35f + .5f));
                     z <= static_cast<int>(std::floor(pos.z + .35f + .5f - .0001f)); ++z) {
                    for (int x = static_cast<int>(std::floor(pos.x - .35f + .5f));
                         x <= static_cast<int>(std::floor(pos.x + .35f + .5f - .0001f)); ++x) {
                        const auto *cell = ladderMap.GetCell(x, y, z);
                        if (cell && cell->isSolid) {
                            return true;
                        }
                    }
                }
            }
            return false;
        };
        Vector3 exit{}, entry{};
        assert(LadderTraversal::TryExit(ladderMap, 3, 1, 3, {3, 2.4f, 3}, collision, exit));
        assert(exit.x == 3 + dx && exit.z == 3 + dz && exit.y == 2.5f);
        assert(LadderTraversal::TryEnterFromTop(ladderMap, exit, collision, entry, angle));
        assert(entry.x == 3 && entry.z == 3 && entry.y < 2.5f);
        // 梯子の4方向とカメラ16方向で、向かう入力のみが上端を掴むことを確認する。
        for (int view = 0; view < 16; ++view) {
            const float yaw = view * .3926990817f;
            const auto up =
                LadderTraversal::ResolveMovement(ladderMap, 3, 1, 3, yaw, dx * std::cos(yaw) - dz * std::sin(yaw),
                                                 dx * std::sin(yaw) + dz * std::cos(yaw));
            assert(std::abs(up.vertical - 1) < .001f);
            const auto down =
                LadderTraversal::ResolveMovement(ladderMap, 3, 1, 3, yaw, -dx * std::cos(yaw) + dz * std::sin(yaw),
                                                 -dx * std::sin(yaw) - dz * std::cos(yaw));
            assert(std::abs(down.vertical + 1) < .001f);
            const auto side =
                LadderTraversal::ResolveMovement(ladderMap, 3, 1, 3, yaw, dz * std::cos(yaw) + dx * std::sin(yaw),
                                                 dz * std::sin(yaw) - dx * std::cos(yaw));
            assert(side.vertical == 0 && std::hypot(side.side.x, side.side.z) > .99f);
            const float inputX = -dx * std::cos(yaw) + dz * std::sin(yaw);
            const float inputZ = -dx * std::sin(yaw) - dz * std::cos(yaw);
            assert(LadderTraversal::TryEnterFromTop(ladderMap, exit, collision, entry, yaw, inputX, inputZ));
            assert(!LadderTraversal::TryEnterFromTop(ladderMap, exit, collision, entry, yaw, -inputX, -inputZ));
            assert(!LadderTraversal::TryEnterFromTop(ladderMap, exit, collision, entry, yaw, 0, 0));
        }

        assert(!LadderTraversal::TryEnterFromTop(ladderMap, exit, collision, entry, angle + 3.14159265f));
        assert(!LadderTraversal::TryExit(ladderMap, 3, 0, 3, {3, 1.4f, 3}, collision, exit));
        ladderMap.SetBlock(3 + dx, 3, 3 + dz, BlockType::Wall);
        assert(!LadderTraversal::TryExit(ladderMap, 3, 2, 3, {3, 2.4f, 3}, collision, exit));
        ladderMap.SetBlock(3 + dx, 3, 3 + dz, BlockType::None);
        ladderMap.SetBlock(3, 4, 3, BlockType::Wall);
        assert(!LadderTraversal::TryEnterFromTop(
            ladderMap, {static_cast<float>(3 + dx), 2.5f, static_cast<float>(3 + dz)}, collision, entry, angle));
        // A bottom ladder over a floor: downward input must leave the ladder column.
        ladderMap.Initialize(7, 8, 7);
        for (int y = 1; y <= 3; ++y) {
            ladderMap.SetBlock(3, y, 3, BlockType::Ladder);
            ladderMap.GetCell(3, y, 3)->rotationY = angle;
        }
        for (int x = 2; x <= 4; ++x) {
            for (int z = 2; z <= 4; ++z) {
                ladderMap.SetBlock(x, 0, z, BlockType::Ground);
            }
        }
        Vector3 bottomExit{};
        assert(LadderTraversal::TryBottomExit(ladderMap, 3, 2, 3, {3, .59f, 3}, angle, collision, bottomExit));
        assert(bottomExit.y == .5f && !collision(bottomExit));
        assert(bottomExit.x != 3 || bottomExit.z != 3);
        assert(!LadderTraversal::TryBottomExit(ladderMap, 3, 2, 3, {3, 1.2f, 3}, angle, collision, bottomExit));
        for (const auto &offset : {Vector3{1, 0, 0}, Vector3{-1, 0, 0}, Vector3{0, 0, 1}, Vector3{0, 0, -1}}) {
            ladderMap.SetBlock(3 + static_cast<int>(offset.x), 1, 3 + static_cast<int>(offset.z), BlockType::Wall);
        }
        assert(!LadderTraversal::TryBottomExit(ladderMap, 3, 1, 3, {3, .59f, 3}, angle, collision, bottomExit));
        // Ground level can overlap the lowest ladder cell's height (not just sit below it).
        ladderMap.Initialize(7, 8, 7);
        ladderMap.SetBlock(3, 0, 3, BlockType::Ladder);
        for (int x = 2; x <= 4; ++x) {
            for (int z = 2; z <= 4; ++z) {
                if (x != 3 || z != 3) {
                    ladderMap.SetBlock(x, 0, z, BlockType::Ground);
                }
            }
        }
        assert(LadderTraversal::TryBottomExit(ladderMap, 3, 0, 3, {3, .59f, 3}, angle, collision, bottomExit));
        assert(bottomExit.y == .5f && !collision(bottomExit));
        ladderMap.Initialize(7, 8, 7);
        ladderMap.SetBlock(3, 1, 3, BlockType::Ladder);
        assert(!LadderTraversal::TryBottomExit(ladderMap, 3, 1, 3, {3, .59f, 3}, angle, collision, bottomExit));
    }
    // Screen-right/left must stay consistent even when the camera looks from behind the ladder.
    for (int eighth = 0; eighth < 8; ++eighth) {
        const float yaw = eighth * .78539816339f;
        const auto right = LadderTraversal::SideOffset(1, yaw, .12f);
        const auto left = LadderTraversal::SideOffset(-1, yaw, .12f);
        assert(right.x * std::cos(yaw) - right.z * std::sin(yaw) > .119f);
        assert(left.x * std::cos(yaw) - left.z * std::sin(yaw) < -.119f);
        assert(std::abs(right.x + left.x) < .00001f && std::abs(right.z + left.z) < .00001f);
    }
    std::cout << "PASS: camera-relative ladder strafe; bottom dismount; blocked/unsupported exit rejected\n";
    assert(!LadderTraversal::ClearPath(Vector3{0, 0, 0}, Vector3{1, 0, 0},
                                       [](const Vector3 &p) { return p.x > .4f && p.x < .6f; }));
    std::cout << "PASS: ladder top exit/descent in four rotations; blocked landing/ceiling and swept wall rejected\n";
    StageMap movingCollision;
    movingCollision.Initialize(7, 8, 7);
    movingCollision.SetBlock(3, 3, 3, BlockType::MovingFloor, 10 | (10 << 8) | (10 << 16));
    movingCollision.Update(0, {0, 0, 0});
    assert(!movingCollision.GetIntersectingMovingFloor(3, 3.5f, 3, .35f, .8f, .35f));
    assert(movingCollision.GetIntersectingMovingFloor(3, 3.49f, 3, .35f, .8f, .35f));
    assert(movingCollision.GetIntersectingMovingFloor(3, 3.45f, 3, .28f, .05f, .28f));
    std::cout << "PASS: moving-floor collider matches the rendered cell centre and foot contact\n";
    StageMap previewMap;
    previewMap.Initialize(8, 8, 8);
    const auto key = PlacementPreviewState::Make(previewMap, {2, 2, 2}, BlockType::Wall, 3, 0, 1, true, {1, 1, 1});
    auto snapshot = [&](Int3 cursor, int validity, float angle) {
        return PlacementPreviewState::Make(previewMap, cursor, BlockType::Wall, 3, angle, validity, true, {1, 1, 1});
    };
    assert(key == snapshot({2, 2, 2}, 1, 0));
    assert(!(key == snapshot({2, 3, 2}, 1, 0)));
    assert(!(key == snapshot({2, 2, 2}, 2, 0)));
    assert(!(key == snapshot({2, 2, 2}, 1, 1.5707963f)));
    const auto originalType = previewMap.GetCustomPart(3)->cells[2][2][2].type;
    previewMap.GetCustomPart(3)->cells[2][2][2].type =
        originalType == BlockType::None ? BlockType::Ladder : BlockType::None;
    assert(!(key == snapshot({2, 2, 2}, 1, 0)));
    previewMap.GetCustomPart(3)->cells[2][2][2].type = originalType;
    previewMap.GetCustomPart(3)->colorR += .1f;
    assert(!(key == snapshot({2, 2, 2}, 1, 0)));
    std::cout << "PASS: preview cache invalidates for cursor, rotation, validity, edited shape and color\n";
    StageMap doors;
    doors.Initialize(10, 4, 10);
    for (const auto pos : {Int3{1, 1, 1}, Int3{2, 1, 1}, Int3{7, 1, 7}, Int3{8, 1, 7}}) {
        doors.SetBlock(pos, BlockType::Door, 0);
    }
    doors.GetCell(1, 1, 1)->doorTargetIndex = {8, 1, 7};
    doors.GetCell(8, 1, 7)->doorTargetIndex = {1, 1, 1};
    doors.GetCell(2, 1, 1)->doorTargetIndex = {7, 1, 7};
    doors.GetCell(7, 1, 7)->doorTargetIndex = {2, 1, 1};
    assert(doors.FindPairedDoor(1, 1, 1).x == 8 && doors.FindPairedDoor(7, 1, 7).x == 2);
    doors.GetCell(1, 1, 1)->doorTargetIndex = {1, 1, 1};
    assert(doors.FindPairedDoor(1, 1, 1).x == 1);
    doors.Initialize(10, 4, 10);
    doors.SetBlock(1, 1, 1, BlockType::Door, 3);
    doors.SetBlock(8, 1, 7, BlockType::Door, 3);
    assert(doors.FindPairedDoor(1, 1, 1).x == 8);
    doors.SetBlock(5, 1, 5, BlockType::Door, 3);
    assert(doors.FindPairedDoor(1, 1, 1).x == 1);
    assert(doors.FindPairedDoor(-1, 0, 0).x == -1);
    StageMap switchGroups;
    switchGroups.Initialize(8, 4, 8);
    switchGroups.SetBlock(2, 1, 2, BlockType::PBlock, 1);
    switchGroups.SetBlock(3, 1, 2, BlockType::PBlock, 2);
    switchGroups.SetBlock(4, 1, 2, BlockType::PBlockAppears, 1);
    switchGroups.SetPSwitchActive(1);
    assert(!switchGroups.GetCell(2, 1, 2)->isSolid && switchGroups.GetCell(3, 1, 2)->isSolid &&
           switchGroups.GetCell(4, 1, 2)->isSolid);
    assert(StagePSwitchVisualController::ShouldHide(switchGroups, {2, 1, 2}));
    assert(!StagePSwitchVisualController::ShouldHide(switchGroups, {3, 1, 2}));
    assert(!StagePSwitchVisualController::ShouldHide(switchGroups, {4, 1, 2}));
    StageMap largeGate;
    largeGate.Initialize(66, 16, 64);
    for (int y = 0; y < 16; ++y) {
        for (int z = 0; z < 64; ++z) {
            for (int x = 0; x < 64; ++x) {
                auto *cell = largeGate.GetCell(x, y, z);
                cell->type = BlockType::KeyBlock;
                cell->isSolid = true;
            }
        }
    }
    largeGate.GetCell(65, 0, 0)->type = BlockType::KeyBlock;
    largeGate.GetCell(65, 0, 0)->isSolid = true;
    largeGate.RemoveConnectedKeyBlocks(0, 0, 0);
    for (int y = 0; y < 16; ++y) {
        for (int z = 0; z < 64; ++z) {
            for (int x = 0; x < 64; ++x) {
                assert(largeGate.GetCell(x, y, z)->type == BlockType::None && !largeGate.GetCell(x, y, z)->isSolid);
            }
        }
    }
    assert(largeGate.GetCell(65, 0, 0)->type == BlockType::KeyBlock);
    largeGate.RemoveConnectedKeyBlocks(-1, 0, 0);
    std::cout << "PASS: explicit/legacy/ambiguous door pairs, independent P groups, 65536-cell key gate\n";
    std::filesystem::create_directory(testDirectory);
    StageMap protectedMap;
    protectedMap.Initialize(8, 4, 8);
    protectedMap.SetBlock(2, 1, 2, BlockType::Wall, 9);
    protectedMap.SetVisualStyle("Miniature");
    protectedMap.SetWeatherPresetName("Snowy");
    protectedMap.ToggleOnState();
    protectedMap.GetCustomPart(2)->name = "PRESERVED";
    const std::vector<std::string> badFiles = {"4 4 4\n1 0 1 1 0 0 0\nbogus\n", "4 4 4\nPART 1\n",
                                               "4 4 4\nPARTCELL 1 4 0 0 2\n",   "4 4 4\n1 1 1 8 0 0 0\n",
                                               "4 4 4\n9 0 0 1 0 0 0\n",        "4 4 4\n0 0 0 999 0 0 0\n",
                                               "4 4 4\nENVIRONMENT 1 2\n",      "4 4 4\nVISUAL_STYLE invalid\n",
                                               "4 4 4\nPRESET invalid\n"};
    for (const auto &contents : badFiles) {
        {
            std::ofstream file(path);
            file << contents;
        }
        assert(!protectedMap.LoadFromFile(path.string()));
        assert(protectedMap.GetWidth() == 8 && protectedMap.GetCell(2, 1, 2)->variant == 9);
        assert(protectedMap.GetVisualStyle() == "Miniature" && protectedMap.GetWeatherPresetName() == "Snowy" &&
               !protectedMap.IsOnState());
        assert(protectedMap.GetCustomPart(2)->name == "PRESERVED");
    }
    assert(!protectedMap.LoadFromFile((testDirectory / "missing.txt").string()));
    {
        std::ofstream file(path);
        file << "4 4 4\n# legacy cell without variant\n1 0 1 1 0 0\n";
    }
    assert(protectedMap.LoadFromFile(path.string()) && protectedMap.GetWidth() == 4 &&
           protectedMap.GetCell(1, 0, 1)->type == BlockType::Ground);
    std::filesystem::remove(path);
    std::filesystem::remove(testDirectory);
    {
        std::ifstream sequence("project/Resources/Stages/sequence.txt");
        assert(sequence);
        std::string name;
        while (std::getline(sequence, name)) {
            if (!name.empty() && name.back() == '\r') {
                name.pop_back();
            }
            if (name.empty()) {
                continue;
            }
            StageMap campaign;
            assert(campaign.LoadFromFile("project/Resources/Stages/" + name));
        }
    }
    std::cout << "PASS: failed stage loads preserve all live data; legacy cells and campaign still load\n";
    using InventoryNavigation::Move;
    assert(Move(0, 0, 0, 1) == -1);
    assert(Move(0, 5, 0, 1) == 2);
    assert(Move(2, 5, 0, -1) == 0);
    assert(Move(0, 5, 1, 0) == 1);
    assert(Move(1, 5, 1, 0) == 1);
    assert(Move(3, 5, 0, 1) == 3);
    for (int count = 1; count <= 11; ++count) {
        for (int index = 0; index < count; ++index) {
            for (const auto direction : {std::pair{-1, 0}, {1, 0}, {0, -1}, {0, 1}}) {
                const int result = Move(index, count, direction.first, direction.second);
                assert(result >= 0 && result < count);
            }
        }
    }
    std::cout
        << "PASS: expansion preserves cells, door links, parts, weather, save/load; invalid sizes and shrinking rejected; two-column selection boundaries\n";
}
