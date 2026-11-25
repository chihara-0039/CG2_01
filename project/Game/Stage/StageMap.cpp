#include "StageMap.h"
#include "StageCellBehavior.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <Windows.h>
#include <stdexcept>
#include <new>

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

bool StageMap::IsValidSize(int width, int height, int depth) {
    return width > 0 && height > 0 && depth > 0 && width <= kMaxDimension && height <= kMaxDimension &&
           depth <= kMaxDimension && static_cast<size_t>(width) * height * depth <= kMaxCells;
}

// 既存セルを残したままグリッドを広げ、確保失敗時は元のマップを保持する。
bool StageMap::Expand(int width, int height, int depth) {
    if (!IsValidSize(width, height, depth) || width < width_ || height < height_ || depth < depth_) {
        return false;
    }
    if (width == width_ && height == height_ && depth == depth_) {
        return true;
    }
    // 確保失敗時は元のマップを変更しない。
    try {
        std::vector<MapCell> expanded(static_cast<size_t>(width) * height * depth);
        for (int y = 0; y < height_; ++y) {
            for (int z = 0; z < depth_; ++z) {
                for (int x = 0; x < width_; ++x) {
                    expanded[x + z * width + y * width * depth] = cells_[ToIndex(x, y, z)];
                }
            }
        }
        cells_.swap(expanded);
    } catch (const std::bad_alloc &) {
        return false;
    }
    width_ = width;
    height_ = height;
    depth_ = depth;
    needsRebuild_ = true;
    return true;
}

// 編集した回転領域を再検証し、不正なら変更前の定義へ戻す。
bool StageMap::UpdateRotationRegion(int index, const RotationRegion &r) {
    if (index < 0 || index >= static_cast<int>(rotationRegions_.size())) {
        return false;
    }
    const auto previous = rotationRegions_;
    rotationRegions_.erase(rotationRegions_.begin() + index);
    if (!AddRotationRegion(r)) {
        rotationRegions_ = previous;
        return false;
    }
    rotationRegions_.pop_back();
    rotationRegions_.insert(rotationRegions_.begin() + index, r);
    return true;
}

// 範囲・重複・操作台の位置を検証し、有効な回転領域だけを追加する。
bool StageMap::AddRotationRegion(const RotationRegion &r) {
    if (r.size < 3 || r.size > 31 || r.size % 2 == 0 || r.x < 0 || r.z < 0 || r.x > width_ - r.size ||
        r.z > depth_ - r.size || !GetCell(r.control)) {
        return false;
    }
    const auto inside = [&](const RotationRegion &a, const Int3 &p) {
        return p.x >= a.x && p.x < a.x + a.size && p.z >= a.z && p.z < a.z + a.size;
    };
    if (inside(r, r.control)) {
        return false;
    }
    if (r.alternate.x != -1 && (!GetCell(r.alternate) || inside(r, r.alternate))) {
        return false;
    }
    for (const auto &a : rotationRegions_) {
        if ((r.x < a.x + a.size && r.x + r.size > a.x && r.z < a.z + a.size && r.z + r.size > a.z) ||
            inside(a, r.control) || inside(r, a.control) || (r.alternate.x != -1 && inside(a, r.alternate)) ||
            (a.alternate.x != -1 && inside(r, a.alternate))) {
            return false;
        }
    }
    rotationRegions_.push_back(r);
    return true;
}

