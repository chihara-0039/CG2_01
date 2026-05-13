#include "StageSelect.h"
#include <filesystem>

void StageSelect::Initialize(Object3dCommon* objCommon, Input* input)
{
	object3dCommon_ = objCommon;
	input_ = input;

	stageFiles_.clear();
	std::string stageDir = "Resources/Stages/";
	if (std::filesystem::exists(stageDir))
	{
		for (const auto& entry : std::filesystem::directory_iterator(stageDir))
		{
			if (entry.is_regular_file())
			{
				std::string fileName = entry.path().filename().string();
				if (fileName.ends_with(".txt"))
				{
					stageFiles_.push_back(fileName);
				}
			}
		}
	}

	selectedStageIndex_ = 0;
	isFinished_ = false;

	camera_.SetPosition({ 8.0f, 5.0f, -11.0f });
	camera_.SetRotation({ 0.0f, 0.0f, 0.0f });

	// モデル読み込み（好きなモデルに変更OK）
	stageModel_ = Model::CreateFromOBJ(
		object3dCommon_->GetDxCommon(),
		"Resources/Models/stageSelect",
		"stageSelect.obj",
		object3dCommon_->GetTextureManager()
	);

	// 実体（オブジェクト）を作って初期化
	stageObject_ = new Object3d();
	stageObject_->Initialize(object3dCommon_);

	// 読み込んだモデルをセットする
	stageObject_->SetModel(stageModel_);

	// 位置やサイズを設定（とりあえず原点に置きます）
	stageObject_->SetPosition({ 0.0f, 0.0f, 5.0f });
	stageObject_->SetScale({ 3.0f, 3.0f, 3.0f });
}

void StageSelect::Update()
{
	// ステージファイルが一つもない場合は何もしない
	if (stageFiles_.empty()) return;

	// ファイル数に関わらず、サイコロの6面分（0～5）を回せるように設定
	int maxStage = 6;

	// --- Aキー：前の面へ ---
	if (input_->TriggerKey(DIK_A))
	{
		selectedStageIndex_ = (selectedStageIndex_ - 1 + maxStage) % maxStage;
	}

	// --- Dキー：次の面へ ---
	if (input_->TriggerKey(DIK_D))
	{
		selectedStageIndex_ = (selectedStageIndex_ + 1) % maxStage;
	}

	// =========================================================
	// 目標角度の設定（度数法で設定すると分かりやすい）
	// =========================================================
	float targetDegX = 0.0f;
	float targetDegY = 0.0f;

	if (selectedStageIndex_ == 0) { targetDegX = 0.0f;   targetDegY = 0.0f; } // 1面：正面
	else if (selectedStageIndex_ == 1) { targetDegX = 0.0f;   targetDegY = 90.0f; } // 2面：右
	else if (selectedStageIndex_ == 2) { targetDegX = 0.0f;   targetDegY = 180.0f; } // 3面：裏
	else if (selectedStageIndex_ == 3) { targetDegX = 0.0f;   targetDegY = 270.0f; } // 4面：左
	else if (selectedStageIndex_ == 4) { targetDegX = -90.0f; targetDegY = 0.0f; } // 5面：上
	else if (selectedStageIndex_ == 5) { targetDegX = 90.0f;  targetDegY = 0.0f; } // 6面：下

	// 度からラジアンに変換して目標値に代入
	targetRotationX_ = targetDegX * (3.141592f / 180.0f);
	targetRotationY_ = targetDegY * (3.141592f / 180.0f);

	// --- 滑らかに回転させる計算（イージング） ---
	currentRotationX_ += (targetRotationX_ - currentRotationX_) * 0.15f;
	currentRotationY_ += (targetRotationY_ - currentRotationY_) * 0.15f;

	// 回転をオブジェクトに反映
	stageObject_->SetRotation({ currentRotationX_, currentRotationY_, 0.0f });

	// --- スペースキーで決定 ---
	if (input_->TriggerKey(DIK_SPACE))
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
}
void StageSelect::Draw()
{
	if (stageObject_ != nullptr)
	{
		stageObject_->Draw(); // 3Dモデルを描画！
	}
}