#include "StageMap.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
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

    // 5つのカスタムブロックパーツスロットをデフォルト値で初期化
    customParts_.resize(5);
    for (int i = 0; i < 5; ++i) {
        customParts_[i].id = i + 1;
        customParts_[i].baseType = BlockType::Wall;
        customParts_[i].colorR = 1.0f;
        customParts_[i].colorG = 1.0f;
        customParts_[i].colorB = 1.0f;

        // 全セルを None に初期化
        for (int y = 0; y < 3; ++y) {
            for (int z = 0; z < 3; ++z) {
                for (int x = 0; x < 3; ++x) {
                    customParts_[i].cells[y][z][x].type = BlockType::None;
                }
            }
        }
    }

    // 楽しい複合プリセット形状の設定！
    // Slot 1: L-SHIELD (L字の壁足場パーツ)
    customParts_[0].name = "L-SHIELD";
    customParts_[0].colorR = 0.9f; customParts_[0].colorG = 0.3f; customParts_[0].colorB = 0.3f; // スタイリッシュ赤
    customParts_[0].cells[0][0][0].type = BlockType::Wall;
    customParts_[0].cells[0][0][1].type = BlockType::Wall;
    customParts_[0].cells[0][0][2].type = BlockType::Wall;
    customParts_[0].cells[1][0][2].type = BlockType::Wall;
    customParts_[0].cells[2][0][2].type = BlockType::Wall;

    // Slot 2: T-BRIDGE (T字足場パーツ)
    customParts_[1].name = "T-BRIDGE";
    customParts_[1].colorR = 0.9f; customParts_[1].colorG = 0.8f; customParts_[1].colorB = 0.2f; // ゴールド黄色
    customParts_[1].cells[0][0][1].type = BlockType::Wall;
    customParts_[1].cells[1][0][1].type = BlockType::Wall;
    customParts_[1].cells[2][0][0].type = BlockType::Wall;
    customParts_[1].cells[2][0][1].type = BlockType::Wall;
    customParts_[1].cells[2][0][2].type = BlockType::Wall;

    // Slot 3: LADDER-WALL (ハシゴ付き壁)
    customParts_[2].name = "LADDER-WALL";
    customParts_[2].colorR = 0.2f; customParts_[2].colorG = 0.7f; customParts_[2].colorB = 0.9f; // ライトブルー
    customParts_[2].cells[0][0][1].type = BlockType::Wall;
    customParts_[2].cells[1][0][1].type = BlockType::Wall;
    customParts_[2].cells[2][0][1].type = BlockType::Wall;
    customParts_[2].cells[0][0][0].type = BlockType::Ladder;
    customParts_[2].cells[1][0][0].type = BlockType::Ladder;
    customParts_[2].cells[2][0][0].type = BlockType::Ladder;

    // Slot 4 & 5: 空白のカスタム用スロット
    customParts_[3].name = "MY PART A";
    customParts_[3].colorR = 0.4f; customParts_[3].colorG = 0.9f; customParts_[3].colorB = 0.4f; // ライムグリーン
    customParts_[3].cells[0][0][0].type = BlockType::Wall; // 1マスだけ

    customParts_[4].name = "MY PART B";
    customParts_[4].colorR = 0.8f; customParts_[4].colorG = 0.4f; customParts_[4].colorB = 0.9f; // パープル
    customParts_[4].cells[0][0][0].type = BlockType::Ladder; // 1マスだけ
}

