#pragma once
#include "engine/scene/scene.h"
#include "engine/component/game_object.h"
#include "engine/component/camera2d.h"
#include <memory>

#include"game/ui/ui_button.h"


class Title_Scene : public Scene
{
//プライベート
private:
	//コンポーネント呼び出し
	//カメラ
	//std::unique_ptr<Camera2D> camera_;


	//スタートボタンテスト
	std::unique_ptr<UIButton> startButton_;


	// カメラ
	std::unique_ptr<GameObject> cameraObj_;
	Camera2D* camera_ = nullptr;


//パブリック
public:
	//ライフサイクル
	Title_Scene() = default;
	~Title_Scene() override = default;
	//初期化
	void OnEnter() override;
	//破棄
	void OnExit() override;

	//フレームコールバック
	//更新
	void Update() override;
	//描画
	void Render() override;

};

