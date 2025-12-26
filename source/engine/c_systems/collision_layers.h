//----------------------------------------------------------------------------
//! @file   collision_layers.h
//! @brief  衝突レイヤー定数定義
//----------------------------------------------------------------------------
#pragma once

#include <cstdint>

//----------------------------------------------------------------------------
//! @brief 衝突レイヤー定数
//! @details コライダーのレイヤーとマスク設定に使用する定数
//----------------------------------------------------------------------------
namespace CollisionLayer
{
    //! @brief プレイヤーレイヤー
    constexpr uint32_t Player = 0x01;

    //! @brief 個体（Individual）レイヤー
    constexpr uint32_t Individual = 0x04;

    //! @brief 矢レイヤー
    constexpr uint32_t Arrow = 0x08;

    //----------------------------------------------------------------------------
    // 事前定義マスク（よく使う組み合わせ）
    //----------------------------------------------------------------------------

    //! @brief プレイヤー用マスク（Individual + Arrow と衝突）
    constexpr uint32_t PlayerMask = Individual | Arrow;

    //! @brief 個体用マスク（Player + Individual + Arrow と衝突）
    constexpr uint32_t IndividualMask = Player | Individual | Arrow;

    //! @brief 矢用マスク（Player + Individual と衝突）
    constexpr uint32_t ArrowMask = Player | Individual;
}
