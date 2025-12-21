//----------------------------------------------------------------------------
//! @file   knight.cpp
//! @brief  Knight種族クラス実装
//----------------------------------------------------------------------------
#include "knight.h"
#include "player.h"
#include "engine/texture/texture_manager.h"
#include "engine/c_systems/sprite_batch.h"
#include "engine/c_systems/collision_manager.h"
#include "engine/c_systems/collision_layers.h"
#include "engine/debug/debug_draw.h"
#include "engine/math/color.h"
#include "common/logging/logging.h"
#include <vector>
#include <cmath>

namespace {
    constexpr float kPi = 3.14159265358979323846f;
    constexpr float kDegToRad = kPi / 180.0f;
}

//----------------------------------------------------------------------------
Knight::Knight(const std::string& id)
    : Individual(id)
{
    // Knightはアニメーションなし（単一フレーム）
    animRows_ = 1;
    animCols_ = 1;
    animFrameInterval_ = 1;

    // ステータス設定（タンク型）
    maxHp_ = kDefaultHp;
    hp_ = maxHp_;
    attackDamage_ = kDefaultDamage;
    moveSpeed_ = kDefaultSpeed;
}

//----------------------------------------------------------------------------
void Knight::SetupTexture()
{
    // 白い■テクスチャを動的生成
    std::vector<uint32_t> pixels(kTextureSize * kTextureSize, 0xFFFFFFFF);
    texture_ = TextureManager::Get().Create2D(
        kTextureSize, kTextureSize,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        D3D11_BIND_SHADER_RESOURCE,
        pixels.data(),
        kTextureSize * sizeof(uint32_t)
    );

    if (sprite_ && texture_) {
        sprite_->SetTexture(texture_.get());
        sprite_->SetSortingLayer(10);

        // 色を設定（白テクスチャに乗算）
        sprite_->SetColor(color_);

        // Pivot設定（中心）
        sprite_->SetPivot(
            static_cast<float>(kTextureSize) * 0.5f,
            static_cast<float>(kTextureSize) * 0.5f
        );

        // サイズ設定（少し大きめ）
        sprite_->SetSize(Vector2(48.0f, 48.0f));
    }
}

//----------------------------------------------------------------------------
void Knight::SetupCollider()
{
    // 基底クラスのコライダー設定を呼び出し
    Individual::SetupCollider();

    // Knightは少し大きめのコライダーにリサイズ
    if (collider_ != nullptr) {
        collider_->SetBounds(Vector2(-24, -24), Vector2(24, 24));
    }
}

//----------------------------------------------------------------------------
void Knight::SetColor(const Color& color)
{
    color_ = color;
    if (sprite_) {
        sprite_->SetColor(color_);
    }
}

//----------------------------------------------------------------------------
void Knight::Update(float dt)
{
    // 基底クラスの更新
    Individual::Update(dt);

    // 剣振りアニメーション更新
    if (isSwinging_) {
        swingTimer_ += dt;

        // 剣振り進行度（0.0〜1.0）
        float progress = swingTimer_ / kSwingDuration;
        if (progress >= 1.0f) {
            // 剣振り終了
            isSwinging_ = false;
            swingTimer_ = 0.0f;
            hasHitTarget_ = false;
        } else {
            // 角度を補間
            swingAngle_ = kSwingStartAngle + (kSwingEndAngle - kSwingStartAngle) * progress;

            // 当たり判定チェック（まだヒットしていない場合のみ）
            if (!hasHitTarget_) {
                CheckSwordHit();
            }
        }
    }
}