// ステージのセルとギミックを準備し、初期値と必要なリソースを設定する。
void StageMap::Initialize(int width, int height, int depth) {
    if (!IsValidSize(width, height, depth)) {
        throw std::invalid_argument("Stage grid exceeds the supported size");
    }
    // ステージの3次元グリッドサイズを決め、全セルを確保する。
    assert(width > 0);
    assert(height > 0);
    assert(depth > 0);

    width_ = width;
    height_ = height;
    depth_ = depth;
    visualStyle_ = "Auto";
    rotationRegions_.clear();

    cells_.resize(static_cast<size_t>(width_) * height_ * depth_);

    // ステージ読み込み時に、前ステージのギミック状態が残らないようにリセットする。
    isPSwitchActive_ = false;
    isOnState_ = true;
    needsRebuild_ = false;
    accumulatedTime_ = 0.0f;

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

    // エディタで最初から使えるカスタムブロックのプリセット形状を用意する。
    // Slot 1: L-SHIELD (L字の壁足場パーツ)
    customParts_[0].name = "L-SHIELD";
    customParts_[0].colorR = 0.9f;
    customParts_[0].colorG = 0.3f;
    customParts_[0].colorB = 0.3f; // スタイリッシュ赤
    customParts_[0].cells[0][0][0].type = BlockType::Wall;
    customParts_[0].cells[0][0][1].type = BlockType::Wall;
    customParts_[0].cells[0][0][2].type = BlockType::Wall;
    customParts_[0].cells[1][0][2].type = BlockType::Wall;
    customParts_[0].cells[2][0][2].type = BlockType::Wall;

    // Slot 2: T-BRIDGE (T字足場パーツ)
    customParts_[1].name = "T-BRIDGE";
    customParts_[1].colorR = 0.9f;
    customParts_[1].colorG = 0.8f;
    customParts_[1].colorB = 0.2f; // ゴールド黄色
    customParts_[1].cells[0][0][1].type = BlockType::Wall;
    customParts_[1].cells[1][0][1].type = BlockType::Wall;
    customParts_[1].cells[2][0][0].type = BlockType::Wall;
    customParts_[1].cells[2][0][1].type = BlockType::Wall;
    customParts_[1].cells[2][0][2].type = BlockType::Wall;

    // Slot 3: LADDER-WALL (ハシゴ付き壁)
    customParts_[2].name = "LADDER-WALL";
    customParts_[2].colorR = 0.2f;
    customParts_[2].colorG = 0.7f;
    customParts_[2].colorB = 0.9f; // ライトブルー
    customParts_[2].cells[0][0][1].type = BlockType::Wall;
    customParts_[2].cells[1][0][1].type = BlockType::Wall;
    customParts_[2].cells[2][0][1].type = BlockType::Wall;
    customParts_[2].cells[0][0][0].type = BlockType::Ladder;
    customParts_[2].cells[1][0][0].type = BlockType::Ladder;
    customParts_[2].cells[2][0][0].type = BlockType::Ladder;

    // Slot 4 & 5: 空白のカスタム用スロット
    customParts_[3].name = "MY PART A";
    customParts_[3].colorR = 0.4f;
    customParts_[3].colorG = 0.9f;
    customParts_[3].colorB = 0.4f;                         // ライムグリーン
    customParts_[3].cells[0][0][0].type = BlockType::Wall; // 1マスだけ

    customParts_[4].name = "MY PART B";
    customParts_[4].colorR = 0.8f;
    customParts_[4].colorG = 0.4f;
    customParts_[4].colorB = 0.9f;                           // パープル
    customParts_[4].cells[0][0][0].type = BlockType::Ladder; // 1マスだけ
}

// 移動床・敵・崩れる床など、セルに属するギミックの時間と状態を進める。
void StageMap::Update(float deltaTime, const Vector3 &playerPos) {
    // ステージ全体で共有する経過時間。時間制ブロックなどの周期演出に使う。
    accumulatedTime_ += deltaTime;

    // 時間制ブロックは variant の十の位をグループ、 一の位を出現順として扱う。
    // まず各グループに存在する出現順を集め、ステージごとに必要な周期を求められるようにする。
    std::array<std::vector<int>, 10> groupOrders;
    for (const auto &cell : cells_) {
        if (cell.type == BlockType::TimedBlock) {
            int group = cell.variant / 10;
            int order = cell.variant % 10;
            if (group >= 1 && group < 10) {
                if (std::find(groupOrders[group].begin(), groupOrders[group].end(), order) ==
                    groupOrders[group].end()) {
                    groupOrders[group].push_back(order);
                }
            }
        }
    }

    // 出現順に並べることで、同じグループ内のブロックを順番に表示/非表示できる。
    for (int g = 1; g < 10; ++g) {
        if (!groupOrders[g].empty()) {
            std::sort(groupOrders[g].begin(), groupOrders[g].end());
        }
    }

    for (int y = 0; y < height_; ++y) {
        for (int z = 0; z < depth_; ++z) {
            for (int x = 0; x < width_; ++x) {
                MapCell &cell = cells_[ToIndex(x, y, z)];
                if (const auto *behavior = FindStageCellBehavior(cell.type)) {
                    behavior->Update(cell,
                                     StageCellContext{deltaTime, accumulatedTime_, {x, y, z}, playerPos, groupOrders});
                }
            }
        }
    }
}

