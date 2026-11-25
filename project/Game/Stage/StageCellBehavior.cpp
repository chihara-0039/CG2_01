#include "StageCellBehavior.h"
#include <algorithm>
#include <cmath>

namespace {
// TimedBlockの一フレームの振る舞い。状態は受け取ったセルだけに保持する。
class TimedBlockBehavior final : public StageCellBehavior {
  public:
    // 出現順と共有時刻から実体化する区間を判定する。
    void Update(MapCell &cell, const StageCellContext &context) const override {

        // 同じグループ内で、order の小さいブロックから順に出現させる。
        int group = cell.variant / 10;
        int order = cell.variant % 10;
        if (group >= 1 && group < 10 && !context.timedOrders[group].empty()) {
            const auto &orders = context.timedOrders[group];

            float kAppearDelay = 1.2f;    // 1.2秒間隔で次のブロックが出現
            float kActiveDuration = 3.0f; // 3.0秒間表示する
            float kRestDuration = 1.5f;   // 全て消えた後のインターバル

            // グループ内の最後のブロックが消えた後、少し待ってから最初に戻る。
            float cycleDuration =
                static_cast<float>(orders.size() - 1) * kAppearDelay + kActiveDuration + kRestDuration;

            // このブロックの出現順インデックスを取得
            auto it = std::find(orders.begin(), orders.end(), order);
            size_t idx = std::distance(orders.begin(), it);

            // 出現時間と消滅時間を計算
            float appearTime = static_cast<float>(idx) * kAppearDelay;
            float disappearTime = appearTime + kActiveDuration;

            // 現在の経過時間を周期内に収める
            float localCycleTime = std::fmod(context.elapsedTime, cycleDuration);
            if (localCycleTime >= appearTime && localCycleTime < disappearTime) {
                cell.isSolid = true;
            } else {
                cell.isSolid = false;
            }
        }
    }
};
// CrumblingFloorの一フレームの振る舞い。状態は受け取ったセルだけに保持する。
class CrumblingFloorBehavior final : public StageCellBehavior {
  public:
    // 乗った床の崩壊・復活・色と透明度を更新する。
    void Update(MapCell &cell, const StageCellContext &context) const override {

        // --- 崩れる処理 ---
        if (!cell.isHidden) {
            if (cell.crumbleTimer > 0.0f || cell.isCrumbling) {
                cell.crumbleTimer += context.deltaTime;
                // ここが崩れるタイマー
                if (cell.crumbleTimer >= 1.0f) {
                    cell.isHidden = true;
                    cell.isSolid = false;
                }
            }
        }

        // --- 復活処理 ---
        if (cell.isHidden) {
            cell.respawnTimer += context.deltaTime;
            if (cell.respawnTimer >= 3.0f) { // 3秒で復活
                cell.isHidden = false;
                cell.isSolid = true; // 判定復活
                cell.respawnTimer = 0.0f;
                cell.crumbleTimer = 0.0f;
            }
        }

        // --- 演出用の色・透明度計算 ---
        if (!cell.isHidden) {
            // crumbleTimerが0なら白、1.0に近づくほど赤くなる
            float r = cell.crumbleTimer / 1.0f;
            cell.colorG = 1.0f - r;
            cell.colorB = 1.0f - r;
            cell.opacity = 1.0f - r; // ★追加：乗っている間は徐々に透明（フェードアウト）にする
        } else {
            cell.opacity = 0.0f; // ★追加：完全に消えている間は透明度 0
        }
        cell.isCrumbling = false;
    }
};
// MovingFloorの一フレームの振る舞い。状態は受け取ったセルだけに保持する。
class MovingFloorBehavior final : public StageCellBehavior {
  public:
    // 往復位置と前フレームからの差分を求める。
    void Update(MapCell &cell, const StageCellContext &context) const override {

        // MovingFloor は moveOffset で指定された目的地まで往復する。
        // currentOffset は描画位置、deltaOffset は乗っているプレイヤーを一緒に動かすために使う。
        float moveSpeed = 1.0f;
        cell.moveTimer += context.deltaTime * moveSpeed;

        float moveRate = (std::sin(cell.moveTimer) + 1.0f) / 2.0f;

        // 前フレームのオフセットを記憶
        float oldX = cell.currentOffsetX;
        float oldY = cell.currentOffsetY;
        float oldZ = cell.currentOffsetZ;

        // 新しいオフセットを計算
        cell.currentOffsetX = static_cast<float>(cell.moveOffset.x) * moveRate;
        cell.currentOffsetY = static_cast<float>(cell.moveOffset.y) * moveRate;
        cell.currentOffsetZ = static_cast<float>(cell.moveOffset.z) * moveRate;

        // 1フレーム分の移動量を記録
        cell.deltaOffsetX = cell.currentOffsetX - oldX;
        cell.deltaOffsetY = cell.currentOffsetY - oldY;
        cell.deltaOffsetZ = cell.currentOffsetZ - oldZ;
    }
};
// EnemyWalkerの一フレームの振る舞い。状態は受け取ったセルだけに保持する。
class EnemyWalkerBehavior final : public StageCellBehavior {
  public:
    // 横方向の往復位置を求める。
    void Update(MapCell &cell, const StageCellContext &context) const override {

        cell.moveTimer += context.deltaTime * 2.0f;
        cell.currentOffsetX = std::sin(cell.moveTimer) * 2.0f;
        cell.currentOffsetY = 0.0f;
        cell.currentOffsetZ = 0.0f;
    }
};
// EnemyFlyerの一フレームの振る舞い。状態は受け取ったセルだけに保持する。
class EnemyFlyerBehavior final : public StageCellBehavior {
  public:
    // 上下の往復位置を求める。
    void Update(MapCell &cell, const StageCellContext &context) const override {

        cell.moveTimer += context.deltaTime * 1.5f;
        cell.currentOffsetY = std::sin(cell.moveTimer) * 2.5f;
        cell.currentOffsetX = 0.0f;
        cell.currentOffsetZ = 0.0f;
    }
};
// EnemyChaserの一フレームの振る舞い。状態は受け取ったセルだけに保持する。
class EnemyChaserBehavior final : public StageCellBehavior {
  public:
    // 範囲内では追尾し、離れたら出現位置へ戻す。
    void Update(MapCell &cell, const StageCellContext &context) const override {

        // スポーン地点から一定範囲内にプレイヤーが入ると追尾し、遠ざかると初期位置へ戻る。
        Vector3 spawnPos = {static_cast<float>(context.index.x), static_cast<float>(context.index.y),
                            static_cast<float>(context.index.z)};
        Vector3 currentPos = {spawnPos.x + cell.currentOffsetX, spawnPos.y + cell.currentOffsetY,
                              spawnPos.z + cell.currentOffsetZ};
        Vector3 toPlayer = {context.playerPosition.x - currentPos.x, context.playerPosition.y - currentPos.y,
                            context.playerPosition.z - currentPos.z};
        float distanceToPlayer = std::sqrt(toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y + toPlayer.z * toPlayer.z);

        // プレイヤーがスポーン位置からどれくらい離れているかを計算
        float playerDistanceFromSpawn =
            std::sqrt((context.playerPosition.x - spawnPos.x) * (context.playerPosition.x - spawnPos.x) +
                      (context.playerPosition.y - spawnPos.y) * (context.playerPosition.y - spawnPos.y) +
                      (context.playerPosition.z - spawnPos.z) * (context.playerPosition.z - spawnPos.z));

        // プレイヤーがスポーン位置から8マス以内にいる場合のみ追尾する
        if (playerDistanceFromSpawn < 8.0f && distanceToPlayer > 0.05f) {
            float speed = 1.2f; // 秒速 1.2 マス
            Vector3 directionToPlayer = {toPlayer.x / distanceToPlayer, toPlayer.y / distanceToPlayer,
                                         toPlayer.z / distanceToPlayer};
            Vector3 nextPos = {currentPos.x + directionToPlayer.x * (speed * context.deltaTime),
                               currentPos.y + directionToPlayer.y * (speed * context.deltaTime),
                               currentPos.z + directionToPlayer.z * (speed * context.deltaTime)};

            // スポーン位置からの最大追尾距離を 8 マスに制限
            Vector3 nextOffset = {nextPos.x - spawnPos.x, nextPos.y - spawnPos.y, nextPos.z - spawnPos.z};
            // 8 マス以上離れないように制限する
            float offsetDistanceFromSpawn =
                std::sqrt(nextOffset.x * nextOffset.x + nextOffset.y * nextOffset.y + nextOffset.z * nextOffset.z);
            if (offsetDistanceFromSpawn > 8.0f) {
                nextOffset = {nextOffset.x / offsetDistanceFromSpawn * 8.0f,
                              nextOffset.y / offsetDistanceFromSpawn * 8.0f,
                              nextOffset.z / offsetDistanceFromSpawn * 8.0f};
            }
            cell.currentOffsetX = nextOffset.x;
            cell.currentOffsetY = nextOffset.y;
            cell.currentOffsetZ = nextOffset.z;
        } else {
            // プレイヤーが遠い場合は、ゆっくり初期位置に戻る
            float returnSpeed = 1.0f;
            float currentOffsetDistance =
                std::sqrt(cell.currentOffsetX * cell.currentOffsetX + cell.currentOffsetY * cell.currentOffsetY +
                          cell.currentOffsetZ * cell.currentOffsetZ);
            if (currentOffsetDistance > 0.05f) {
                cell.currentOffsetX -= (cell.currentOffsetX / currentOffsetDistance) * returnSpeed * context.deltaTime;
                cell.currentOffsetY -= (cell.currentOffsetY / currentOffsetDistance) * returnSpeed * context.deltaTime;
                cell.currentOffsetZ -= (cell.currentOffsetZ / currentOffsetDistance) * returnSpeed * context.deltaTime;
            } else {
                cell.currentOffsetX = 0.0f;
                cell.currentOffsetY = 0.0f;
                cell.currentOffsetZ = 0.0f;
            }
        }
    }
};
}
// 保存用の種類番号を振る舞いへ対応させる。更新不要なセルには実装を割り当てない。
const StageCellBehavior *FindStageCellBehavior(BlockType type) {
    static const TimedBlockBehavior timedBlock;
    static const CrumblingFloorBehavior crumblingFloor;
    static const MovingFloorBehavior movingFloor;
    static const EnemyWalkerBehavior enemyWalker;
    static const EnemyFlyerBehavior enemyFlyer;
    static const EnemyChaserBehavior enemyChaser;
    static const auto table = [] {
        std::array<const StageCellBehavior *, static_cast<size_t>(BlockType::TransparentBlock) + 1> result{};
        result[static_cast<size_t>(BlockType::TimedBlock)] = &timedBlock;
        result[static_cast<size_t>(BlockType::CrumblingFloor)] = &crumblingFloor;
        result[static_cast<size_t>(BlockType::MovingFloor)] = &movingFloor;
        result[static_cast<size_t>(BlockType::EnemyWalker)] = &enemyWalker;
        result[static_cast<size_t>(BlockType::EnemyFlyer)] = &enemyFlyer;
        result[static_cast<size_t>(BlockType::EnemyChaser)] = &enemyChaser;
        return result;
    }();
    const auto index = static_cast<size_t>(type);
    return index < table.size() ? table[index] : nullptr;
}
