//----------------------------------------------------------------------------
//! @file   test_scene.cpp
//! @brief  テストシーン実装（衝突判定テスト）
//----------------------------------------------------------------------------
#include "test_scene.h"
#include "dx11/graphics_context.h"
#include "engine/platform/renderer.h"
#include "engine/platform/application.h"
#include "engine/texture/texture_manager.h"
#include "engine/c_systems/sprite_batch.h"
#include "engine/c_systems/collision_manager.h"
#include "engine/math/color.h"
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

    // テスト用白テクスチャを作成（32x32）
    std::vector<uint32_t> whitePixels(32 * 32, 0xFFFFFFFF);
    testTexture_ = TextureManager::Get().Create2D(
        32, 32,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        D3D11_BIND_SHADER_RESOURCE,
        whitePixels.data(),
        32 * sizeof(uint32_t)
    );

    // 背景テクスチャをロード
    backgroundTexture_ = TextureManager::Get().LoadTexture2D("background.png");

    // 背景作成（最背面に描画、画面全体に表示）
    background_ = std::make_unique<GameObject>("Background");
    bgTransform_ = background_->AddComponent<Transform2D>();
    bgTransform_->SetPosition(Vector2(width * 0.5f, height * 0.5f));
    bgSprite_ = background_->AddComponent<SpriteRenderer>();
    bgSprite_->SetTexture(backgroundTexture_.get());
    bgSprite_->SetSortingLayer(-100);  // 最背面
    // 背景を画面サイズに合わせてスケール
    if (backgroundTexture_) {
        float texW = static_cast<float>(backgroundTexture_->Width());
        float texH = static_cast<float>(backgroundTexture_->Height());
        bgTransform_->SetPivot(Vector2(texW * 0.5f, texH * 0.5f));
        float scaleX = width / texW;
        float scaleY = height / texH;
        float scale = (scaleX > scaleY) ? scaleX : scaleY;
        bgTransform_->SetScale(Vector2(scale, scale));
    }

    // プレイヤー作成
    player_ = std::make_unique<Player>();
    player_->Initialize(Vector2(width * 0.5f, height * 0.5f));

    // 障害物を複数作成（赤色、静止）
    const float spacing = 150.0f;
    for (int i = 0; i < 5; ++i) {
        std::unique_ptr<GameObject> obj = std::make_unique<GameObject>("Obstacle" + std::to_string(i));

        Transform2D* transform = obj->AddComponent<Transform2D>();
        transform->SetPosition(Vector2(200.0f + i * spacing, 200.0f));
        transform->SetScale(2.0f);

        SpriteRenderer* sprite = obj->AddComponent<SpriteRenderer>();
        sprite->SetTexture(testTexture_.get());
        sprite->SetColor(Color(1.0f, 0.3f, 0.3f, 1.0f));  // 赤
        sprite->SetPivot(16.0f, 16.0f);

        Collider2D* collider = obj->AddComponent<Collider2D>(Vector2(64, 64));
        collider->SetLayer(0x02);
        collider->SetMask(0x01);

        objects_.push_back(std::move(obj));
    }

    // 下段にも障害物
    for (int i = 0; i < 5; ++i) {
        std::unique_ptr<GameObject> obj = std::make_unique<GameObject>("Obstacle" + std::to_string(i + 5));

        Transform2D* transform = obj->AddComponent<Transform2D>();
        transform->SetPosition(Vector2(275.0f + i * spacing, 400.0f));
        transform->SetScale(2.0f);

        SpriteRenderer* sprite = obj->AddComponent<SpriteRenderer>();
        sprite->SetTexture(testTexture_.get());
        sprite->SetColor(Color(0.3f, 0.3f, 1.0f, 1.0f));  // 青
        sprite->SetPivot(16.0f, 16.0f);

        Collider2D* collider = obj->AddComponent<Collider2D>(Vector2(64, 64));
        collider->SetLayer(0x02);
        collider->SetMask(0x01);

        objects_.push_back(std::move(obj));
    }

    LOG_INFO("[TestScene] 衝突判定テスト開始");
    LOG_INFO("  WASD: プレイヤー移動");
    LOG_INFO("  緑の四角を動かして赤/青の四角に当ててください");
}

//----------------------------------------------------------------------------
void TestScene::OnExit()
{
    objects_.clear();
    if (player_) {
        player_->Shutdown();
        player_.reset();
    }
    background_.reset();
    cameraObj_.reset();
    testTexture_.reset();
    backgroundTexture_.reset();
}