// ステージデータをファイルに保存する。SaveToFile はステージサイズを最初に書き出すため、LoadFromFile が最初にこの3値を読んでグリッドを確保する。
void StageMap::SaveToFile(const std::string &filename) {
    std::ofstream ofs(filename);
    if (!ofs.is_open()) {
        return;
    }

    // 1行目は必ずステージサイズにする。LoadFromFile が最初にこの3値を読んでグリッドを確保する。
    ofs << width_ << " " << height_ << " " << depth_ << "\n";
    ofs << "VISUAL_STYLE " << visualStyle_ << "\n";
    for (const auto &r : rotationRegions_) {
        ofs << "ROTATION " << r.x << ' ' << r.z << ' ' << r.size << ' ' << r.control.x << ' ' << r.control.y << ' '
            << r.control.z << ' ' << r.alternate.x << ' ' << r.alternate.y << ' ' << r.alternate.z << '\n';
    }

    // ステージごとの見た目を再現するため、天候プリセットとライト設定も保存する。
    ofs << "PRESET \"" << weatherPresetName_ << "\"\n";
    ofs << "ENVIRONMENT " << clearColor_.x << " " << clearColor_.y << " " << clearColor_.z << " " << clearColor_.w
        << " " << lightIntensity_ << " " << lightColor_.x << " " << lightColor_.y << " " << lightColor_.z << " "
        << lightDirection_.x << " " << lightDirection_.y << " " << lightDirection_.z << "\n";

    // カスタムブロック定義を書き出す。
    // PART はスロット全体の見た目、PARTCELL は 3x3x3 内の実セル構成を表す。
    for (const auto &part : customParts_) {
        ofs << "PART " << part.id << " " << static_cast<int>(part.baseType) << " " << part.colorR << " " << part.colorG
            << " " << part.colorB << " " << part.name << "\n";

        // アセンブリの各セルで None でないものを書き出す
        for (int y = 0; y < 3; ++y) {
            for (int z = 0; z < 3; ++z) {
                for (int x = 0; x < 3; ++x) {
                    if (part.cells[y][z][x].type != BlockType::None) {
                        ofs << "PARTCELL " << part.id << " " << x << " " << y << " " << z << " "
                            << static_cast<int>(part.cells[y][z][x].type) << "\n";
                    }
                }
            }
        }
    }

    // 通常ブロックの配置情報を書き出す。空セルは保存せず、ファイルサイズを小さく保つ。
    for (int y = 0; y < height_; ++y) {
        for (int z = 0; z < depth_; ++z) {
            for (int x = 0; x < width_; ++x) {
                const MapCell *cell = GetCell(x, y, z);
                if (cell->type == BlockType::None) {
                    continue; // 空ブロックは保存しない
                }

                int saveVariant = cell->variant;
                if (cell->type == BlockType::MovingFloor) {
                    // MovingFloor は移動先オフセットを variant にパックして保存する。
                    // +10 しているのは負のオフセットも 0 以上の値として扱うため。
                    saveVariant = (cell->moveOffset.x + 10) | ((cell->moveOffset.y + 10) << 8) |
                                  ((cell->moveOffset.z + 10) << 16);
                }

                // 座標、ブロック種別、回転角度、variant を保存する。
                ofs << x << " " << y << " " << z << " " << static_cast<int>(cell->type) << " " << cell->rotationX << " "
                    << cell->rotationY << " " << saveVariant << "\n";

                // ドアブロックの場合、ドア先座標を保存する。
                if (cell->type == BlockType::Door) {
                    ofs << cell->doorTargetIndex.x << " " << cell->doorTargetIndex.y << " " << cell->doorTargetIndex.z
                        << "\n"; // ドア先座標の後に改行を出力して安全に読み込めるようにする
                }
            }
        }
    }
    ofs.close();
}

// ステージファイルを読み込む。LoadFromFile はステージサイズを最初に読み込むため、Initialize() でグリッドを確保する。
bool StageMap::LoadFromFile(const std::string &filename) {
    StageMap candidate;
    try {
        if (!candidate.ParseFromFile(filename)) {
            return false;
        }
    } catch (const std::exception &) {
        return false;
    }
    *this = std::move(candidate);
    return true;
}

