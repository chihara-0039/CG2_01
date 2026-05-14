#include "StageMap.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <Windows.h>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

void StageMap::Initialize(int width, int height, int depth) {
    assert(width > 0);
    assert(height > 0);
    assert(depth > 0);

    width_ = width;
    height_ = height;
    depth_ = depth;

    cells_.resize(width_ * height_ * depth_);
    Clear();
}

void StageMap::Update(float deltaTime, float totalTime) {
    for (auto& cell : cells_) {
        if (cell.type == BlockType::CrumblingFloor) {
            // --- 崩れる処理 ---
            if (!cell.isHidden) {
                if (cell.isCrumbling) {
                    // プレイヤーが乗っているならタイマーを進める
                    cell.crumbleTimer += deltaTime;

                    if (cell.crumbleTimer >= 1.0f) {
                        cell.isHidden = true;
                        cell.isSolid = false;
                        cell.isCrumbling = false;
                    }
                }
                else {
                    // ★ここが重要：プレイヤーが降りたらタイマーを 0 に戻す
                    // これで「一瞬かすめただけ」なら赤くならずに済みます
                    cell.crumbleTimer -= deltaTime * 2.0f; // 徐々に回復させる（または 0.0f で即リセット）
                    if (cell.crumbleTimer < 0.0f) cell.crumbleTimer = 0.0f;
                }
            }

            // --- 復活処理 ---
            if (cell.isHidden) {
                cell.respawnTimer += deltaTime;
                if (cell.respawnTimer >= 3.0f) { // 3秒で復活
                    cell.isHidden = false;
                    cell.isSolid = true; // 判定復活
                    cell.respawnTimer = 0.0f;
                    // ★ changed = true; もここでは呼ばない！
                }
            }

            // --- 演出用の色・透明度計算 ---
            // 乗っている間は赤くする
            if (!cell.isHidden) {
                // crumbleTimerが0なら白、1.0に近づくほど赤くなる
                float r = cell.crumbleTimer / 1.0f;
                cell.colorG = 1.0f - r;
                cell.colorB = 1.0f - r;
            }
            cell.isCrumbling = false;
        }
    }
}

void StageMap::SaveToFile(const std::string& filename) {
    std::ofstream ofs(filename);
    if (!ofs.is_open()) return;

    ofs << width_ << " " << height_ << " " << depth_ << "\n";

    for (int y = 0; y < height_; ++y) {
        for (int z = 0; z < depth_; ++z) {
            for (int x = 0; x < width_; ++x) {
                const MapCell* cell = GetCell(x, y, z);
                if (cell->type == BlockType::None) continue; // 空ブロックは保存しない（ファイル軽量化）

                // ★回転角 (rotationX, rotationY) も保存する
                ofs << x << " " << y << " " << z << " "
                    << static_cast<int>(cell->type) << " "
                    << cell->rotationX << " " << cell->rotationY << "\n";

                if (cell->type == BlockType::Door)
                {
                    ofs << cell->doorTargetIndex.x << " "
                        << cell->doorTargetIndex.y << " "
                        << cell->doorTargetIndex.z << " ";
                }
            }
        }
    }
    ofs.close();
}

void StageMap::LoadFromFile(const std::string& filename) {
    std::ifstream ifs(filename);
    if (!ifs.is_open()) return;

    int w, h, d;
    ifs >> w >> h >> d;
    Initialize(w, h, d);

    int x, y, z, type;
    float rotX, rotY;
    // ★ 6つの値をセットで読み込む
    while (ifs >> x >> y >> z >> type >> rotX >> rotY) {
        SetBlock(x, y, z, static_cast<BlockType>(type));
        MapCell* cell = GetCell(x, y, z);
        if (cell) {
            cell->rotationX = rotX;
            cell->rotationY = rotY;
        }
        if (cell->type == BlockType::Door) {
            ifs >> cell->doorTargetIndex.x
                >> cell->doorTargetIndex.y
                >> cell->doorTargetIndex.z;
        }
        else {
            // ドア以外のブロックならワープ先は初期値にしておく
            cell->doorTargetIndex = { 0, 0, 0 };
        }
    }
    ifs.close();
}

void StageMap::Clear() {
    for (MapCell& cell : cells_) {
        cell.type = BlockType::None;
        cell.variant = 0;
        cell.isSolid = false;
    }
}

bool StageMap::IsInside(int x, int y, int z) const {
    return
        x >= 0 && x < width_ &&
        y >= 0 && y < height_ &&
        z >= 0 && z < depth_;
}

bool StageMap::IsInside(const Int3& index) const {
    return IsInside(index.x, index.y, index.z);
}

const MapCell* StageMap::GetCell(int x, int y, int z) const {
    if (!IsInside(x, y, z)) {
        return nullptr;
    }
    return &cells_[ToIndex(x, y, z)];
}

const MapCell* StageMap::GetCell(const Int3& index) const {
    return GetCell(index.x, index.y, index.z);
}

MapCell* StageMap::GetCell(int x, int y, int z) {
    if (!IsInside(x, y, z)) {
        return nullptr;
    }
    return &cells_[ToIndex(x, y, z)];
}

MapCell* StageMap::GetCell(const Int3& index) {
    return GetCell(index.x, index.y, index.z);
}

bool StageMap::SetBlock(int x, int y, int z, BlockType type, int variant) {
    if (!IsInside(x, y, z)) {
        return false;
    }

    cells_[ToIndex(x, y, z)] = MakeCell(type, variant);
    return true;
}

bool StageMap::SetBlock(const Int3& index, BlockType type, int variant) {
    return SetBlock(index.x, index.y, index.z, type, variant);
}

bool StageMap::RemoveBlock(int x, int y, int z) {
    if (!IsInside(x, y, z)) {
        return false;
    }

    cells_[ToIndex(x, y, z)] = MakeCell(BlockType::None, 0);
    return true;
}

bool StageMap::RemoveBlock(const Int3& index) {
    return RemoveBlock(index.x, index.y, index.z);
}

void StageMap::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::Text("Size: %d x %d x %d", width_, height_, depth_);

    // 固定位置のセルの情報など（デバッグ用）
    const MapCell* cell = GetCell(2, 1, 0);
    if (cell) {
        ImGui::Text("Cell(2,1,0) type = %d", static_cast<int>(cell->type));
        ImGui::Text("Cell(2,1,0) solid = %s", cell->isSolid ? "true" : "false");
    }
#endif
}

int StageMap::ToIndex(int x, int y, int z) const {
    return x + (z * width_) + (y * width_ * depth_);
}

MapCell StageMap::MakeCell(BlockType type, int variant) {
    MapCell cell{};
    cell.type = type;
    cell.variant = variant;

    switch (type) {
    case BlockType::None:
    cell.isSolid = false;
    break;

    case BlockType::Ground:
    case BlockType::Wall:
    case BlockType::Star:
    case BlockType::CrumblingFloor:
    cell.isSolid = true;
    break;

    case BlockType::BubblePickup:
    case BlockType::Goal:
    case BlockType::PlayerStart:
    case BlockType::Door:
    case BlockType::PSwitch:
    case BlockType::PBlock:
    cell.isSolid = false;
    break;

    default:
    cell.isSolid = false;
    break;
    }

    return cell;
}