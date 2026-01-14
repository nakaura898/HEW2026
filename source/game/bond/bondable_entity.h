//----------------------------------------------------------------------------
//! @file   bondable_entity.h
//! @brief  縁に参加できるエンティティの型定義とヘルパー関数
//----------------------------------------------------------------------------
#pragma once

#include "engine/math/math_types.h"
#include <variant>
#include <string>

// 前方宣言
class Player;
class Group;

//----------------------------------------------------------------------------
//! @brief 縁に参加できるエンティティ型
//! @details Player と Group のみが縁を結べる
//----------------------------------------------------------------------------
using BondableEntity = std::variant<Player*, Group*>;

//----------------------------------------------------------------------------
//! @brief BondableEntity ヘルパー関数群
//----------------------------------------------------------------------------
namespace BondableHelper
{
    //! @brief エンティティのIDを取得
    [[nodiscard]] std::string GetId(const BondableEntity& entity);

    //! @brief エンティティの位置を取得
    [[nodiscard]] Vector2 GetPosition(const BondableEntity& entity);

    //! @brief エンティティの脅威度を取得
    [[nodiscard]] float GetThreat(const BondableEntity& entity);

    //! @brief Playerかどうか判定
    [[nodiscard]] bool IsPlayer(const BondableEntity& entity);

    //! @brief Groupかどうか判定
    [[nodiscard]] bool IsGroup(const BondableEntity& entity);

    //! @brief Player*に変換（失敗時nullptr）
    [[nodiscard]] Player* AsPlayer(const BondableEntity& entity);

    //! @brief Group*に変換（失敗時nullptr）
    [[nodiscard]] Group* AsGroup(const BondableEntity& entity);

    //! @brief 2つのエンティティが同一か判定
    [[nodiscard]] bool IsSame(const BondableEntity& a, const BondableEntity& b);

    //! @brief エンティティがnullか判定
    [[nodiscard]] bool IsNull(const BondableEntity& entity);
}