// マップの各設定とセルを検証しながら、読み込み候補へ復元する。
bool StageMap::ParseFromFile(const std::string &filename) {
    std::ifstream ifs(filename);
    if (!ifs.is_open()) {
        return false;
    }

    // 最初の行からステージサイズを復元し、そのサイズでセル配列を作り直す。
    std::string firstLine;
    if (!std::getline(ifs, firstLine)) {
        return false;
    }
    if (firstLine.rfind("\xEF\xBB\xBF", 0) == 0) {
        firstLine.erase(0, 3);
    }

    // 1行目の "width height depth" を読み込む
    std::stringstream ss(firstLine);
    int w, h, d;
    if (!(ss >> w >> h >> d)) {
        return false;
    }
    if (!IsValidSize(w, h, d)) {
        return false;
    }
    Initialize(w, h, d);

    // 環境設定をデフォルト値に初期化（ファイルに記述がない場合用）
    clearColor_ = {0.1f, 0.25f, 0.5f, 1.0f};
    lightIntensity_ = 1.0f;
    lightColor_ = {0.9f, 0.9f, 0.9f};
    lightDirection_ = {0.5f, -1.0f, 0.5f};

    // PARTCELL があるスロットだけプリセット形状を消して、ファイル側の定義で上書きする。
    bool partCleared[5] = {false, false, false, false, false};

    std::string line;
    while (std::getline(ifs, line)) {
        if (line.empty()) {
            continue;
        }

        // 行の先頭トークンを取得して、行の種類を判定する。
        std::stringstream lineSS(line);
        std::string token;
        lineSS >> token;
        if (token.empty() || token[0] == '#') {
            continue;
        }

        if (token == "ROTATION") {
            RotationRegion r;
            if (!(lineSS >> r.x >> r.z >> r.size >> r.control.x >> r.control.y >> r.control.z)) {
                return false;
            }
            lineSS >> std::ws;
            if (!lineSS.eof() && !(lineSS >> r.alternate.x >> r.alternate.y >> r.alternate.z)) {
                return false;
            }
            if (!AddRotationRegion(r)) {
                return false;
            }
            continue;
        }
        if (token == "VISUAL_STYLE") {
            std::string style;
            lineSS >> style;
            if (style == "Auto" || style == "Classic" || style == "Miniature") {
                visualStyle_ = style;
            } else {
                return false;
            }
            continue;
        }

        if (token == "PART") {
            // カスタムブロックのスロット情報。色や名前など、UI と描画に使う設定を復元する。
            int id, baseTypeVal;
            float r, g, b;
            if (!(lineSS >> id >> baseTypeVal >> r >> g >> b) || id < 1 || id > 5 || baseTypeVal < 0 ||
                baseTypeVal > 26 || !std::isfinite(r) || !std::isfinite(g) || !std::isfinite(b)) {
                return false;
            }
            std::string name;
            std::getline(lineSS, name);
            // 先頭のスペースを除去
            if (!name.empty() && name[0] == ' ') {
                name = name.substr(1);
            }

            // パーツスロットの範囲内であれば復元する
            if (id >= 1 && id <= (int)customParts_.size()) {
                auto &part = customParts_[id - 1];
                part.id = id;
                part.baseType = static_cast<BlockType>(baseTypeVal);
                part.colorR = r;
                part.colorG = g;
                part.colorB = b;
                part.name = name;
            }
        } else if (token == "PARTCELL") {
            // カスタムブロックの内部セル情報。3x3x3 のどこに何のブロックがあるかを復元する。
            int id, lx, ly, lz, typeVal;
            // 互換性重視のパース設計：
            if (!(lineSS >> id >> lx >> ly >> lz >> typeVal) || id < 1 || id > 5 || lx < 0 || lx > 2 || ly < 0 ||
                ly > 2 || lz < 0 || lz > 2 || typeVal < 0 || typeVal > 26) {
                return false;
            }
            {
                // 座標が 0～2 の範囲内であることを確認してから配置する
                if (id >= 1 && id <= (int)customParts_.size()) {
                    auto &part = customParts_[id - 1];

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

                    // 座標が 0～2 の範囲内であることを確認してから配置する
                    if (lx >= 0 && lx < 3 && ly >= 0 && ly < 3 && lz >= 0 && lz < 3) {
                        part.cells[ly][lz][lx].type = static_cast<BlockType>(typeVal);
                    }
                }
            }
            // ステージの見た目を再現するため、天候プリセット名とライト設定も復元する。
        } else if (token == "PRESET") {
            // 天候プリセット名は空白を含められるようにダブルクォート付きで保存している。
            std::string presetName;
            std::getline(lineSS, presetName);
            // " " を取り除く
            size_t start = presetName.find('\"');
            size_t end = presetName.rfind('\"');
            if (start != std::string::npos && end != std::string::npos && start < end) {
                weatherPresetName_ = presetName.substr(start + 1, end - start - 1);
            } else {
                return false;
            }
            // 天候プリセット名を復元したら、ステージの見た目を再現するために ApplyWeatherPreset を呼ぶ。
        } else if (token == "ENVIRONMENT") {
            // 背景色、ライト強度、ライト色、ライト方向を復元する。
            lineSS >> clearColor_.x >> clearColor_.y >> clearColor_.z >> clearColor_.w >> lightIntensity_ >>
                lightColor_.x >> lightColor_.y >> lightColor_.z >> lightDirection_.x >> lightDirection_.y >>
                lightDirection_.z;
            if (!lineSS || !std::isfinite(clearColor_.x) || !std::isfinite(clearColor_.y) ||
                !std::isfinite(clearColor_.z) || !std::isfinite(clearColor_.w) || !std::isfinite(lightIntensity_) ||
                !std::isfinite(lightColor_.x) || !std::isfinite(lightColor_.y) || !std::isfinite(lightColor_.z) ||
                !std::isfinite(lightDirection_.x) || !std::isfinite(lightDirection_.y) ||
                !std::isfinite(lightDirection_.z)) {
                return false;
            }
        } else {
            // 通常のブロック配置行（token は x 座標）
            size_t consumed = 0;
            int x = std::stoi(token, &consumed);
            if (consumed != token.size()) {
                return false;
            }
            int y, z, typeVal;
            float rotX, rotY;
            int variant = 0;

            // 互換性重視の完璧なパース設計：
            // 残りパラメータが 5個（y z typeVal rotX rotY）以上あればパース成功
            if (!(lineSS >> y >> z >> typeVal >> rotX >> rotY) || !GetCell(x, y, z) || typeVal < 0 || typeVal > 26 ||
                !std::isfinite(rotX) || !std::isfinite(rotY)) {
                return false;
            }
            {
                // さらに variant があれば読み込む（なければデフォルトの0を使用）
                lineSS >> std::ws;
                if (!lineSS.eof() && !(lineSS >> variant)) {
                    return false;
                }

                // 座標がステージ内にあるか確認してから配置する
                BlockType type = static_cast<BlockType>(typeVal);
                SetBlock(x, y, z, type, variant);
                MapCell *cell = GetMutableCell(x, y, z);
                // 角度を復元する
                if (cell) {
                    cell->rotationX = rotX;
                    cell->rotationY = rotY;

                    // ドアの追加データ
                    if (cell->type == BlockType::Door) {
                        if (!(ifs >> cell->doorTargetIndex.x >> cell->doorTargetIndex.y >> cell->doorTargetIndex.z)) {
                            return false;
                        }
                        // 改行を消費
                        std::string dummy;
                        std::getline(ifs, dummy);
                    }
                }
            }
        }
    }
    if (ifs.bad()) {
        return false;
    }
    ifs.close();
    RebuildMovingFloorList();
    RebuildEnemyList();
    return true;
}