void StageMap::Update(float deltaTime, float totalTime, const Vector3& playerPos) 
{
    for (int y = 0; y < height_; ++y) {
        for (int z = 0; z < depth_; ++z) {
            for (int x = 0; x < width_; ++x) {
                MapCell& cell = cells_[ToIndex(x, y, z)];

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
                            // プレイヤーが降りたらタイマーを回復させる
                            cell.crumbleTimer -= deltaTime * 2.0f;
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
                        }
                    }

                    // --- 演出用の色・透明度計算 ---
                    if (!cell.isHidden) {
                        float r = cell.crumbleTimer / 1.0f;
                        cell.colorG = 1.0f - r;
                        cell.colorB = 1.0f - r;
                    }
                    cell.isCrumbling = false;
                }

                if (cell.type == BlockType::MovingFloor) {
                    float moveSpeed = 1.0f;
                    cell.moveTimer += deltaTime * moveSpeed;

                    float t = (std::sin(cell.moveTimer) + 1.0f) / 2.0f;

                    // 前フレームのオフセットを記憶
                    float oldX = cell.currentOffsetX;
                    float oldY = cell.currentOffsetY;
                    float oldZ = cell.currentOffsetZ;

                    // 新しいオフセットを計算
                    cell.currentOffsetX = static_cast<float>(cell.moveOffset.x) * t;
                    cell.currentOffsetY = static_cast<float>(cell.moveOffset.y) * t;
                    cell.currentOffsetZ = static_cast<float>(cell.moveOffset.z) * t;

                    // 1フレーム分の移動量を記録
                    cell.deltaOffsetX = cell.currentOffsetX - oldX;
                    cell.deltaOffsetY = cell.currentOffsetY - oldY;
                    cell.deltaOffsetZ = cell.currentOffsetZ - oldZ;
                }

                // --- 敵1 (EnemyWalker): 左右往復 (X軸) ---
                if (cell.type == BlockType::EnemyWalker) {
                    cell.moveTimer += deltaTime * 2.0f;
                    cell.currentOffsetX = std::sin(cell.moveTimer) * 2.0f;
                    cell.currentOffsetY = 0.0f;
                    cell.currentOffsetZ = 0.0f;
                }

                // --- 敵2 (EnemyFlyer): 上下往復 (Y軸) ---
                if (cell.type == BlockType::EnemyFlyer) {
                    cell.moveTimer += deltaTime * 1.5f;
                    cell.currentOffsetY = std::sin(cell.moveTimer) * 2.5f;
                    cell.currentOffsetX = 0.0f;
                    cell.currentOffsetZ = 0.0f;
                }

                // --- 敵3 (EnemyChaser): プレイヤー追尾 (一定範囲内のみ) ---
                if (cell.type == BlockType::EnemyChaser) {
                    Vector3 spawnPos = { static_cast<float>(x), static_cast<float>(y), static_cast<float>(z) };
                    Vector3 currentPos = {
                        spawnPos.x + cell.currentOffsetX,
                        spawnPos.y + cell.currentOffsetY,
                        spawnPos.z + cell.currentOffsetZ
                    };
                    Vector3 toPlayer = {
                        playerPos.x - currentPos.x,
                        playerPos.y - currentPos.y,
                        playerPos.z - currentPos.z
                    };
                    float dist = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y + toPlayer.z * toPlayer.z);

                    float spawnDist = std::sqrt(
                        (playerPos.x - spawnPos.x) * (playerPos.x - spawnPos.x) +
                        (playerPos.y - spawnPos.y) * (playerPos.y - spawnPos.y) +
                        (playerPos.z - spawnPos.z) * (playerPos.z - spawnPos.z)
                    );

                    if (spawnDist < 8.0f && dist > 0.05f) {
                        float speed = 1.2f; // 秒速 1.2 マス
                        Vector3 dir = { toPlayer.x / dist, toPlayer.y / dist, toPlayer.z / dist };
                        Vector3 nextPos = {
                            currentPos.x + dir.x * (speed * deltaTime),
                            currentPos.y + dir.y * (speed * deltaTime),
                            currentPos.z + dir.z * (speed * deltaTime)
                        };

                        // スポーン位置からの最大追尾距離を 8 マスに制限
                        Vector3 nextOffset = {
                            nextPos.x - spawnPos.x,
                            nextPos.y - spawnPos.y,
                            nextPos.z - spawnPos.z
                        };
                        float offsetDist = std::sqrt(nextOffset.x * nextOffset.x + nextOffset.y * nextOffset.y + nextOffset.z * nextOffset.z);
                        if (offsetDist > 8.0f) {
                            nextOffset = { nextOffset.x / offsetDist * 8.0f, nextOffset.y / offsetDist * 8.0f, nextOffset.z / offsetDist * 8.0f };
                        }
                        cell.currentOffsetX = nextOffset.x;
                        cell.currentOffsetY = nextOffset.y;
                        cell.currentOffsetZ = nextOffset.z;
                    } else {
                        // プレイヤーが遠い場合は、ゆっくり初期位置に戻る
                        float returnSpeed = 1.0f;
                        float curDist = std::sqrt(cell.currentOffsetX * cell.currentOffsetX + cell.currentOffsetY * cell.currentOffsetY + cell.currentOffsetZ * cell.currentOffsetZ);
                        if (curDist > 0.05f) {
                            cell.currentOffsetX -= (cell.currentOffsetX / curDist) * returnSpeed * deltaTime;
                            cell.currentOffsetY -= (cell.currentOffsetY / curDist) * returnSpeed * deltaTime;
                            cell.currentOffsetZ -= (cell.currentOffsetZ / curDist) * returnSpeed * deltaTime;
                        } else {
                            cell.currentOffsetX = 0.0f;
                            cell.currentOffsetY = 0.0f;
                            cell.currentOffsetZ = 0.0f;
                        }
                    }
                }
            }
        }
    }
}

