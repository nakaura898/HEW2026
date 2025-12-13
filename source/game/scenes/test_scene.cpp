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
#include "engine/c_systems/sprite_batch.h"
#include "engine/c_systems/collision_manager.h"
#include "engine/math/color.h"
#include "common/logging/logging.h"
#include <cmath>
#include <algorithm>

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

    // プレイヤー用スプライトシートをロード
    playerTexture_ = TextureManager::Get().LoadTexture2D("elf_sprite.png");

    // 矢用テクスチャを作成（細長い白：32x8）
    std::vector<uint32_t> arrowPixels(32 * 8, 0xFFFFFFFF);
    arrowTexture_ = TextureManager::Get().Create2D(
        32, 8,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        D3D11_BIND_SHADER_RESOURCE,
        arrowPixels.data(),
        32 * sizeof(uint32_t)
    );

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
        // pivotをテクスチャの中心に設定
        bgTransform_->SetPivot(Vector2(texW * 0.5f, texH * 0.5f));
        float scaleX = width / texW;
        float scaleY = height / texH;
        float scale = (scaleX > scaleY) ? scaleX : scaleY;  // 画面全体を覆うように大きい方に合わせる
        bgTransform_->SetScale(Vector2(scale, scale));
    }

    // プレイヤー作成（WASDで操作）
    player_ = std::make_unique<GameObject>("Player");
    playerTransform_ = player_->AddComponent<Transform2D>();
    playerTransform_->SetPosition(Vector2(width * 0.5f, height * 0.5f));
    playerTransform_->SetScale(0.3f);  // 小さく

    playerSprite_ = player_->AddComponent<SpriteRenderer>();
    playerSprite_->SetTexture(playerTexture_.get());
    playerSprite_->SetSortingLayer(10);  // 前面に描画

    // Animatorコンポーネント追加（4行4列、デフォルト6フレーム間隔）
    constexpr int kAnimRows = 4;
    constexpr int kAnimCols = 4;
    playerAnimator_ = player_->AddComponent<Animator>(kAnimRows, kAnimCols, 6);

    // Pivot: スプライトの原点設定（SpriteRendererに設定）
    // SetPivotFromCenter(frameWidth, frameHeight, offsetX, offsetY)
    // - offsetX: キャラが中心より右にいる場合は正の値
    // - offsetY: キャラが中心より下にいる場合は正の値
    if (playerTexture_) {
        float frameWidth = static_cast<float>(playerTexture_->Width()) / kAnimCols;
        float frameHeight = static_cast<float>(playerTexture_->Height()) / kAnimRows;
        float offsetX = 0.0f;  // 調整が必要な場合はここを変更
        float offsetY = 0.0f;
        playerSprite_->SetPivotFromCenter(frameWidth, frameHeight, offsetX, offsetY);
    }
    playerAnimator_->SetRowFrameCount(0, 1, 12);  // Idle: 1フレーム、遅め
    playerAnimator_->SetRowFrameCount(1, 4, 36);   // Walk: 4フレーム、普通
    playerAnimator_->SetRowFrameCount(2, 3, 20);   // Attack: 3フレーム、速め
    playerAnimator_->SetRowFrameCount(3, 2, 10);  // Death: 2フレーム、やや遅め
    playerAnimator_->SetRow(1);  // 初期はWalkアニメーション

    // プレイヤーにコライダー追加（テスト: 中心対称）
    Collider2D* playerCollider = player_->AddComponent<Collider2D>();
    playerCollider->SetBounds(Vector2(-30, -40), Vector2(30, 40));  // 60x80、中心がTransform位置
    playerCollider->SetLayer(0x01);  // プレイヤーレイヤー
    playerCollider->SetMask(0x02);   // 障害物と衝突

    // 衝突コールバック設定
    playerCollider->SetOnCollisionEnter([this](Collider2D* /*self*/, Collider2D* /*other*/) {
        ++collisionCount_;
        LOG_INFO("[Collision] Enter!");
    });

    playerCollider->SetOnCollisionExit([this](Collider2D* /*self*/, Collider2D* /*other*/) {
        LOG_INFO("[Collision] Exit!");
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
        sprite->SetPivot(16.0f, 16.0f);  // テクスチャ中心を原点に（32x32の半分）

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
        sprite->SetPivot(16.0f, 16.0f);  // テクスチャ中心を原点に（32x32の半分）

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
    arrows_.clear();
    objects_.clear();
    player_.reset();
    background_.reset();
    cameraObj_.reset();
    testTexture_.reset();
    playerTexture_.reset();
    backgroundTexture_.reset();
    arrowTexture_.reset();
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

    InputManager* inputMgr = InputManager::GetInstance();
    if (!inputMgr) return;

    Keyboard& keyboard = inputMgr->GetKeyboard();

    Mouse& mouse = inputMgr->GetMouse();

    // クリックで攻撃開始＆矢を発射
    if (mouse.IsButtonDown(MouseButton::Left) && !isAttacking_) {
        isAttacking_ = true;
        playerAnimator_->SetRow(2);  // Attack
        playerAnimator_->SetLooping(false);
        playerAnimator_->Reset();

        // マウス位置からプレイヤーへの方向で左右反転
        Vector2 mousePos = Vector2(static_cast<float>(mouse.GetX()), static_cast<float>(mouse.GetY()));
        Vector2 playerScreenPos = camera_->WorldToScreen(playerTransform_->GetPosition());
        if (mousePos.x < playerScreenPos.x) {
            playerAnimator_->SetMirror(false);  // 左向き
        } else {
            playerAnimator_->SetMirror(true);   // 右向き
        }

        // 矢を発射
        Vector2 mouseWorld = camera_->ScreenToWorld(mousePos);
        Vector2 playerPos = playerTransform_->GetPosition();
        Vector2 direction = mouseWorld - playerPos;
        float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
        if (length > 0.001f) {
            direction.x /= length;
            direction.y /= length;
        }
        float arrowSpeed = 800.0f;
        float rotation = std::atan2(direction.y, direction.x);

        Arrow arrow;
        arrow.position = playerPos;
        arrow.velocity = Vector2(direction.x * arrowSpeed, direction.y * arrowSpeed);
        arrow.rotation = rotation;
        arrow.lifetime = 3.0f;
        arrows_.push_back(arrow);
    }

    // 攻撃アニメーション終了チェック
    if (isAttacking_) {
        // 攻撃の最終フレームに達したら攻撃終了
        if (playerAnimator_->GetColumn() >= 2) {  // Attack: 3フレーム（0,1,2）
            isAttacking_ = false;
            playerAnimator_->SetLooping(true);
            playerAnimator_->SetRow(0);  // Idle
        }
    }

    // 攻撃中は移動しない
    if (!isAttacking_) {
        // プレイヤー操作（WASD）
        const float speed = 300.0f;
        Vector2 move = Vector2::Zero;
        if (keyboard.IsKeyPressed(Key::W)) move.y -= speed * dt;
        if (keyboard.IsKeyPressed(Key::S)) move.y += speed * dt;
        if (keyboard.IsKeyPressed(Key::A)) move.x -= speed * dt;
        if (keyboard.IsKeyPressed(Key::D)) move.x += speed * dt;

        // 左右移動で反転
        if (move.x < 0.0f) {
            playerAnimator_->SetMirror(false);  // 左向き
        } else if (move.x > 0.0f) {
            playerAnimator_->SetMirror(true);   // 右向き
        }

        // 移動中はWalk、静止中はIdle
        if (move.x != 0.0f || move.y != 0.0f) {
            playerTransform_->Translate(move);
            if (playerAnimator_->GetRow() != 1) {
                playerAnimator_->SetRow(1);  // Walk
            }
        } else {
            if (playerAnimator_->GetRow() != 0) {
                playerAnimator_->SetRow(0);  // Idle
            }
        }
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

    // 矢の更新
    for (Arrow& arrow : arrows_) {
        arrow.position.x += arrow.velocity.x * dt;
        arrow.position.y += arrow.velocity.y * dt;
        arrow.lifetime -= dt;
    }
    // 寿命切れの矢を削除
    arrows_.erase(
        std::remove_if(arrows_.begin(), arrows_.end(),
            [](const Arrow& a) { return a.lifetime <= 0.0f; }),
        arrows_.end()
    );
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

    // プレイヤー描画（Animator使用）
    if (playerTransform_ && playerSprite_ && playerAnimator_) {
        spriteBatch.Draw(*playerSprite_, *playerTransform_, *playerAnimator_);
    }

    // 矢を描画
    for (const Arrow& arrow : arrows_) {
        spriteBatch.Draw(
            arrowTexture_.get(),
            arrow.position,
            Colors::White,
            arrow.rotation,
            Vector2(16.0f, 4.0f),  // origin: テクスチャ中心（32x8の半分）
            Vector2::One,
            false, false,
            50, 0  // sortingLayer: プレイヤーより下
        );
    }

    // デバッグ：コライダー枠描画
    const float lineWidth = 2.0f;  // 枠線の太さ

    // 枠を描画するラムダ関数
    auto drawColliderOutline = [&](Vector2 pos, Vector2 size, const Color& color, bool centered) {
        float left, top;
        if (centered) {
            left = pos.x - size.x * 0.5f;
            top = pos.y - size.y * 0.5f;
        } else {
            left = pos.x;
            top = pos.y;
        }
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
    Color playerColliderColor(0.0f, 1.0f, 0.0f, 1.0f);
    if (player_) {
        Collider2D* collider = player_->GetComponent<Collider2D>();
        if (collider && playerTransform_) {
            Vector2 pos = playerTransform_->GetPosition();
            Vector2 offset = collider->GetOffset();
            pos.x += offset.x;
            pos.y += offset.y;
            Vector2 size = collider->GetSize();
            drawColliderOutline(pos, size, playerColliderColor, true);  // 中心基準
        }
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
            drawColliderOutline(pos, size, obstacleColliderColor, true);  // 中心基準
        }
    }

    spriteBatch.End();
}
