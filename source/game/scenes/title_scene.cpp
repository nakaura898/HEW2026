#include "title_scene.h"
#include "test_scene.h"
#include "dx11/graphics_context.h"
#include "engine/scene/scene_manager.h"
#include "engine/platform/application.h"
#include "engine/platform/renderer.h"
#include "engine/input/input_manager.h"
#include "engine/input/key.h"
#include "engine/c_systems/sprite_batch.h"
#include "engine/debug/debug_draw.h"
#include "engine/component/transform.h"
#include "engine/component/ui_button_component.h"
#include "common/logging/logging.h"


//ライフサイクル
//初期化
void Title_Scene::OnEnter()
{
	LOG_INFO("現在のシーン : タイトル");

	//画面中央
	cameraObj_ = std::make_unique<GameObject>("Camera");
	cameraObj_->AddComponent<Transform>(Vector2(640.0f, 360.0f));
	camera_ = cameraObj_->AddComponent<Camera2D>(1280.0f, 720.0f);

	// スタートボタン（GameObjectベース）
	startButtonObj_ = std::make_unique<GameObject>("StartButton");
	startButtonObj_->AddComponent<Transform>(Vector2(640.0f, 400.0f));
	startButton_ = startButtonObj_->AddComponent<UIButtonComponent>();
	startButton_->SetSize(Vector2(200.0f, 100.0f));
	startButton_->SetOnClick([]() {
		SceneManager::Get().Load<TestScene>();
	});
	startButton_->SetNormalColor(Color(0.2f, 0.5f, 0.2f, 1.0f));
	startButton_->SetHoverColor(Color(0.5f, 0.5f, 0.5f, 1.0f));
}

//破棄
void Title_Scene::OnExit()
{
	cameraObj_.reset();
	startButtonObj_.reset();
	startButton_ = nullptr;
}

//フレームコールバック
//更新
void Title_Scene::Update()
{
	auto& input = InputManager::Get();

	//space or enterがおされたら
	if (input.GetKeyboard().IsKeyDown(Key::Enter) || input.GetKeyboard().IsKeyDown(Key::Space))
	{
		//Gameシーンへの画面遷移
		SceneManager::Get().Load<TestScene>();
	}

	//escで終了
	if (input.GetKeyboard().IsKeyDown(Key::Escape))
	{
		Application::Get().Quit();
	}

	// GameObjectの更新（UIButtonComponentも更新される）
	startButtonObj_->Update(0.016f);  // TODO: 実際のdeltaTimeを使用
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

	SpriteBatch& batch = SpriteBatch::Get();

	batch.Begin();
	batch.SetCamera(*camera_);

	// ボタン描画（Transformから位置取得）
	Transform* buttonTransform = startButtonObj_->GetComponent<Transform>();
	if (buttonTransform && startButton_) {
		DEBUG_RECT_FILL(
			buttonTransform->GetPosition(),
			startButton_->GetSize(),
			startButton_->GetCurrentColor()
		);
	}

	batch.End();
}