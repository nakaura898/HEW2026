//----------------------------------------------------------------------------
//! @file   test_scene.cpp
//! @brief  テストシーン実装（Animatorテスト）
//----------------------------------------------------------------------------
#include "test_scene.h"
#include "dx11/graphics_context.h"
#include "engine/platform/renderer.h"
#include "engine/platform/application.h"
#include "engine/texture/texture_manager.h"
#include "engine/input/input_manager.h"
#include "engine/graphics2d/sprite_batch.h"
#include "engine/color.h"
#include "common/logging/logging.h"

//----------------------------------------------------------------------------
void TestScene::OnEnter()
{
    Application& app = Application::Get();
    Window* window = app.GetWindow();
    float width = static_cast<float>(window->GetWidth());
    float height = static_cast<float>(window->GetHeight());

    // カメラ作成（Transform2D必須）
    cameraObj_ = std::make_unique<GameObject>("MainCamera");
    cameraObj_->AddComponent<Transform2D>(Vector2(width * 0.5f, height * 0.5f));
    camera_ = cameraObj_->AddComponent<Camera2D>(width, height);

    // テクスチャ読み込み
    TextureManager& texMgr = TextureManager::Get();
    backgroundTexture_ = texMgr.LoadTexture2D("assets:/texture/background.png");
    spriteTexture_ = texMgr.LoadTexture2D("assets:/texture/elf_sprite.png");

    if (!backgroundTexture_) {
        LOG_ERROR("[TestScene] background.png の読み込みに失敗");
    }
    if (!spriteTexture_) {
        LOG_ERROR("[TestScene] elf_sprite.png の読み込みに失敗");
    }

    // 背景作成
    background_ = std::make_unique<GameObject>("Background");
    bgTransform_ = background_->AddComponent<Transform2D>();
    bgTransform_->SetPosition(Vector2(width * 0.5f, height * 0.5f));

    bgSprite_ = background_->AddComponent<SpriteRenderer>();
    bgSprite_->SetTexture(backgroundTexture_.get());
    bgSprite_->SetSortingLayer(-1);  // 最背面

    // アニメーションスプライト作成
    sprite_ = std::make_unique<GameObject>("AnimatedSprite");
    spriteTransform_ = sprite_->AddComponent<Transform2D>();
    spriteTransform_->SetPosition(Vector2(width * 0.5f, height * 0.5f));
    spriteTransform_->SetScale(2.0f);

    spriteRenderer_ = sprite_->AddComponent<SpriteRenderer>();
    spriteRenderer_->SetTexture(spriteTexture_.get());
    spriteRenderer_->SetSortingLayer(0);  // 背景より手前

    // Animator追加（4行8列、10フレーム間隔）
    animator_ = sprite_->AddComponent<Animator>(4, 8, 10);
    animator_->SetRow(0);  // 最初のアニメーション行
    animator_->SetRowFrameCount(0, 8);  // 行0は8フレーム

    // SpriteBatch初期化
    SpriteBatch::Get().Initialize();

    LOG_INFO("[TestScene] Animatorテスト開始");
    LOG_INFO("  WASD: スプライト移動");
    LOG_INFO("  1-4: アニメーション行切り替え");
    LOG_INFO("  Space: 左右反転切り替え");
}

//----------------------------------------------------------------------------
void TestScene::OnExit()
{
    background_.reset();
    sprite_.reset();
    cameraObj_.reset();
    backgroundTexture_.reset();
    spriteTexture_.reset();
    SpriteBatch::Get().Shutdown();
}

//----------------------------------------------------------------------------
void TestScene::Update()
{
    float dt = Application::Get().GetDeltaTime();
    time_ += dt;

    InputManager* inputMgr = InputManager::GetInstance();
    if (!inputMgr) return;

    Keyboard& keyboard = inputMgr->GetKeyboard();

    // スプライト操作（WASD）
    const float speed = 200.0f;
    Vector2 move = Vector2::Zero;
    if (keyboard.IsKeyPressed(Key::W)) move.y -= speed * dt;
    if (keyboard.IsKeyPressed(Key::S)) move.y += speed * dt;
    if (keyboard.IsKeyPressed(Key::A)) move.x -= speed * dt;
    if (keyboard.IsKeyPressed(Key::D)) move.x += speed * dt;

    if (move.x != 0.0f || move.y != 0.0f) {
        spriteTransform_->Translate(move);
        // 移動方向で反転
        if (move.x < 0.0f) animator_->SetMirror(true);
        if (move.x > 0.0f) animator_->SetMirror(false);
    }

    // アニメーション行切り替え（1-4キー）
    if (keyboard.IsKeyTriggered(Key::D1)) animator_->SetRow(0);
    if (keyboard.IsKeyTriggered(Key::D2)) animator_->SetRow(1);
    if (keyboard.IsKeyTriggered(Key::D3)) animator_->SetRow(2);
    if (keyboard.IsKeyTriggered(Key::D4)) animator_->SetRow(3);

    // スペースで反転トグル
    if (keyboard.IsKeyTriggered(Key::Space)) {
        animator_->SetMirror(!animator_->GetMirror());
    }

    // スプライト更新（Animatorが更新される）
    sprite_->Update(dt);
}

//----------------------------------------------------------------------------
void TestScene::Render()
{
    GraphicsContext& ctx = GraphicsContext::Get();
    Renderer& renderer = Renderer::Get();

    Texture* backBuffer = renderer.GetBackBuffer();
    if (!backBuffer) return;

    ctx.SetRenderTarget(backBuffer, nullptr);
    ctx.SetViewport(0, 0,
        static_cast<float>(backBuffer->Width()),
        static_cast<float>(backBuffer->Height()));

    // 背景クリア
    float clearColor[4] = { 0.2f, 0.2f, 0.3f, 1.0f };
    ctx.ClearRenderTarget(backBuffer, clearColor);

    // SpriteBatchで描画
    SpriteBatch& spriteBatch = SpriteBatch::Get();
    spriteBatch.SetCamera(*camera_);
    spriteBatch.Begin();

    // 背景描画
    if (bgTransform_ && bgSprite_ && backgroundTexture_) {
        spriteBatch.Draw(*bgSprite_, *bgTransform_);
    }

    // アニメーションスプライト描画
    if (spriteTransform_ && spriteRenderer_ && animator_ && spriteTexture_) {
        spriteBatch.Draw(*spriteRenderer_, *spriteTransform_, *animator_);
    }

    spriteBatch.End();
}
