//----------------------------------------------------------------------------
//! @file   player.cpp
//! @brief  プレイヤークラス実装
//----------------------------------------------------------------------------
#include "player.h"
#include "engine/input/input_manager.h"
#include "engine/texture/texture_manager.h"
#include "engine/c_systems/sprite_batch.h"
#include "engine/math/color.h"
#include "common/logging/logging.h"
#include <cmath>
#include <algorithm>

//----------------------------------------------------------------------------
void Player::Initialize(const Vector2& position)
{
    // テクスチャロード
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

    // GameObject作成
    gameObject_ = std::make_unique<GameObject>("Player");

    // Transform
    transform_ = gameObject_->AddComponent<Transform2D>();
    transform_->SetPosition(position);
    transform_->SetScale(0.3f);

    // SpriteRenderer
    sprite_ = gameObject_->AddComponent<SpriteRenderer>();
    sprite_->SetTexture(playerTexture_.get());
    sprite_->SetSortingLayer(10);

    // Animator
    animator_ = gameObject_->AddComponent<Animator>(kAnimRows, kAnimCols, 6);

    // Pivot設定
    if (playerTexture_) {
        float frameWidth = static_cast<float>(playerTexture_->Width()) / kAnimCols;
        float frameHeight = static_cast<float>(playerTexture_->Height()) / kAnimRows;
        sprite_->SetPivotFromCenter(frameWidth, frameHeight, 0.0f, 0.0f);
    }

    // アニメーション設定
    animator_->SetRowFrameCount(0, 1, 12);   // Idle: 1フレーム
    animator_->SetRowFrameCount(1, 4, 36);   // Walk: 4フレーム
    animator_->SetRowFrameCount(2, 3, 20);   // Attack: 3フレーム
    animator_->SetRowFrameCount(3, 2, 10);   // Death: 2フレーム
    animator_->SetRow(1);  // 初期はWalk

    // Collider
    collider_ = gameObject_->AddComponent<Collider2D>();
    collider_->SetBounds(Vector2(-30, -40), Vector2(30, 40));
    collider_->SetLayer(0x01);
    collider_->SetMask(0x02);

    // 衝突コールバック
    collider_->SetOnCollisionEnter([this](Collider2D* /*self*/, Collider2D* /*other*/) {
        ++collisionCount_;
        LOG_INFO("[Collision] Enter!");
    });

    collider_->SetOnCollisionExit([this](Collider2D* /*self*/, Collider2D* /*other*/) {
        LOG_INFO("[Collision] Exit!");
    });

    LOG_INFO("[Player] Initialized");
}

//----------------------------------------------------------------------------
void Player::Shutdown()
{
    arrows_.clear();
    gameObject_.reset();
    playerTexture_.reset();
    arrowTexture_.reset();
    transform_ = nullptr;
    sprite_ = nullptr;
    animator_ = nullptr;
    collider_ = nullptr;
}

//----------------------------------------------------------------------------
void Player::Update(float dt, Camera2D& camera)
{
    HandleInput(dt, camera);
    UpdateArrows(dt);

    if (gameObject_) {
        gameObject_->Update(dt);
    }
}

