//----------------------------------------------------------------------------
//! @file   knight.cpp
//! @brief  Knight種族クラス実装
//----------------------------------------------------------------------------
#include "knight.h"
#include "player.h"
#include "game/systems/animation/melee_attack_behavior.h"
#include "engine/texture/texture_manager.h"
#include "engine/c_systems/sprite_batch.h"
#include "engine/c_systems/collision_manager.h"
#include "engine/c_systems/collision_layers.h"
#include "engine/debug/debug_draw.h"
#include "engine/math/color.h"
#include "common/logging/logging.h"
#include <vector>

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
void Knight::SetupStateMachine()
{
    // 基底クラスのセットアップを呼び出し
    Individual::SetupStateMachine();

    // MeleeAttackBehaviorを設定（ポインタをキャッシュしてからownership移譲）
    if (stateMachine_) {
        auto behavior = std::make_unique<MeleeAttackBehavior>(this);
        cachedMeleeAttackBehavior_ = behavior.get();
        stateMachine_->SetAttackBehavior(std::move(behavior));
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
void Knight::Render(SpriteBatch& spriteBatch)
{
    // 基底クラスの描画
    Individual::Render(spriteBatch);

    // 剣振りエフェクト描画
    if (IsSwinging() && IsAlive()) {
        // キャッシュされたポインタを使用（dynamic_cast回避）
        if (cachedMeleeAttackBehavior_) {
            Vector2 myPos = GetPosition();
            Vector2 swordTip = cachedMeleeAttackBehavior_->CalculateSwordTip();

            // 剣の色（白〜銀色）
            Color swordColor(0.9f, 0.9f, 1.0f, 1.0f);

            // 剣を線で描画
            DEBUG_LINE(myPos, swordTip, swordColor, 3.0f);

            // 剣先に小さな円を描画（ヒット判定可視化）
            DEBUG_CIRCLE(swordTip, 8.0f, swordColor);
        }
    }
}

//----------------------------------------------------------------------------
void Knight::Attack(Individual* target)
{
    if (target == nullptr || !target->IsAlive()) return;
    if (!IsAlive()) return;

    // 攻撃状態に設定
    action_ = IndividualAction::Attack;

    // StateMachineに攻撃開始を委譲
    StartAttack(target);

    LOG_INFO("[Knight] " + id_ + " starts sword swing at " + target->GetId());
}

//----------------------------------------------------------------------------
void Knight::AttackPlayer(Player* target)
{
    if (target == nullptr || !target->IsAlive()) return;
    if (!IsAlive()) return;

    // 攻撃状態に設定
    action_ = IndividualAction::Attack;

    // StateMachineに攻撃開始を委譲
    StartAttackPlayer(target);

    LOG_INFO("[Knight] " + id_ + " starts sword swing at Player");
}

//----------------------------------------------------------------------------
bool Knight::IsSwinging() const
{
    // キャッシュされたポインタを使用（dynamic_cast回避）
    if (cachedMeleeAttackBehavior_) {
        return cachedMeleeAttackBehavior_->IsSwinging();
    }
    return false;
}
