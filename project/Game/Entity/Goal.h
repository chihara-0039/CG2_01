#pragma once
#include "StageMap.h"
#include "MyMath.h"
class Goal
{
public:
	// プレイヤーとゴールの当たり判定
	static bool Check(const Vector3& pos, const Vector3& radius, const StageMap& map);
};