//----------------------------------------------------------------------------
void TestScene::Update()
{
    float dt = Application::Get().GetDeltaTime();
    time_ += dt;

    // デバッグ用にFPSを表示（1秒ごと）
    static float fpsTimer = 0.0f;
    fpsTimer += dt;
    if (fpsTimer >= 1.0f) {
        LOG_INFO("FPS: " + std::to_string(1.0f / dt));
        fpsTimer = 0.0f;
    }

    // プレイヤー更新
    if (player_ && camera_) {
        player_->Update(dt, *camera_);

        // カメラがプレイヤーを追従
        camera_->Follow(player_->GetTransform()->GetPosition(), 0.1f);
    }

    // 障害物更新
    for (std::unique_ptr<GameObject>& obj : objects_) {
        obj->Update(dt);
    }

    // 衝突判定実行（固定タイムステップ）
    CollisionManager::Get().Update(dt);
}

//----------------------------------------------------------------------------
void TestScene::Render()
{
    GraphicsContext& ctx = GraphicsContext::Get();
    Renderer& renderer = Renderer::Get();

    Texture* backBuffer = renderer.GetBackBuffer();
    Texture* depthBuffer = renderer.GetDepthBuffer();
    if (!backBuffer) return;

    // 深度バッファをバインド
    ctx.SetRenderTarget(backBuffer, depthBuffer);
    ctx.SetViewport(0, 0,
        static_cast<float>(backBuffer->Width()),
        static_cast<float>(backBuffer->Height()));

    // 背景クリア（暗い青）＋深度バッファクリア
    float clearColor[4] = { 0.1f, 0.1f, 0.2f, 1.0f };
    ctx.ClearRenderTarget(backBuffer, clearColor);
    ctx.ClearDepthStencil(depthBuffer, 1.0f, 0);

    // SpriteBatchで描画
    SpriteBatch& spriteBatch = SpriteBatch::Get();
    spriteBatch.SetCamera(*camera_);
    spriteBatch.Begin();

    // 背景描画（最背面）
    if (bgTransform_ && bgSprite_) {
        spriteBatch.Draw(*bgSprite_, *bgTransform_);
    }

    // 障害物描画
    for (std::unique_ptr<GameObject>& obj : objects_) {
        Transform2D* transform = obj->GetComponent<Transform2D>();
        SpriteRenderer* sprite = obj->GetComponent<SpriteRenderer>();
        if (transform && sprite) {
            spriteBatch.Draw(*sprite, *transform);
        }
    }

    // プレイヤー描画
    if (player_) {
        player_->Render(spriteBatch);
    }

    // デバッグ：コライダー枠描画
    const float lineWidth = 2.0f;

    // 枠を描画するラムダ関数
    auto drawColliderOutline = [&](Vector2 pos, Vector2 size, const Color& color) {
        float left = pos.x - size.x * 0.5f;
        float top = pos.y - size.y * 0.5f;
        float right = left + size.x;
        float bottom = top + size.y;

        // 上辺
        spriteBatch.Draw(testTexture_.get(), Vector2(left, top), color, 0.0f,
            Vector2(0, 0), Vector2(size.x / 32.0f, lineWidth / 32.0f), false, false, 100, 0);
        // 下辺
        spriteBatch.Draw(testTexture_.get(), Vector2(left, bottom - lineWidth), color, 0.0f,
            Vector2(0, 0), Vector2(size.x / 32.0f, lineWidth / 32.0f), false, false, 100, 0);
        // 左辺
        spriteBatch.Draw(testTexture_.get(), Vector2(left, top), color, 0.0f,
            Vector2(0, 0), Vector2(lineWidth / 32.0f, size.y / 32.0f), false, false, 100, 0);
        // 右辺
        spriteBatch.Draw(testTexture_.get(), Vector2(right - lineWidth, top), color, 0.0f,
            Vector2(0, 0), Vector2(lineWidth / 32.0f, size.y / 32.0f), false, false, 100, 0);
    };

    // プレイヤーのコライダー（緑枠）
    if (player_) {
        player_->RenderColliderDebug(spriteBatch, testTexture_.get());
    }

    // 障害物のコライダー（赤枠）
    Color obstacleColliderColor(1.0f, 0.0f, 0.0f, 1.0f);
    for (std::unique_ptr<GameObject>& obj : objects_) {
        Collider2D* collider = obj->GetComponent<Collider2D>();
        Transform2D* transform = obj->GetComponent<Transform2D>();
        if (collider && transform) {
            Vector2 pos = transform->GetPosition();
            Vector2 offset = collider->GetOffset();
            pos.x += offset.x;
            pos.y += offset.y;
            Vector2 size = collider->GetSize();
            drawColliderOutline(pos, size, obstacleColliderColor);
        }
    }

    spriteBatch.End();
}
