//----------------------------------------------------------------------------
//! @file   arrow.cpp
//! @brief  矢クラス実装
//----------------------------------------------------------------------------
#include "arrow.h"
#include "individual.h"
#include "player.h"
#include "engine/texture/texture_manager.h"
#include "engine/c_systems/sprite_batch.h"
#include "engine/c_systems/collision_manager.h"
#include "engine/c_systems/collision_layers.h"
#include "engine/time/time_manager.h"
#include "common/logging/logging.h"
#include <cmath>

//----------------------------------------------------------------------------
Arrow::Arrow(Individual* owner, Individual* target, float damage)
    : owner_(owner)
    , target_(target)
    , damage_(damage)
{
}

//----------------------------------------------------------------------------
Arrow::Arrow(Individual* owner, Player* targetPlayer, float damage)
    : owner_(owner)
    , targetPlayer_(targetPlayer)
    , damage_(damage)
{
}

//----------------------------------------------------------------------------
Arrow::~Arrow() = default;

//----------------------------------------------------------------------------
void Arrow::Initialize(const Vector2& startPos, const Vector2& targetPos)
{
    // GameObject作成
    gameObject_ = std::make_unique<GameObject>("Arrow");
    transform_ = gameObject_->AddComponent<Transform>(startPos);
    sprite_ = gameObject_->AddComponent<SpriteRenderer>();

    // コライダー設定（矢のサイズに合わせた小さなAABB）
    collider_ = gameObject_->AddComponent<Collider2D>(Vector2(20.0f, 10.0f));
    collider_->SetLayer(CollisionLayer::Arrow);
    collider_->SetMask(CollisionLayer::ArrowMask);

    // 衝突コールバック設定
    collider_->SetOnCollisionEnter([this](Collider2D* /*self*/, Collider2D* other) {
        if (!isActive_) {
            return;
        }

        // 時間停止中はダメージを与えない
        if (TimeManager::Get().IsFrozen()) {
            return;
        }

        // Individual対象
        if (target_ != nullptr && target_->GetCollider() == other) {
            if (target_->IsAlive()) {
                target_->TakeDamage(damage_);
                isActive_ = false;
                if (owner_ != nullptr) {
                    LOG_INFO("[Arrow] Hit! " + owner_->GetId() + " -> " + target_->GetId() +
                             " for " + std::to_string(damage_) + " damage");
                }
            }
            return;
        }

        // Player対象
        if (targetPlayer_ != nullptr && targetPlayer_->GetCollider() == other) {
            if (targetPlayer_->IsAlive()) {
                targetPlayer_->TakeDamage(damage_);
                isActive_ = false;
                if (owner_ != nullptr) {
                    LOG_INFO("[Arrow] Hit! " + owner_->GetId() + " -> Player for " +
                             std::to_string(damage_) + " damage");
                }
            }
            return;
        }
    });

    // 矢テクスチャをロード
    texture_ = TextureManager::Get().LoadTexture2D("Elf_arrow.png");

    if (sprite_ && texture_) {
        sprite_->SetTexture(texture_.get());
        sprite_->SetSortingLayer(15);

        // ピボットを中心に設定
        float texWidth = static_cast<float>(texture_->Width());
        float texHeight = static_cast<float>(texture_->Height());
        sprite_->SetPivotFromCenter(texWidth, texHeight, 0.0f, 0.0f);
    }

    // スケール設定（小さく）
    if (transform_) {
        transform_->SetScale(0.3f);
    }

    // 方向計算
    Vector2 diff = targetPos - startPos;
    float length = diff.Length();
    if (length > 0.0f) {
        direction_ = diff / length;
    } else {
        direction_ = Vector2(1.0f, 0.0f);
    }

    // 回転設定（テクスチャは左向き）
    if (transform_) {
        float angle = std::atan2(direction_.y, direction_.x);
        transform_->SetRotation(angle);
    }

    isActive_ = true;
    lifetime_ = 0.0f;
}

//----------------------------------------------------------------------------
void Arrow::Update(float dt)
{
    if (!isActive_) return;

    // 時間スケール適用
    float scaledDt = TimeManager::Get().GetScaledDeltaTime(dt);

    // 寿命チェック（スケール済み時間で）
    lifetime_ += scaledDt;
    if (lifetime_ >= kMaxLifetime) {
        isActive_ = false;
        return;
    }

    // 移動（スケール済み時間で）
    if (transform_) {
        Vector2 pos = transform_->GetPosition();
        pos.x += direction_.x * speed_ * scaledDt;
        pos.y += direction_.y * speed_ * scaledDt;
        transform_->SetPosition(pos);
    }

    // 命中チェックはCollisionManagerのコールバックで自動処理

    // GameObject更新（スケール済み時間で）
    if (gameObject_) {
        gameObject_->Update(scaledDt);
    }
}

//----------------------------------------------------------------------------
void Arrow::Render(SpriteBatch& spriteBatch)
{
    if (!isActive_) return;
    if (!transform_ || !sprite_) return;

    // SpriteBatchで描画
    spriteBatch.Draw(*sprite_, *transform_);
}

//----------------------------------------------------------------------------
Vector2 Arrow::GetPosition() const
{
    if (transform_) {
        return transform_->GetPosition();
    }
    return Vector2::Zero;
}

