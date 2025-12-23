//----------------------------------------------------------------------------
//! @file   bond_type_selector.h
//! @brief  縁タイプ選択システム - Basic/Friends/Loveの切り替えを管理
//----------------------------------------------------------------------------
#pragma once

#include "game/bond/bond.h"
#include <functional>

//----------------------------------------------------------------------------
//! @brief 縁タイプ選択システム（シングルトン）
//! @details 結モード中に縁のタイプを選択するためのシステム
//----------------------------------------------------------------------------
class BondTypeSelector
{
public:
    //! @brief シングルトンインスタンス取得
    static BondTypeSelector& Get();

    //------------------------------------------------------------------------
    // タイプ選択
    //------------------------------------------------------------------------

    //! @brief 次のタイプに切り替え（Basic -> Friends -> Love -> Basic）
    void CycleNextType();

    //! @brief 前のタイプに切り替え（Basic -> Love -> Friends -> Basic）
    void CyclePrevType();

    //! @brief 現在選択中のタイプを取得
    [[nodiscard]] BondType GetCurrentType() const { return currentType_; }

    //! @brief タイプを直接設定
    void SetCurrentType(BondType type);

    //! @brief タイプをリセット（Basicに戻す）
    void Reset();

    //------------------------------------------------------------------------
    // ヘルパー
    //------------------------------------------------------------------------

    //! @brief 縁タイプの名前を取得
    [[nodiscard]] static const char* GetTypeName(BondType type);

    //------------------------------------------------------------------------
    // コールバック
    //------------------------------------------------------------------------

    //! @brief タイプ変更時コールバック設定
    void SetOnTypeChanged(std::function<void(BondType)> callback)
    {
        onTypeChanged_ = callback;
    }

private:
    BondTypeSelector() = default;
    ~BondTypeSelector() = default;
    BondTypeSelector(const BondTypeSelector&) = delete;
    BondTypeSelector& operator=(const BondTypeSelector&) = delete;

    BondType currentType_ = BondType::Basic;    //!< 現在選択中のタイプ

    // コールバック
    std::function<void(BondType)> onTypeChanged_;
};
