#include "result_scene.h"
#include "title_scene.h"
#include "dx11/graphics_context.h"
#include "engine/scene/scene_manager.h"
#include "engine/platform/renderer.h"
#include "engine/input/input_manager.h"
#include "engine/input/key.h"
#include "game/systems/game_state_manager.h"
#include "common/logging/logging.h"


//ライフサイクル
//初期化
void Result_Scene::OnEnter()
{
	LOG_INFO("現在のシーン : リザルト");

	// 勝敗結果を取得
	GameState result = GameStateManager::GetLastResult();
	isVictory_ = (result == GameState::Victory);
	LOG_INFO(isVictory_ ? "リザルト: 勝利" : "リザルト: 敗北");
}

//破棄
void Result_Scene::OnExit()
{
}

//フレームコールバック
//更新
void Result_Scene::Update()
{
	auto& input = InputManager::Get();

	// SpaceまたはEnterが押されたら
	if (input.GetKeyboard().IsKeyDown(Key::Enter) || input.GetKeyboard().IsKeyDown(Key::Space))
	{
		//Titleシーンへの画面遷移
		SceneManager::Get().Load<Title_Scene>();
	}
}

//描画
void Result_Scene::Render()
{
	GraphicsContext& ctx = GraphicsContext::Get();
	Renderer& renderer = Renderer::Get();

	Texture* backBuffer = renderer.GetBackBuffer();
	Texture* depthBuffer = renderer.GetDepthBuffer();
	if (!backBuffer) return;

	ctx.SetRenderTarget(backBuffer, depthBuffer);
	ctx.SetViewport(0, 0,
		static_cast<float>(backBuffer->Width()),
		static_cast<float>(backBuffer->Height()));

	// 勝敗に応じた背景色でクリア
	// 勝利: 青, 敗北: 赤
	float clearColor[4];
	if (isVictory_) {
		clearColor[0] = 0.2f;  // R
		clearColor[1] = 0.4f;  // G
		clearColor[2] = 0.8f;  // B
		clearColor[3] = 1.0f;  // A
	} else {
		clearColor[0] = 0.8f;  // R
		clearColor[1] = 0.2f;  // G
		clearColor[2] = 0.2f;  // B
		clearColor[3] = 1.0f;  // A
	}
	ctx.ClearRenderTarget(backBuffer, clearColor);
	ctx.ClearDepthStencil(depthBuffer, 1.0f, 0);
}