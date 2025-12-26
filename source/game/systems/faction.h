//----------------------------------------------------------------------------
//! @file   faction.h
//! @brief  Faction（陣営）- 縁で繋がったエンティティ群
//----------------------------------------------------------------------------
#pragma once

#include "game/bond/bondable_entity.h"
#include <vector>

//----------------------------------------------------------------------------
//! @brief Faction（陣営）
//! @details 縁で繋がったエンティティ群。同じFaction内は攻撃しない
//----------------------------------------------------------------------------
class Faction
{
public:
    //! @brief メンバーを追加
    void AddMember(BondableEntity entity);

    //! @brief メンバーをクリア
    void Clear();

    //! @brief 指定エンティティが所属しているか判定
    [[nodiscard]] bool Contains(BondableEntity entity) const;

    //! @brief プレイヤーが所属しているか判定
    [[nodiscard]] bool HasPlayer() const;

    //! @brief 合計脅威度を取得
    //! @param playerBonus プレイヤー所属時のボーナス倍率
    [[nodiscard]] float GetTotalThreat(float playerBonus = 1.5f) const;

    //! @brief 全メンバーを取得
    [[nodiscard]] const std::vector<BondableEntity>& GetMembers() const { return members_; }

    //! @brief Groupメンバーのみを取得（AI用）
    [[nodiscard]] std::vector<Group*> GetGroups() const;

    //! @brief メンバー数を取得
    [[nodiscard]] size_t GetMemberCount() const { return members_.size(); }

private:
    std::vector<BondableEntity> members_;
};
