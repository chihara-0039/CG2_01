#include "StageSelect.h"
#include <filesystem>
#include <fstream>
#include "StageSelection.h"
#include <algorithm>

void StageSelect::Initialize(Object3dCommon* objCommon, Input* input, int startIndex)
{
	object3dCommon_ = objCommon;
	input_ = input;

		stageFiles_.clear();
	std::string sequencePath = "Resources/Stages/sequence.txt";
	std::string stageDir = "Resources/Stages/";

	if (std::filesystem::exists(sequencePath)) {
		std::ifstream ifs(sequencePath);
		std::string line;
		while (std::getline(ifs, line)) {
			// WindowsのCRLFをLinuxで検査しても末尾CRをファイル名に含めない。
			if (!line.empty() && line.back() == '\r') line.pop_back();
			if (!line.empty()) {
				stageFiles_.push_back(line);
			}
		}
	} else {
		if (std::filesystem::exists(stageDir)) {
			for (const auto& entry : std::filesystem::directory_iterator(stageDir)) {
				if (entry.is_regular_file()) {
					std::string fileName = entry.path().filename().string();
					if (fileName.ends_with(".txt") && fileName != "sequence.txt") {
						stageFiles_.push_back(fileName);
					}
				}
			}
		}
	}

	if (stageFiles_.empty()) {
		stageFiles_.push_back("tutorial.txt");
	}

	selectedStageIndex_ = std::clamp(startIndex, 0, static_cast<int>(stageFiles_.size()) - 1);
	isFinished_ = false;
	currentRotationX_ = currentRotationY_ = 0.0f;
	targetRotationX_ = targetRotationY_ = 0.0f;

	camera_.SetPosition({ 0.0f, 1.0f, -11.0f });
	camera_.SetRotation({ 0.0f, 0.0f, 0.0f });
	camera_.Update();

	// モデル読み込み（好きなモデルに変更OK）
	stageModel_ = Model::CreateFromOBJ(
		object3dCommon_->GetDxCommon(),
		"Resources/Models/stageSelect",
		"stageSelect.obj",
		object3dCommon_->GetTextureManager()
	);

	// 実体（オブジェクト）を作って初期化
	stageObject_ = std::make_unique<Object3d>();
	stageObject_->Initialize(object3dCommon_);

	// 読み込んだモデルをセットする
	stageObject_->SetModel(stageModel_.get());

	// 位置やサイズを設定（とりあえず原点に置きます）
	stageObject_->SetPosition({ 0.0f, 0.0f, 5.0f });
	stageObject_->SetScale({ 3.0f, 3.0f, 3.0f });

	for (size_t i = 0; i < labelModels_.size(); ++i) {
		labelModels_[i] = Model::CreateFromOBJ(object3dCommon_->GetDxCommon(),
			"Resources/Models/Recovery", "stageLabel" + std::to_string(i) + ".obj",
			object3dCommon_->GetTextureManager());
	}
	labelObject_ = std::make_unique<Object3d>();
	labelObject_->Initialize(object3dCommon_);
	labelObject_->SetPosition({0.0f, 3.2f, 5.0f});
	labelObject_->SetEnableLighting(false);
	labelObject_->SetColor({1.0f, 1.0f, 1.0f, 1.0f});
	backGuideModel_ = Model::CreateFromOBJ(object3dCommon_->GetDxCommon(),
		"Resources/Models/Recovery", "backGuide.obj", object3dCommon_->GetTextureManager());
	backGuideObject_ = std::make_unique<Object3d>();
	backGuideObject_->Initialize(object3dCommon_);
	backGuideObject_->SetModel(backGuideModel_.get());
	backGuideObject_->SetPosition({0.0f, -2.0f, 5.0f});
	backGuideObject_->SetEnableLighting(false);
}

