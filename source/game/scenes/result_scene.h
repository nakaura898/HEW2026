#pragma once
#include "engine/scene/scene.h"


class Result_Scene : public Scene
{
//プライベート
private:
	//! 勝敗結果（true=勝利、false=敗北）
	bool isVictory_ = false;

//パブリック
public:
	//ライフサイクル
	Result_Scene() = default;
	~Result_Scene() override = default;
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