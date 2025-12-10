//----------------------------------------------------------------------------
//! @file   test_scene.cpp
//! @brief  テストシーン実装（衝突判定テスト）
//----------------------------------------------------------------------------
#include "test_scene.h"
#include "dx11/graphics_context.h"
#include "engine/platform/renderer.h"
#include "engine/platform/application.h"
#include "engine/texture/texture_manager.h"
#include "engine/input/input_manager.h"
#include "engine/graphics2d/sprite_batch.h"
#include "engine/collision/collision_manager.h"
#include "engine/color.h"
#include "common/logging/logging.h"
#include <cmath>

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

    // プレイヤー作成（WASDで操作、緑色）
    player_ = std::make_unique<GameObject>("Player");
    playerTransform_ = player_->AddComponent<Transform2D>();
    playerTransform_->SetPosition(Vector2(width * 0.5f, height * 0.5f));
    playerTransform_->SetScale(2.0f);

    playerSprite_ = player_->AddComponent<SpriteRenderer>();
    playerSprite_->SetTexture(testTexture_.get());
    playerSprite_->SetColor(Color(0.2f, 1.0f, 0.2f, 1.0f));  // 緑

    // プレイヤーにコライダー追加
    Collider2D* playerCollider = player_->AddComponent<Collider2D>(Vector2(64, 64));
    playerCollider->SetLayer(0x01);  // プレイヤーレイヤー
    playerCollider->SetMask(0x02);   // 障害物と衝突

    // 衝突コールバック設定
    playerCollider->SetOnCollisionEnter([this](Collider2D* /*self*/, Collider2D* /*other*/) {
        ++collisionCount_;
        LOG_INFO("[Collision] Enter!");
        // 衝突時に白くフラッシュ
        playerSprite_->SetColor(Color(1.0f, 1.0f, 1.0f, 1.0f));
    });

    playerCollider->SetOnCollisionExit([this](Collider2D* /*self*/, Collider2D* /*other*/) {
        LOG_INFO("[Collision] Exit!");
        // 衝突終了時に緑に戻る
        playerSprite_->SetColor(Color(0.2f, 1.0f, 0.2f, 1.0f));
    });

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

        // 障害物にコライダー追加
        Collider2D* collider = obj->AddComponent<Collider2D>(Vector2(64, 64));
        collider->SetLayer(0x02);  // 障害物レイヤー
        collider->SetMask(0x01);   // プレイヤーと衝突

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

        Collider2D* collider = obj->AddComponent<Collider2D>(Vector2(64, 64));
        collider->SetLayer(0x02);
        collider->SetMask(0x01);

        objects_.push_back(std::move(obj));
    }

    // SpriteBatch初期化
    SpriteBatch::Get().Initialize();

    LOG_INFO("[TestScene] 衝突判定テスト開始");
    LOG_INFO("  WASD: プレイヤー移動");
    LOG_INFO("  緑の四角を動かして赤/青の四角に当ててください");
}

//----------------------------------------------------------------------------
void TestScene::OnExit()
{
    objects_.clear();
    player_.reset();
    cameraObj_.reset();
    testTexture_.reset();
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

    // プレイヤー操作（WASD）
    const float speed = 300.0f;
    Vector2 move = Vector2::Zero;
    if (keyboard.IsKeyPressed(Key::W)) move.y -= speed * dt;
    if (keyboard.IsKeyPressed(Key::S)) move.y += speed * dt;
    if (keyboard.IsKeyPressed(Key::A)) move.x -= speed * dt;
    if (keyboard.IsKeyPressed(Key::D)) move.x += speed * dt;

    if (move.x != 0.0f || move.y != 0.0f) {
        playerTransform_->Translate(move);
    }

    // プレイヤー更新（Collider2D::Updateが呼ばれる）
    player_->Update(dt);

    // カメラがプレイヤーを追従
    camera_->Follow(playerTransform_->GetPosition(), 0.1f);

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
    if (!backBuffer) return;

    ctx.SetRenderTarget(backBuffer, nullptr);
    ctx.SetViewport(0, 0,
        static_cast<float>(backBuffer->Width()),
        static_cast<float>(backBuffer->Height()));

    // 背景クリア（暗い青）
    float clearColor[4] = { 0.1f, 0.1f, 0.2f, 1.0f };
    ctx.ClearRenderTarget(backBuffer, clearColor);

    // SpriteBatchで描画
    SpriteBatch& spriteBatch = SpriteBatch::Get();
    spriteBatch.SetCamera(*camera_);
    spriteBatch.Begin();

    // 障害物描画
    for (std::unique_ptr<GameObject>& obj : objects_) {
        Transform2D* transform = obj->GetComponent<Transform2D>();
        SpriteRenderer* sprite = obj->GetComponent<SpriteRenderer>();
        if (transform && sprite) {
            spriteBatch.Draw(*sprite, *transform);
        }
    }

    // プレイヤー描画（最前面）
    if (playerTransform_ && playerSprite_) {
        spriteBatch.Draw(*playerSprite_, *playerTransform_);
    }

    spriteBatch.End();
}