void StageSelect::Update()
{
	// ステージファイルが一つもない場合は何もしない
	if (stageFiles_.empty()) {
		return;
	}

	// キーボードとパッドで同じ六面選択規則を使い、範囲外の面を選ばない。
	const auto move = [&](StageSelection::Direction direction) {
		selectedStageIndex_ = StageSelection::Move(selectedStageIndex_, direction,
			static_cast<int>(stageFiles_.size()));
	};
	if (input_->TriggerKey(DIK_W)) move(StageSelection::Direction::Up);
	if (input_->TriggerKey(DIK_S)) move(StageSelection::Direction::Down);
	if (input_->TriggerKey(DIK_A)) move(StageSelection::Direction::Left);
	if (input_->TriggerKey(DIK_D)) move(StageSelection::Direction::Right);
	if (input_->TriggerControllerButton(XINPUT_GAMEPAD_DPAD_UP)) {
		move(StageSelection::Direction::Up);
	}
	if (input_->TriggerControllerButton(XINPUT_GAMEPAD_DPAD_DOWN)) {
		move(StageSelection::Direction::Down);
	}
	if (input_->TriggerControllerButton(XINPUT_GAMEPAD_DPAD_LEFT)) {
		move(StageSelection::Direction::Left);
	}
	if (input_->TriggerControllerButton(XINPUT_GAMEPAD_DPAD_RIGHT)) {
		move(StageSelection::Direction::Right);
	}

	// --- 目標角度の設定 ---
	float targetDegX = 0.0f;
	float targetDegY = 0.0f;

	if      (selectedStageIndex_ == 0) { targetDegX = 0.0f;   targetDegY = 0.0f;  } //正面
	else if (selectedStageIndex_ == 1) { targetDegX = 0.0f;   targetDegY = 90.0f; } //右
	else if (selectedStageIndex_ == 2) { targetDegX = 0.0f;   targetDegY = 180.0f;} //裏
	else if (selectedStageIndex_ == 3) { targetDegX = 0.0f;   targetDegY = 270.0f;} //左
	else if (selectedStageIndex_ == 4) { targetDegX = -90.0f; targetDegY = 0.0f;  } //上
	else if (selectedStageIndex_ == 5) { targetDegX = 90.0f;  targetDegY = 0.0f;  } //下

	// 度からラジアンに変換して目標値に代入
	targetRotationX_ = targetDegX * (3.141592f / 180.0f);
	targetRotationY_ = targetDegY * (3.141592f / 180.0f);

	// 最短回転補正 (Y軸)
	while (targetRotationY_ - currentRotationY_ > 3.141592f)  targetRotationY_ -= 6.283184f;
	while (targetRotationY_ - currentRotationY_ < -3.141592f) targetRotationY_ += 6.283184f;
	// 最短回転補正 (X軸)
	while (targetRotationX_ - currentRotationX_ > 3.141592f)  targetRotationX_ -= 6.283184f;
	while (targetRotationX_ - currentRotationX_ < -3.141592f) targetRotationX_ += 6.283184f;

	// --- 滑らかに回転させる計算（イージング） ---
	currentRotationX_ += (targetRotationX_ - currentRotationX_) * 0.15f;
	currentRotationY_ += (targetRotationY_ - currentRotationY_) * 0.15f;

	// 回転をオブジェクトに反映
	stageObject_->SetRotation({ currentRotationX_, currentRotationY_, 0.0f });

	// --- スペースキーで決定 ---
	if (input_->TriggerKey(DIK_SPACE) ||
		input_->TriggerControllerButton(XINPUT_GAMEPAD_A))
	{
		// 実際にその番号に対応するファイルが存在する場合のみ決定
		if (selectedStageIndex_ < (int)stageFiles_.size())
		{
			isFinished_ = true;
		}
	}

	// --- 行列の更新 ---
	// カメラの行列を取得してセット
	const Matrix4x4& view = camera_.GetViewMatrix();
	const Matrix4x4& proj = camera_.GetProjectionMatrix();

	stageObject_->SetCamera(view, proj);

	// 最後にワールド行列を更新
	stageObject_->Update(Math::MakeIdentity4x4());
	if (selectedStageIndex_ < static_cast<int>(labelModels_.size())) {
		labelObject_->SetModel(labelModels_[selectedStageIndex_].get());
		labelObject_->SetCamera(view, proj);
		labelObject_->Update(Math::MakeIdentity4x4());
	}
	backGuideObject_->SetCamera(view, proj);
	backGuideObject_->Update(Math::MakeIdentity4x4());
}
void StageSelect::Draw()
{
	if (stageObject_ != nullptr)
	{
		stageObject_->Draw(); // 3Dモデルを描画！
	}
	if (labelObject_ && selectedStageIndex_ < static_cast<int>(labelModels_.size())) labelObject_->Draw();
	if (backGuideObject_) backGuideObject_->Draw();
}