void StageMap::SaveToFile(const std::string& filename) {
    std::ofstream ofs(filename);
    if (!ofs.is_open()) return;

    // ヘッダー: サイズ
    ofs << width_ << " " << height_ << " " << depth_ << "\n";

    // 環境・ライティング設定を書き出す
    ofs << "ENVIRONMENT "
        << clearColor_.x << " " << clearColor_.y << " " << clearColor_.z << " " << clearColor_.w << " "
        << lightIntensity_ << " "
        << lightColor_.x << " " << lightColor_.y << " " << lightColor_.z << " "
        << lightDirection_.x << " " << lightDirection_.y << " " << lightDirection_.z << "\n";


    // カスタムブロック定義を書き出す
    for (const auto& part : customParts_) {
        ofs << "PART " << part.id << " "
            << static_cast<int>(part.baseType) << " "
            << part.colorR << " "
            << part.colorG << " "
            << part.colorB << " "
            << part.name << "\n";

        // アセンブリの各セルで None でないものを書き出す
        for (int y = 0; y < 3; ++y) {
            for (int z = 0; z < 3; ++z) {
                for (int x = 0; x < 3; ++x) {
                    if (part.cells[y][z][x].type != BlockType::None) {
                        ofs << "PARTCELL " << part.id << " "
                            << x << " " << y << " " << z << " "
                            << static_cast<int>(part.cells[y][z][x].type) << "\n";
                    }
                }
            }
        }
    }

    // ブロックデータ
    for (int y = 0; y < height_; ++y) {
        for (int z = 0; z < depth_; ++z) {
            for (int x = 0; x < width_; ++x) {
                const MapCell* cell = GetCell(x, y, z);
                if (cell->type == BlockType::None) continue; // 空ブロックは保存しない

                int saveVariant = cell->variant;
                if (cell->type == BlockType::MovingFloor) {
                    saveVariant = (cell->moveOffset.x + 10) | ((cell->moveOffset.y + 10) << 8) | ((cell->moveOffset.z + 10) << 16);
                }

                ofs << x << " " << y << " " << z << " "
                    << static_cast<int>(cell->type) << " "
                    << cell->rotationX << " " << cell->rotationY << " "
                    << saveVariant << "\n";

                if (cell->type == BlockType::Door) {
                    ofs << cell->doorTargetIndex.x << " "
                        << cell->doorTargetIndex.y << " "
                        << cell->doorTargetIndex.z << "\n"; // ドア先座標の後に改行を出力して安全に読み込めるようにする
                }
            }
        }
    }
    ofs.close();
}