// ステージ全体を初期化する（全セルを None に置き換える）
void StageMap::Clear() {
    for (MapCell &cell : cells_) {
        cell = MapCell{};
        cell.type = BlockType::None;
        cell.variant = 0;
        cell.isSolid = false;
    }

    movingFloors_.clear();
    enemies_.clear();

    isPSwitchActive_ = false;
    isOnState_ = true;
    needsRebuild_ = false;

    ResetTime();
}

// 指定された座標がステージ内にあるかどうかを判定する
bool StageMap::IsInside(int x, int y, int z) const {
    return x >= 0 && x < width_ && y >= 0 && y < height_ && z >= 0 && z < depth_;
}

// Int3 版の IsInside
bool StageMap::IsInside(const Int3 &index) const {
    return IsInside(index.x, index.y, index.z);
}

// セルのインデックスを 1 次元配列に変換する
const MapCell *StageMap::GetCell(int x, int y, int z) const {
    if (!IsInside(x, y, z)) {
        return nullptr;
    }
    return &cells_[ToIndex(x, y, z)];
}

// Int3 版の GetCell
const MapCell *StageMap::GetCell(const Int3 &index) const {
    return GetCell(index.x, index.y, index.z);
}

// Mutable 版の GetCell
MapCell *StageMap::GetMutableCell(int x, int y, int z) {
    if (!IsInside(x, y, z)) {
        return nullptr;
    }
    return &cells_[ToIndex(x, y, z)];
}

