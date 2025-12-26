//----------------------------------------------------------------------------
//! @file   animation_decision_context.cpp
//! @brief  アニメーション判定コンテキスト実装
//----------------------------------------------------------------------------
#include "animation_decision_context.h"
#include "game/ai/group_ai.h"
#include <sstream>

//----------------------------------------------------------------------------
bool AnimationDecisionContext::ShouldWalk() const
{
    // 攻撃中は判定しない（攻撃アニメーション優先）
    if (isAttacking) {
        return false;
    }

    // 優先度1: グループが移動中（GroupAIが移動と判断）
    if (isGroupMoving) {
        return true;
    }

    // 優先度2: Loveクラスターが移動中
    if (isInLoveCluster && isLoveClusterMoving) {
        return true;
    }

    // 優先度3: 希望速度が十分ある（分離オフセットは除外）
    // ※separationOffsetは回避動作なので歩きアニメの判定には使わない
    if (desiredVelocity.Length() > kVelocityEpsilon) {
        return true;
    }

    // 優先度4: 実際に移動している（前フレームで位置が変化した）
    if (isActuallyMoving) {
        return true;
    }

    // それ以外: Idle
    // ※distanceToSlotは削除: スロットからの微小なずれでWalkになるのを防ぐ
    return false;
}

//----------------------------------------------------------------------------
Vector2 AnimationDecisionContext::GetFacingDirection() const
{
    // 攻撃中は攻撃対象の方を向く
    if (isAttacking && (attackTarget || playerTarget)) {
        return attackTargetPosition;  // 相対方向ではなく絶対位置を返す（呼び出し側で計算）
    }

    // 移動中は速度方向
    if (velocity.LengthSquared() > 0.01f) {
        return velocity;
    }

    // デフォルト: 右向き
    return Vector2(1.0f, 0.0f);
}

//----------------------------------------------------------------------------
std::string AnimationDecisionContext::ToString() const
{
    std::ostringstream oss;
    oss << "AnimationDecisionContext {\n";
    oss << "  velocity: (" << velocity.x << ", " << velocity.y << ")\n";
    oss << "  distanceToSlot: " << distanceToSlot << "\n";
    oss << "  isActuallyMoving: " << (isActuallyMoving ? "true" : "false") << "\n";
    oss << "  isGroupMoving: " << (isGroupMoving ? "true" : "false") << "\n";
    oss << "  isInLoveCluster: " << (isInLoveCluster ? "true" : "false") << "\n";
    oss << "  isLoveClusterMoving: " << (isLoveClusterMoving ? "true" : "false") << "\n";
    oss << "  distanceToClusterCenter: " << distanceToClusterCenter << "\n";
    oss << "  isAttacking: " << (isAttacking ? "true" : "false") << "\n";
    oss << "  isUnderAttack: " << (isUnderAttack ? "true" : "false") << "\n";
    oss << "  attackers.count: " << attackers.size() << "\n";
    oss << "  ShouldWalk(): " << (ShouldWalk() ? "true" : "false") << "\n";
    oss << "}\n";
    return oss.str();
}
