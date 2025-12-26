//----------------------------------------------------------------------------
//! @file   insulation_system.h
//! @brief  絶縁システム - 縁を切られたペア間の再接続を禁止
//----------------------------------------------------------------------------
#pragma once

#include "game/bond/bondable_entity.h"
#include <set>
#include <functional>
#include <utility>

//----------------------------------------------------------------------------
//! @brief 絶縁システム（シングルトン）
//! @details 縁を切られたペアは再び縁を結べない
//----------------------------------------------------------------------------
class InsulationSystem
{
public:
    //! @brief シングルトンインスタンス取得
    static InsulationSystem& Get();

    //------------------------------------------------------------------------
    // 絶縁操作
    //------------------------------------------------------------------------

    //! @brief ペアを絶縁状態にする
    //! @param a エンティティA
    //! @param b エンティティB
    void AddInsulation(const BondableEntity& a, const BondableEntity& b);

    //! @brief ペアが絶縁状態かどうか判定
    //! @param a エンティティA
    //! @param b エンティティB
    //! @return 絶縁状態ならtrue
    [[nodiscard]] bool IsInsulated(const BondableEntity& a, const BondableEntity& b) const;

    //! @brief 絶縁を解除（通常は使用しない）
    //! @param a エンティティA
    //! @param b エンティティB
    void RemoveInsulation(const BondableEntity& a, const BondableEntity& b);

    //------------------------------------------------------------------------
    // エンティティ死亡処理
    //------------------------------------------------------------------------

    //! @brief エンティティが倒された時に関連する絶縁情報を削除
    //! @param entity 倒されたエンティティ
    void OnEntityDefeated(const BondableEntity& entity);

    //------------------------------------------------------------------------
    // クリア
    //------------------------------------------------------------------------

    //! @brief 全ての絶縁情報をクリア
    void Clear();

    //! @brief 絶縁ペア数を取得
    [[nodiscard]] size_t GetInsulationCount() const { return insulatedPairs_.size(); }

    //------------------------------------------------------------------------
    // コールバック
    //------------------------------------------------------------------------

    //! @brief 絶縁追加時コールバック
    void SetOnInsulationAdded(std::function<void(const BondableEntity&, const BondableEntity&)> callback)
    {
        onInsulationAdded_ = callback;
    }

private:
    InsulationSystem() = default;
    ~InsulationSystem() = default;
    InsulationSystem(const InsulationSystem&) = delete;
    InsulationSystem& operator=(const InsulationSystem&) = delete;

    //! @brief 絶縁ペアのキーを生成（順序不問）
    [[nodiscard]] std::pair<std::string, std::string> MakePairKey(
        const BondableEntity& a, const BondableEntity& b) const;

    //! @brief 絶縁ペアの集合
    std::set<std::pair<std::string, std::string>> insulatedPairs_;

    // コールバック
    std::function<void(const BondableEntity&, const BondableEntity&)> onInsulationAdded_;
};