// セル値と派生リストを一緒に更新し、読み取り側に古い当たり判定や敵一覧を残さない。
bool StageMap::SetCell(const Int3 &index, const MapCell &cell) {
    auto *current = GetMutableCell(index.x, index.y, index.z);
    if (!current || static_cast<uint32_t>(cell.type) > static_cast<uint32_t>(BlockType::TransparentBlock) ||
        (cell.type == BlockType::None && cell.isSolid)) {
        return false;
    }
    for (float value : {cell.rotationX, cell.rotationY, cell.crumbleTimer, cell.respawnTimer, cell.colorR, cell.colorG,
                        cell.colorB, cell.opacity, cell.moveTimer, cell.currentOffsetX, cell.currentOffsetY,
                        cell.currentOffsetZ, cell.deltaOffsetX, cell.deltaOffsetY, cell.deltaOffsetZ}) {
        if (!std::isfinite(value)) {
            return false;
        }
    }
    const auto enemy = [](BlockType type) {
        return type == BlockType::EnemyWalker || type == BlockType::EnemyFlyer || type == BlockType::EnemyChaser;
    };
    // 変更した座標だけを更新し、連続配置のたびに全マップを走査しない。
    const auto updateList = [&](auto &list, bool wasMember, bool isMember) {
        if (wasMember == isMember) {
            return;
        }
        if (isMember) {
            // 全走査で作った場合と同じグリッド順を保ち、重なった足場の判定順を変えない。
            const int target = ToIndex(index.x, index.y, index.z);
            const auto position = std::lower_bound(list.begin(), list.end(), target, [&](const auto &entry, int value) {
                return ToIndex(entry.x, entry.y, entry.z) < value;
            });
            list.insert(position, {index.x, index.y, index.z});
        } else {
            list.erase(std::remove_if(list.begin(), list.end(),
                                      [&](const auto &entry) {
                                          return entry.x == index.x && entry.y == index.y && entry.z == index.z;
                                      }),
                       list.end());
        }
    };
    updateList(movingFloors_, current->type == BlockType::MovingFloor, cell.type == BlockType::MovingFloor);
    updateList(enemies_, enemy(current->type), enemy(cell.type));
    *current = cell;
    needsRebuild_ = true;
    return true;
}

// 崩壊の開始通知だけを受け取り、タイマーや描画色の更新はStageMap自身に任せる。
bool StageMap::StartCrumbling(const Int3 &index) {
    auto *cell = GetMutableCell(index.x, index.y, index.z);
    if (!cell || cell->type != BlockType::CrumblingFloor || cell->isHidden) {
        return false;
    }
    cell->isCrumbling = true;
    return true;
}

// パーツはコピーを保存し、外側に残った編集値から内部の形状を変更できないようにする。
bool StageMap::SetCustomPart(int id, const CustomBlockPart &part) {
    if (id < 1 || id > static_cast<int>(customParts_.size()) ||
        static_cast<uint32_t>(part.baseType) > static_cast<uint32_t>(BlockType::TransparentBlock)) {
        return false;
    }
    for (float color : {part.colorR, part.colorG, part.colorB}) {
        if (!std::isfinite(color) || color < 0 || color > 1) {
            return false;
        }
    }
    for (const auto &layer : part.cells) {
        for (const auto &row : layer) {
            for (const auto &cell : row) {
                if (static_cast<uint32_t>(cell.type) > static_cast<uint32_t>(BlockType::TransparentBlock)) {
                    return false;
                }
            }
        }
    }
    customParts_[id - 1] = part;
    customParts_[id - 1].id = id;
    needsRebuild_ = true;
    return true;
}

// 指定座標のブロックを新しいタイプとバリアントで置き換える
bool StageMap::SetBlock(int x, int y, int z, BlockType type, int variant) {
    if (!IsInside(x, y, z)) {
        return false;
    }

    // 指定座標のブロックを新しいタイプとバリアントで置き換える
    return SetCell({x, y, z}, MakeCell(type, variant));
}

// Int3 版の SetBlock
bool StageMap::SetBlock(const Int3 &index, BlockType type, int variant) {
    return SetBlock(index.x, index.y, index.z, type, variant);
}

// 指定座標のブロックを削除する（None に置き換える）
bool StageMap::RemoveBlock(int x, int y, int z) {
    if (!IsInside(x, y, z)) {
        return false;
    }

    // 指定座標のブロックを None に置き換える
    return SetCell({x, y, z}, MakeCell(BlockType::None, 0));
}

// Int3 版の RemoveBlock
bool StageMap::RemoveBlock(const Int3 &index) {
    return RemoveBlock(index.x, index.y, index.z);
}

// PSwitch の状態を切り替える
void StageMap::SetPSwitchActive(int switchId) {
    isPSwitchActive_ = true;
    needsRebuild_ = true;
    for (auto &cell : cells_) {
        if ((cell.type == BlockType::PSwitch || cell.type == BlockType::PBlock) && cell.variant == switchId) {
            cell.isSolid = false;
            cell.isHidden = true;
        }
        if (cell.type == BlockType::PBlockAppears && cell.variant == switchId) {
            cell.isSolid = true;
        }
    }
}

