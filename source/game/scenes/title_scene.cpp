#include "title_scene.h"
#include "test_scene.h"
#include "dx11/graphics_context.h"
#include "engine/scene/scene_manager.h"
#include "engine/platform/renderer.h"
#include "engine/input/input_manager.h"
#include "engine/input/key.h"
#include "engine/c_systems/sprite_batch.h"
#include "engine/debug/debug_draw.h"
#include "common/logging/logging.h"


//ライフサイクル
//初期化
void Title_Scene::OnEnter()
{
	LOG_INFO("現在のシーン : タイトル");
	camera_ = std::make_unique<Camera2D>();
	//画面中央
	camera_->SetPosition(Vector2(640.0f, 360.0f));
}
//破棄
void Title_Scene::OnExit()
{
	camera_.reset();
}

//フレームコールバック
//更新
void Title_Scene::Update()
{
	auto& input = *InputManager::GetInstance();

	//space or enterがおされたら
	if (input.GetKeyboard().IsKeyDown(Key::Enter) || input.GetKeyboard().IsKeyDown(Key::Space))
	{
		//Gameシーンへの画面遷移
		SceneManager::Get().Load<TestScene>();
	}

	//escで終了
	if (input.GetKeyboard().IsKeyDown(Key::Escape))
	{
		PostQuitMessage(0);
	}
}

//描画
void Title_Scene::Render()
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

	// 黒で画面クリア
	float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	ctx.ClearRenderTarget(backBuffer, clearColor);
	ctx.ClearDepthStencil(depthBuffer, 1.0f, 0);
}