void StageMap::LoadFromFile(const std::string& filename) {
    std::ifstream ifs(filename);
    if (!ifs.is_open()) return;

    std::string firstLine;
    if (!std::getline(ifs, firstLine)) return;

    std::stringstream ss(firstLine);
    int w, h, d;
    if (!(ss >> w >> h >> d)) return;
    Initialize(w, h, d);

    // 環境設定をデフォルト値に初期化（ファイルに記述がない場合用）
    clearColor_ = { 0.1f, 0.25f, 0.5f, 1.0f };
    lightIntensity_ = 1.0f;
    lightColor_ = { 0.9f, 0.9f, 0.9f };
    lightDirection_ = { 0.5f, -1.0f, 0.5f };

    // 各スロットがファイルロードによってクリアされたかを追跡するフラグ
    bool partCleared[5] = { false, false, false, false, false };

    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty()) continue;

        std::stringstream lineSS(line);
        std::string token;
        lineSS >> token;

        if (token == "PART") {
            int id, baseTypeVal;
            float r, g, b;
            lineSS >> id >> baseTypeVal >> r >> g >> b;
            std::string name;
            std::getline(lineSS, name);
            // 先頭のスペースを除去
            if (!name.empty() && name[0] == ' ') {
                name = name.substr(1);
            }

            if (id >= 1 && id <= (int)customParts_.size()) {
                auto& part = customParts_[id - 1];
                part.id = id;
                part.baseType = static_cast<BlockType>(baseTypeVal);
                part.colorR = r;
                part.colorG = g;
                part.colorB = b;
                part.name = name;
            }
        } else if (token == "PARTCELL") {
            int id, lx, ly, lz, typeVal;
            if (lineSS >> id >> lx >> ly >> lz >> typeVal) {
                if (id >= 1 && id <= (int)customParts_.size()) {
                    auto& part = customParts_[id - 1];

                    // ファイルにアセンブリセル情報があるスロットのみ、初回出現時に元のプリセット形状をクリアして適用
                    if (!partCleared[id - 1]) {
                        for (int y = 0; y < 3; ++y) {
                            for (int z = 0; z < 3; ++z) {
                                for (int x = 0; x < 3; ++x) {
                                    part.cells[y][z][x].type = BlockType::None;
                                }
                            }
                        }
                        partCleared[id - 1] = true;
                    }

                    if (lx >= 0 && lx < 3 && ly >= 0 && ly < 3 && lz >= 0 && lz < 3) {
                        part.cells[ly][lz][lx].type = static_cast<BlockType>(typeVal);
                    }
                }
            }
        } else if (token == "ENVIRONMENT") {
            lineSS >> clearColor_.x >> clearColor_.y >> clearColor_.z >> clearColor_.w
                   >> lightIntensity_
                   >> lightColor_.x >> lightColor_.y >> lightColor_.z
                   >> lightDirection_.x >> lightDirection_.y >> lightDirection_.z;
        } else {
            // 通常のブロック配置行（token は x 座標）
            int x = std::stoi(token);
            int y, z, typeVal;
            float rotX, rotY;
            int variant = 0;

            // 互換性重視の完璧なパース設計：
            // 残りパラメータが 5個（y z typeVal rotX rotY）以上あればパース成功
            if (lineSS >> y >> z >> typeVal >> rotX >> rotY) {
                // さらに variant があれば読み込む（なければデフォルトの0を使用）
                lineSS >> variant;

                BlockType type = static_cast<BlockType>(typeVal);
                SetBlock(x, y, z, type, variant);
                MapCell* cell = GetCell(x, y, z);
                if (cell) {
                    cell->rotationX = rotX;
                    cell->rotationY = rotY;

                    // ドアの追加データ
                    if (cell->type == BlockType::Door) {
                        ifs >> cell->doorTargetIndex.x
                            >> cell->doorTargetIndex.y
                            >> cell->doorTargetIndex.z;
                        // 改行を消費
                        std::string dummy;
                        std::getline(ifs, dummy);
                    }
                }
            }
        }
    }
    ifs.close();
    RebuildMovingFloorList();
    RebuildEnemyList();
}