// PSwitch の状態をリセットする（再構築なし）
void StageMap::ResetPSwitchStateNoRebuild() {
    isPSwitchActive_ = false;
    for (auto &cell : cells_) {
        if (cell.type == BlockType::PSwitch) {
            cell.isSolid = false;
            cell.isHidden = false;
        } else if (cell.type == BlockType::PBlock) {
            cell.isSolid = true;
            cell.isHidden = false;
        } else if (cell.type == BlockType::PBlockAppears) {
            cell.isSolid = false;
        }
    }
    needsRebuild_ = false;
}

// ステージ全体の ON/OFF 状態を切り替える
void StageMap::ToggleOnState() {
    isOnState_ = !isOnState_;
    for (auto &cell : cells_) {
        if (cell.type == BlockType::OnBlock) {
            cell.isSolid = isOnState_;
        } else if (cell.type == BlockType::OffBlock) {
            cell.isSolid = !isOnState_;
        }
    }
    needsRebuild_ = true;
}

// ステージのセルとギミックの設定と状態をGUIへ表示し、編集入力を反映する。
void StageMap::DrawImGui() {
#ifdef USE_IMGUI
    ImGui::Text("Size: %d x %d x %d", width_, height_, depth_);

    // 固定位置のセルの情報など（デバッグ用）
    const MapCell *cell = GetCell(2, 1, 0);
    if (cell) {
        ImGui::Text("Cell(2,1,0) type = %d", static_cast<int>(cell->type));
        ImGui::Text("Cell(2,1,0) solid = %s", cell->isSolid ? "true" : "false");
    }
#endif
}

// ★ 追加：動く足場とのワールド座標（AABBボックス型）当たり判定の実装
const MapCell *StageMap::GetIntersectingMovingFloor(float pX, float pY, float pZ, float rX, float rY, float rZ) const {
    for (const auto &ref : movingFloors_) {
        const MapCell *cell = GetCell(ref.x, ref.y, ref.z);
        if (!cell || cell->type != BlockType::MovingFloor) {
            continue;
        }

        // 動く足場の中心座標を計算（セルの中心 + 現在のオフセット）
        float floorCenterX = static_cast<float>(ref.x) + cell->currentOffsetX;
        // The rendered mesh and static grid both use the integer cell centre.
        float floorCenterY = static_cast<float>(ref.y) + cell->currentOffsetY;
        float floorCenterZ = static_cast<float>(ref.z) + cell->currentOffsetZ;

        // プレイヤーの中心座標を計算（AABBの中心）
        float playerCenterX = pX;
        float playerCenterY = pY + rY;
        float playerCenterZ = pZ;

        float blockSize = 0.5f;

        // AABB（軸平行境界ボックス）同士の交差判定
        if (std::abs(playerCenterX - floorCenterX) < (rX + blockSize) &&
            std::abs(playerCenterY - floorCenterY) < (rY + blockSize) &&
            std::abs(playerCenterZ - floorCenterZ) < (rZ + blockSize)) {
            return cell;
        }
    }

    return nullptr;
}

// 接続している鍵ブロックをたどり、まとめて解放する。
void StageMap::RemoveConnectedKeyBlocks(int x, int y, int z) {
    std::vector<Int3> pending;
    auto enqueue = [&](int cx, int cy, int cz) {
        auto *cell = GetMutableCell(cx, cy, cz);
        if (!cell || cell->type != BlockType::KeyBlock) {
            return;
        }
        cell->type = BlockType::None;
        cell->isSolid = false;
        pending.push_back({cx, cy, cz});
    };
    enqueue(x, y, z);
    if (pending.empty()) {
        return;
    }
    needsRebuild_ = true;
    // Mark on enqueue, so every connected cell is processed exactly once without recursion.
    while (!pending.empty()) {
        const auto cell = pending.back();
        pending.pop_back();
        enqueue(cell.x + 1, cell.y, cell.z);
        enqueue(cell.x - 1, cell.y, cell.z);
        enqueue(cell.x, cell.y + 1, cell.z);
        enqueue(cell.x, cell.y - 1, cell.z);
        enqueue(cell.x, cell.y, cell.z + 1);
        enqueue(cell.x, cell.y, cell.z - 1);
    }
}