//----------------------------------------------------------------------------
void Player::HandleInput(float dt, Camera2D& camera)
{
    InputManager* inputMgr = InputManager::GetInstance();
    if (!inputMgr) return;

    Keyboard& keyboard = inputMgr->GetKeyboard();
    Mouse& mouse = inputMgr->GetMouse();

    // クリックで攻撃開始＆矢を発射
    if (mouse.IsButtonDown(MouseButton::Left) && !isAttacking_) {
        isAttacking_ = true;
        animator_->SetRow(2);  // Attack
        animator_->SetLooping(false);
        animator_->Reset();

        // マウス位置からプレイヤーへの方向で左右反転
        Vector2 mousePos = Vector2(
            static_cast<float>(mouse.GetX()),
            static_cast<float>(mouse.GetY())
        );
        Vector2 playerScreenPos = camera.WorldToScreen(transform_->GetPosition());
        if (mousePos.x < playerScreenPos.x) {
            animator_->SetMirror(false);  // 左向き
        } else {
            animator_->SetMirror(true);   // 右向き
        }

        // 矢を発射
        Vector2 mouseWorld = camera.ScreenToWorld(mousePos);
        FireArrow(mouseWorld);
    }

    // 攻撃アニメーション終了チェック
    if (isAttacking_) {
        if (animator_->GetColumn() >= 2) {
            isAttacking_ = false;
            animator_->SetLooping(true);
            animator_->SetRow(0);  // Idle
        }
    }

    // 攻撃中は移動しない
    if (!isAttacking_) {
        Vector2 move = Vector2::Zero;
        if (keyboard.IsKeyPressed(Key::W)) move.y -= kMoveSpeed * dt;
        if (keyboard.IsKeyPressed(Key::S)) move.y += kMoveSpeed * dt;
        if (keyboard.IsKeyPressed(Key::A)) move.x -= kMoveSpeed * dt;
        if (keyboard.IsKeyPressed(Key::D)) move.x += kMoveSpeed * dt;

        // 左右移動で反転
        if (move.x < 0.0f) {
            animator_->SetMirror(false);
        } else if (move.x > 0.0f) {
            animator_->SetMirror(true);
        }

        // 移動中はWalk、静止中はIdle
        if (move.x != 0.0f || move.y != 0.0f) {
            transform_->Translate(move);
            if (animator_->GetRow() != 1) {
                animator_->SetRow(1);  // Walk
            }
        } else {
            if (animator_->GetRow() != 0) {
                animator_->SetRow(0);  // Idle
            }
        }
    }
}

//----------------------------------------------------------------------------
void Player::FireArrow(const Vector2& targetWorld)
{
    Vector2 playerPos = transform_->GetPosition();
    Vector2 direction = targetWorld - playerPos;
    float length = std::sqrt(direction.x * direction.x + direction.y * direction.y);
    if (length > 0.001f) {
        direction.x /= length;
        direction.y /= length;
    }

    Arrow arrow;
    arrow.position = playerPos;
    arrow.velocity = Vector2(direction.x * kArrowSpeed, direction.y * kArrowSpeed);
    arrow.rotation = std::atan2(direction.y, direction.x);
    arrow.lifetime = kArrowLifetime;
    arrows_.push_back(arrow);
}

//----------------------------------------------------------------------------
void Player::UpdateArrows(float dt)
{
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
void Player::Render(SpriteBatch& spriteBatch)
{
    if (transform_ && sprite_ && animator_) {
        spriteBatch.Draw(*sprite_, *transform_, *animator_);
    }

    // 矢を描画
    for (const Arrow& arrow : arrows_) {
        spriteBatch.Draw(
            arrowTexture_.get(),
            arrow.position,
            Colors::White,
            arrow.rotation,
            Vector2(16.0f, 4.0f),  // origin: テクスチャ中心
            Vector2::One,
            false, false,
            50, 0
        );
    }
}

//----------------------------------------------------------------------------
void Player::RenderColliderDebug(SpriteBatch& spriteBatch, Texture* debugTexture)
{
    if (!collider_ || !transform_ || !debugTexture) return;

    const float lineWidth = 2.0f;
    Color color(0.0f, 1.0f, 0.0f, 1.0f);  // 緑

    Vector2 pos = transform_->GetPosition();
    Vector2 offset = collider_->GetOffset();
    pos.x += offset.x;
    pos.y += offset.y;
    Vector2 size = collider_->GetSize();

    float left = pos.x - size.x * 0.5f;
    float top = pos.y - size.y * 0.5f;
    float right = left + size.x;
    float bottom = top + size.y;

    // 上辺
    spriteBatch.Draw(debugTexture, Vector2(left, top), color, 0.0f,
        Vector2(0, 0), Vector2(size.x / 32.0f, lineWidth / 32.0f), false, false, 100, 0);
    // 下辺
    spriteBatch.Draw(debugTexture, Vector2(left, bottom - lineWidth), color, 0.0f,
        Vector2(0, 0), Vector2(size.x / 32.0f, lineWidth / 32.0f), false, false, 100, 0);
    // 左辺
    spriteBatch.Draw(debugTexture, Vector2(left, top), color, 0.0f,
        Vector2(0, 0), Vector2(lineWidth / 32.0f, size.y / 32.0f), false, false, 100, 0);
    // 右辺
    spriteBatch.Draw(debugTexture, Vector2(right - lineWidth, top), color, 0.0f,
        Vector2(0, 0), Vector2(lineWidth / 32.0f, size.y / 32.0f), false, false, 100, 0);
}
