#pragma once

// 六面の選択操作を描画・入力APIから分離し、範囲外アクセスを防ぐ。
namespace StageSelection {
enum class Direction { Up, Down, Left, Right };
int Move(int current, Direction direction, int stageCount);
}