// 明示された接続先と扉番号を調べ、対応する移動先を探す。
Int3 StageMap::FindPairedDoor(int srcX, int srcY, int srcZ) const {
    const Int3 source{srcX, srcY, srcZ};
    const auto *src = GetCell(srcX, srcY, srcZ);
    if (!src || src->type != BlockType::Door) {
        return source;
    }
    const auto target = src->doorTargetIndex;
    const auto *linked = GetCell(target);
    // Reciprocal links distinguish authored pairs from legacy default/self coordinates.
    if ((target.x != srcX || target.y != srcY || target.z != srcZ) && linked && linked->type == BlockType::Door &&
        linked->doorTargetIndex.x == srcX && linked->doorTargetIndex.y == srcY && linked->doorTargetIndex.z == srcZ) {
        return target;
    }
    Int3 candidate = source;
    bool found = false;
    for (int y = 0; y < height_; ++y) {
        for (int z = 0; z < depth_; ++z) {
            for (int x = 0; x < width_; ++x) {
                if (x == srcX && y == srcY && z == srcZ) {
                    continue;
                }
                const auto *cell = GetCell(x, y, z);
                if (cell && cell->type == BlockType::Door && cell->variant == src->variant) {
                    if (found) {
                        return source;
                    } // Ambiguous IDs must not choose an arbitrary exit.
                    candidate = {x, y, z};
                    found = true;
                }
            }
        }
    }
    return candidate;
}

// 座標 (x, y, z) を 1 次元配列のインデックスに変換する
int StageMap::ToIndex(int x, int y, int z) const {
    return x + (z * width_) + (y * width_ * depth_);
}

// Int3 版の ToIndex
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
    case BlockType::KeyBlock: // 鍵ブロックは通り抜けられない
    case BlockType::PBlock:
    case BlockType::TransparentBlock:
        cell.isSolid = true;
        break;

    case BlockType::MovingFloor:
        cell.isSolid = true;
        if (variant == 0) {
            cell.moveOffset = {0, 0, 3}; // デフォルト Z 軸に 3 マス
        } else {
            int dx = (variant & 0xFF) - 10;
            int dy = ((variant >> 8) & 0xFF) - 10;
            int dz = ((variant >> 16) & 0xFF) - 10;
            if (dx >= -10 && dx <= 10 && dy >= -10 && dy <= 10 && dz >= -10 && dz <= 10) {
                cell.moveOffset = {dx, dy, dz};
            } else {
                cell.moveOffset = {0, 0, 3};
            }
        }
        break;

    case BlockType::BubblePickup:
    case BlockType::Goal:
    case BlockType::PlayerStart:
    case BlockType::Door:
    case BlockType::PSwitch:
    case BlockType::Key:        // 鍵は通り抜けられる
    case BlockType::Checkpoint: // 🌟 追加：中間地点は通り抜けられる
    case BlockType::TimedBlock: // 🌟 追加：時間差ブロック（最初は消えている）
        cell.isSolid = false;
        break;

    case BlockType::OnBlock:
        // ON状態なら実体化（true）、OFF状態ならすり抜ける（false）
        cell.isSolid = isOnState_;
        break;

    case BlockType::OffBlock:
        // OFF状態なら実体化（true）、ON状態ならすり抜ける（false）
        cell.isSolid = !isOnState_;
        break;

    case BlockType::OnOffSwitch:
        // スイッチ自体は叩く必要があるので常に当たり判定を持たせる
        cell.isSolid = false;
        break;
    }

    return cell;
}

// PSwitch の状態をリセットする（再構築あり）
void StageMap::ResetPSwitchState() {
    isPSwitchActive_ = false;
}

// 動く足場のリストを再構築する
void StageMap::RebuildMovingFloorList() {
    movingFloors_.clear();

    for (int y = 0; y < height_; ++y) {
        for (int z = 0; z < depth_; ++z) {
            for (int x = 0; x < width_; ++x) {
                const MapCell *cell = GetCell(x, y, z);
                if (cell && cell->type == BlockType::MovingFloor) {
                    movingFloors_.push_back({x, y, z});
                }
            }
        }
    }
}

// 敵のリストを再構築する
void StageMap::RebuildEnemyList() {
    enemies_.clear();

    for (int y = 0; y < height_; ++y) {
        for (int z = 0; z < depth_; ++z) {
            for (int x = 0; x < width_; ++x) {
                const MapCell *cell = GetCell(x, y, z);
                if (cell && (cell->type == BlockType::EnemyWalker || cell->type == BlockType::EnemyFlyer ||
                             cell->type == BlockType::EnemyChaser)) {
                    enemies_.push_back({x, y, z});
                }
            }
        }
    }
}
