#pragma once
#include "Object3d.h"
#include "Object3dCommon.h"
#include "Model.h"
#include "Camera.h"
#include "Input.h"
#include <vector>
#include <string>
#include <memory>

class StageSelect
{
public:
	void Initialize(Object3dCommon* objCommon, Input* input, int startIndex = 0);

	void Update();

	void Draw();

	bool IsFnished() const { return isFinished_; }
	int GetSelectedIndex() const { return selectedStageIndex_; }
	std::string GetSelectedFileName() const { return stageFiles_[selectedStageIndex_]; }
private:
	Object3dCommon* object3dCommon_ = nullptr;
	Input* input_ = nullptr;
	Camera camera_;

	std::vector<std::string> stageFiles_;

	int selectedStageIndex_ = 0;
	bool isFinished_ = false;

	std::unique_ptr<Model> stageModel_;
	std::unique_ptr<Object3d> stageObject_;

	Vector3 rotation_ = { 0.0f, 0.0f, 0.0f };

	float targetRotationX_ = 0.0f;
	float targetRotationY_ = 0.0f;
	float currentRotationX_ = 0.0f;
	float currentRotationY_ = 0.0f;
};