void StageMap::Clear() {
    for (MapCell& cell : cells_) {
        cell.type = BlockType::None;
        cell.variant = 0;
        cell.isSolid = false;
    }
    enemies_.clear();
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
    RebuildMovingFloorList();
    RebuildEnemyList();
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
    RebuildMovingFloorList();
    RebuildEnemyList();

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

// ★ 追加：動く足場とのワールド座標（AABBボックス型）当たり判定の実装
const MapCell* StageMap::GetIntersectingMovingFloor(
    float pX, float pY, float pZ,
    float rX, float rY, float rZ
) const {
    for (const auto& ref : movingFloors_) {
        const MapCell* cell = GetCell(ref.x, ref.y, ref.z);
        if (!cell || cell->type != BlockType::MovingFloor) {
            continue;
        }

        float floorCenterX = static_cast<float>(ref.x) + cell->currentOffsetX;
        float floorCenterY = static_cast<float>(ref.y) + 0.5f + cell->currentOffsetY;
        float floorCenterZ = static_cast<float>(ref.z) + cell->currentOffsetZ;

        float playerCenterX = pX;
        float playerCenterY = pY + rY;
        float playerCenterZ = pZ;

        float blockSize = 0.5f;

        if (std::abs(playerCenterX - floorCenterX) < (rX + blockSize) &&
            std::abs(playerCenterY - floorCenterY) < (rY + blockSize) &&
            std::abs(playerCenterZ - floorCenterZ) < (rZ + blockSize)) {
            return cell;
        }
    }

    return nullptr;
}

void StageMap::RemoveConnectedKeyBlocks(int x, int y, int z)
{
    // マップの範囲外なら処理を抜ける
    if (x < 0 || x >= width_ || y < 0 || y >= height_ || z < 0 || z >= depth_) {
        return;
    }

    // 指定座標のセルを取得
    MapCell* cell = GetCell(x, y, z);

    // セルが存在しない、または「鍵ブロック」でなければ処理を抜ける
    // (既に None になっている場合もここで止まるため、無限ループを防げます)
    if (!cell || cell->type != BlockType::KeyBlock) {
        return;
    }

    // 自身のブロックを消去する
    cell->type = BlockType::None;
    cell->isSolid = false;

    // 上下左右前後の6方向に対して、同じ処理を芋づる式に呼び出す（再帰呼び出し）
    RemoveConnectedKeyBlocks(x + 1, y, z); // 右
    RemoveConnectedKeyBlocks(x - 1, y, z); // 左
    RemoveConnectedKeyBlocks(x, y + 1, z); // 上
    RemoveConnectedKeyBlocks(x, y - 1, z); // 下
    RemoveConnectedKeyBlocks(x, y, z + 1); // 前
    RemoveConnectedKeyBlocks(x, y, z - 1); // 後
}

Int3 StageMap::FindPairedDoor(int srcX, int srcY, int srcZ) const
{
    const MapCell* srcCell = GetCell(srcX, srcY, srcZ);
    // そもそも指定座標がドアではない、または存在しない場合は元の座標を返す
    if (!srcCell || srcCell->type != BlockType::Door) {
        return { srcX, srcY, srcZ };
    }

    // 入ったドアの番号（ID）
    int targetDoorId = srcCell->variant;

    // マップ全体をループして、同じドア番号を持つ「別の座標のドア」を探す
    for (int y = 0; y < height_; ++y) {
        for (int z = 0; z < depth_; ++z) {
            for (int x = 0; x < width_; ++x) {
                // 自分自身の座標はスキップ
                if (x == srcX && y == srcY && z == srcZ) continue;

                const MapCell* cell = GetCell(x, y, z);
                // 同じ種類のブロック（Door）かつ、同じ識別番号（variant）のセルが見つかった場合
                if (cell && cell->type == BlockType::Door && cell->variant == targetDoorId) {
                    return { x, y, z }; // 相方の座標を返す
                }
            }
        }
    }

    // 万が一、相方のドアが見つからなかった（1つしか配置していない）場合はワープさせず元の座標を返す
    return { srcX, srcY, srcZ };
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
    case BlockType::IceBlock:
    case BlockType::KeyBlock:    // 鍵ブロックは通り抜けられない
    cell.isSolid = true;
    break;

    case BlockType::MovingFloor:
    cell.isSolid = true;
    if (variant == 0) {
        cell.moveOffset = { 0, 0, 3 }; // デフォルト Z 軸に 3 マス
    } else {
        int dx = (variant & 0xFF) - 10;
        int dy = ((variant >> 8) & 0xFF) - 10;
        int dz = ((variant >> 16) & 0xFF) - 10;
        if (dx >= -10 && dx <= 10 && dy >= -10 && dy <= 10 && dz >= -10 && dz <= 10) {
            cell.moveOffset = { dx, dy, dz };
        } else {
            cell.moveOffset = { 0, 0, 3 };
        }
    }
    break;

    case BlockType::BubblePickup:
    case BlockType::Goal:
    case BlockType::PlayerStart:
    case BlockType::Door:
    case BlockType::PSwitch:
    case BlockType::PBlock:
    case BlockType::Key:         // 鍵は通り抜けられる
    cell.isSolid = false;
    break;

    default:
    cell.isSolid = false;
    break;
    }

    return cell;
}

void StageMap::ResetPSwitchState()
{
    isPSwitchActive_ = false;
}

void StageMap::RebuildMovingFloorList() {
    movingFloors_.clear();

    for (int y = 0; y < height_; ++y) {
        for (int z = 0; z < depth_; ++z) {
            for (int x = 0; x < width_; ++x) {
                const MapCell* cell = GetCell(x, y, z);
                if (cell && cell->type == BlockType::MovingFloor) {
                    movingFloors_.push_back({ x, y, z });
                }
            }
        }
    }
}

void StageMap::RebuildEnemyList() {
    enemies_.clear();

    for (int y = 0; y < height_; ++y) {
        for (int z = 0; z < depth_; ++z) {
            for (int x = 0; x < width_; ++x) {
                const MapCell* cell = GetCell(x, y, z);
                if (cell && (cell->type == BlockType::EnemyWalker || 
                             cell->type == BlockType::EnemyFlyer || 
                             cell->type == BlockType::EnemyChaser)) {
                    enemies_.push_back({ x, y, z });
                }
            }
        }
    }
}