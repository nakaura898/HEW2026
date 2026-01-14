//----------------------------------------------------------------------------
//! @file   melee_attack_behavior.cpp
//! @brief  近接攻撃行動（Knight用）実装
//----------------------------------------------------------------------------
#include "melee_attack_behavior.h"
#include "game/entities/knight.h"
#include "game/entities/player.h"
#include "game/systems/relationship_context.h"
#include "engine/component/collider2d.h"
#include "common/logging/logging.h"
#include <cmath>

//----------------------------------------------------------------------------
MeleeAttackBehavior::MeleeAttackBehavior(Knight* owner)
    : owner_(owner)
{
}

//----------------------------------------------------------------------------
void MeleeAttackBehavior::OnAttackStart(Individual* attacker, Individual* target)
{
    attackTarget_ = target;
    playerTarget_ = nullptr;
    hasHitTarget_ = false;

    // 関係レジストリに攻撃関係を登録
    if (attacker && target) {
        RelationshipContext::Get().RegisterAttack(attacker, target);
    }

    if (target) {
        StartSwordSwing(target->GetPosition());
    }
}

//----------------------------------------------------------------------------
void MeleeAttackBehavior::OnAttackStartPlayer(Individual* attacker, Player* target)
{
    attackTarget_ = nullptr;
    playerTarget_ = target;
    hasHitTarget_ = false;

    // 関係レジストリに攻撃関係を登録
    if (attacker && target) {
        RelationshipContext::Get().RegisterAttackPlayer(attacker, target);
    }

    if (target) {
        StartSwordSwing(target->GetPosition());
    }
}

//----------------------------------------------------------------------------
void MeleeAttackBehavior::OnAttackUpdate(float /*dt*/, AnimState phase, float phaseTime)
{
    // AttackActive中のみ剣の角度を更新
    if (phase == AnimState::AttackActive && isSwinging_) {
        float progress = phaseTime / kSwingDuration;
        if (progress > 1.0f) {
            progress = 1.0f;
        }
        swingAngle_ = kSwingStartAngle + (kSwingEndAngle - kSwingStartAngle) * progress;
    }
}

//----------------------------------------------------------------------------
bool MeleeAttackBehavior::OnDamageFrame()
{
    // 既にヒット済みならスキップ
    if (hasHitTarget_) {
        return true;  // ダメージ適用済み
    }

    // 剣振り中でなければスキップ
    if (!isSwinging_) {
        return false;
    }

    // 当たり判定チェック
    return CheckSwordHit();
}

//----------------------------------------------------------------------------
void MeleeAttackBehavior::OnAttackEnd()
{
    // 関係レジストリから攻撃関係を解除
    if (owner_) {
        RelationshipContext::Get().UnregisterAttack(owner_);
    }

    attackTarget_ = nullptr;
    playerTarget_ = nullptr;
    isSwinging_ = false;
    hasHitTarget_ = false;
    swingAngle_ = 0.0f;
}

//----------------------------------------------------------------------------
void MeleeAttackBehavior::OnAttackInterrupt()
{
    // 中断時も同様にクリア
    OnAttackEnd();
}

//----------------------------------------------------------------------------
float MeleeAttackBehavior::GetWindupDuration() const
{
    // Knightは予備動作なし（即座に剣を振り始める）
    return 0.0f;
}

//----------------------------------------------------------------------------
float MeleeAttackBehavior::GetActiveDuration() const
{
    return kSwingDuration;
}

//----------------------------------------------------------------------------
float MeleeAttackBehavior::GetRecoveryDuration() const
{
    return kRecoveryDuration;
}

//----------------------------------------------------------------------------
float MeleeAttackBehavior::GetDamageFrameTime() const
{
    // 剣振り開始時点から当たり判定チェック開始
    return 0.0f;
}

//----------------------------------------------------------------------------
bool MeleeAttackBehavior::GetTargetPosition(Vector2& outPosition) const
{
    if (attackTarget_ && attackTarget_->IsAlive()) {
        outPosition = attackTarget_->GetPosition();
        return true;
    }

    if (playerTarget_ && playerTarget_->IsAlive()) {
        outPosition = playerTarget_->GetPosition();
        return true;
    }

    return false;
}

//----------------------------------------------------------------------------
void MeleeAttackBehavior::StartSwordSwing(const Vector2& targetPos)
{
    if (!owner_) return;

    isSwinging_ = true;
    swingAngle_ = kSwingStartAngle;
    hasHitTarget_ = false;

    // ターゲット方向を計算
    Vector2 myPos = owner_->GetPosition();
    Vector2 diff = targetPos - myPos;
    float length = diff.Length();

    if (length > kMinLength) {
        swingDirection_ = diff / length;
    } else {
        swingDirection_ = Vector2(1.0f, 0.0f);
    }

}

//----------------------------------------------------------------------------
bool MeleeAttackBehavior::CheckSwordHit()
{
    if (!owner_) return false;

    Vector2 swordTip = CalculateSwordTip();
    float damage = owner_->GetAttackDamage();

    // Individual対象の場合
    if (attackTarget_ != nullptr && attackTarget_->IsAlive()) {
        Collider2D* targetCollider = attackTarget_->GetCollider();
        if (targetCollider != nullptr) {
            AABB targetAABB = targetCollider->GetAABB();

            if (targetAABB.Contains(swordTip.x, swordTip.y)) {
                attackTarget_->TakeDamage(damage);
                hasHitTarget_ = true;
                return true;
            }
        }
        return false;
    }

    // Player対象の場合
    if (playerTarget_ != nullptr && playerTarget_->IsAlive()) {
        Collider2D* targetCollider = playerTarget_->GetCollider();
        if (targetCollider != nullptr) {
            AABB targetAABB = targetCollider->GetAABB();

            if (targetAABB.Contains(swordTip.x, swordTip.y)) {
                playerTarget_->TakeDamage(damage);
                hasHitTarget_ = true;
                return true;
            }
        }
    }

    return false;
}

//----------------------------------------------------------------------------
Vector2 MeleeAttackBehavior::CalculateSwordTip() const
{
    if (!owner_) return Vector2(0.0f, 0.0f);

    Vector2 myPos = owner_->GetPosition();

    // 基準方向からの角度を計算
    float baseAngle = std::atan2(swingDirection_.y, swingDirection_.x);
    float totalAngle = baseAngle + swingAngle_ * kDegToRad;

    // 剣先の位置
    Vector2 swordTip;
    swordTip.x = myPos.x + std::cos(totalAngle) * kSwordLength;
    swordTip.y = myPos.y + std::sin(totalAngle) * kSwordLength;

    return swordTip;
}
