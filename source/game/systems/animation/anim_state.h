//----------------------------------------------------------------------------
//! @file   anim_state.h
//! @brief  アニメーション状態定義
//----------------------------------------------------------------------------
#pragma once

#include <cstdint>

//----------------------------------------------------------------------------
//! @brief アニメーション状態（統一ステートマシン用）
//! @details Individual/Elf/Knight共通の状態定義
//----------------------------------------------------------------------------
enum class AnimState : uint8_t
{
    Idle,           //!< 待機（ループ、割り込み可）
    Walk,           //!< 移動（ループ、割り込み可）
    AttackWindup,   //!< 攻撃予備動作（割り込み不可）
    AttackActive,   //!< 攻撃実行中（ダメージフレーム発火、割り込み不可）
    AttackRecovery, //!< 攻撃後隙（一定時間後に割り込み可）
    Death,          //!< 死亡（永続、割り込み不可）

    Count           //!< 状態数
};

//----------------------------------------------------------------------------
//! @brief 攻撃中かどうか判定
//! @param state アニメーション状態
//! @return 攻撃フェーズ中ならtrue
//----------------------------------------------------------------------------
inline bool IsAttackState(AnimState state)
{
    return state == AnimState::AttackWindup ||
           state == AnimState::AttackActive ||
           state == AnimState::AttackRecovery;
}

//----------------------------------------------------------------------------
//! @brief 割り込み不可状態かどうか判定
//! @param state アニメーション状態
//! @return ロック状態ならtrue
//----------------------------------------------------------------------------
inline bool IsLockedState(AnimState state)
{
    return state == AnimState::AttackWindup ||
           state == AnimState::AttackActive ||
           state == AnimState::Death;
}