//----------------------------------------------------------------------------
void Knight::Render(SpriteBatch& spriteBatch)
{
    // 基底クラスの描画
    Individual::Render(spriteBatch);

    // 剣振りエフェクト描画
    if (isSwinging_ && IsAlive()) {
        Vector2 myPos = GetPosition();
        Vector2 swordTip = CalculateSwordTip();

        // 剣の色（白〜銀色）
        Color swordColor(0.9f, 0.9f, 1.0f, 1.0f);

        // 剣を線で描画
        DEBUG_LINE(myPos, swordTip, swordColor, 3.0f);

        // 剣先に小さな円を描画（ヒット判定可視化）
        DEBUG_CIRCLE(swordTip, 8.0f, swordColor);
    }
}

//----------------------------------------------------------------------------
void Knight::Attack(Individual* target)
{
    if (target == nullptr || !target->IsAlive()) return;
    if (!IsAlive()) return;

    // ターゲットを保存（CheckSwordHitで使用）
    attackTarget_ = target;
    playerTarget_ = nullptr;

    StartSwordSwing(target->GetPosition());
    LOG_INFO("[Knight] " + id_ + " starts sword swing at " + target->GetId());
}

//----------------------------------------------------------------------------
void Knight::AttackPlayer(Player* target)
{
    if (target == nullptr || !target->IsAlive()) return;
    if (!IsAlive()) return;

    // ターゲットを保存（CheckSwordHitで使用）
    attackTarget_ = nullptr;
    playerTarget_ = target;

    StartSwordSwing(target->GetPosition());
    LOG_INFO("[Knight] " + id_ + " starts sword swing at Player");
}

//----------------------------------------------------------------------------
void Knight::StartSwordSwing(const Vector2& targetPos)
{
    if (isSwinging_) return;

    isSwinging_ = true;
    swingTimer_ = 0.0f;
    swingAngle_ = kSwingStartAngle;
    hasHitTarget_ = false;

    // ターゲット方向を計算
    Vector2 myPos = GetPosition();
    Vector2 diff = targetPos - myPos;
    float length = diff.Length();

    constexpr float kMinLength = 0.001f;
    if (length > kMinLength) {
        swingDirection_ = diff / length;
    } else {
        swingDirection_ = Vector2(1.0f, 0.0f);
    }
}

//----------------------------------------------------------------------------
void Knight::CheckSwordHit()
{
    Vector2 swordTip = CalculateSwordTip();

    // Individual対象の場合
    if (attackTarget_ != nullptr && attackTarget_->IsAlive()) {
        Collider2D* targetCollider = attackTarget_->GetCollider();
        if (targetCollider != nullptr) {
            AABB targetAABB = targetCollider->GetAABB();

            if (targetAABB.Contains(swordTip.x, swordTip.y)) {
                attackTarget_->TakeDamage(attackDamage_);
                hasHitTarget_ = true;

                LOG_INFO("[Knight] " + id_ + " sword hit " + attackTarget_->GetId() +
                         " for " + std::to_string(attackDamage_) + " damage");
            }
        }
        return;
    }

    // Player対象の場合
    if (playerTarget_ != nullptr && playerTarget_->IsAlive()) {
        Collider2D* targetCollider = playerTarget_->GetCollider();
        if (targetCollider != nullptr) {
            AABB targetAABB = targetCollider->GetAABB();

            if (targetAABB.Contains(swordTip.x, swordTip.y)) {
                playerTarget_->TakeDamage(attackDamage_);
                hasHitTarget_ = true;

                LOG_INFO("[Knight] " + id_ + " sword hit Player for " +
                         std::to_string(attackDamage_) + " damage");
            }
        }
    }
}

//----------------------------------------------------------------------------
Vector2 Knight::CalculateSwordTip() const
{
    Vector2 myPos = GetPosition();

    // 基準方向からの角度を計算
    float baseAngle = std::atan2(swingDirection_.y, swingDirection_.x);
    float totalAngle = baseAngle + swingAngle_ * kDegToRad;

    // 剣先の位置
    Vector2 swordTip;
    swordTip.x = myPos.x + std::cos(totalAngle) * kSwordLength;
    swordTip.y = myPos.y + std::sin(totalAngle) * kSwordLength;

    return swordTip;
}
