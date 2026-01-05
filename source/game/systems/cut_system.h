//----------------------------------------------------------------------------
//! @file   cut_system.h
//! @brief  切システム - プレイヤーが縁を切る操作を管理
//----------------------------------------------------------------------------
#pragma once

#include "game/bond/bond.h"
#include <functional>
#include <optional>
#include <memory>
#include <cassert>

//----------------------------------------------------------------------------
//! @brief 切システム（シングルトン）
//! @details 時間停止中に既存の縁を切る操作を管理
//----------------------------------------------------------------------------
class CutSystem
{
public:
    //! @brief シングルトンインスタンス取得
    static CutSystem& Get();

    //! @brief インスタンス生成
    static void Create();

    //! @brief インスタンス破棄
    static void Destroy();

    //! @brief デストラクタ
    ~CutSystem();

    //------------------------------------------------------------------------
    // モード制御
    //------------------------------------------------------------------------

    //! @brief 切モードを有効化
    void Enable();

    //! @brief 切モードを無効化
    void Disable();

    //! @brief 切モードの有効/無効を切り替え
    void Toggle();

    //! @brief 切モードが有効か判定
    [[nodiscard]] bool IsEnabled() const { return isEnabled_; }

    //------------------------------------------------------------------------
    // 縁選択・切断
    //------------------------------------------------------------------------

    //! @brief 縁を選択
    //! @param bond 選択する縁
    void SelectBond(Bond* bond);

    //! @brief 選択中の縁を切断
    //! @return 切断成功ならtrue
    bool CutSelectedBond();

    //! @brief 指定した縁を直接切断
    //! @param bond 切断する縁
    //! @return 切断成功ならtrue
    bool CutBond(Bond* bond);

    //! @brief 選択をクリア
    void ClearSelection();

    //! @brief 選択中の縁を取得
    [[nodiscard]] Bond* GetSelectedBond() const { return selectedBond_; }

    //! @brief 縁が選択されているか判定
    [[nodiscard]] bool HasSelection() const { return selectedBond_ != nullptr; }

    //------------------------------------------------------------------------
    // 判定
    //------------------------------------------------------------------------

    //! @brief 縁を切れるか判定
    //! @param bond 対象の縁
    //! @return 切れる場合true
    [[nodiscard]] bool CanCut(Bond* bond) const;

    //------------------------------------------------------------------------
    // FEコスト
    //------------------------------------------------------------------------

    //! @brief 縁を切るのに必要なFEを取得
    [[nodiscard]] float GetCutCost() const { return cutCost_; }

    //! @brief FEコストを設定
    void SetCutCost(float cost) { cutCost_ = cost; }

    //------------------------------------------------------------------------
    // 回数制限
    //------------------------------------------------------------------------

    //! @brief 切る回数上限を設定（-1で無制限）
    void SetMaxCutCount(int count) {
        assert((count == -1 || count >= 0) && "maxCutCount must be -1 or non-negative");
        maxCutCount_ = count;
    }

    //! @brief 切る回数上限を取得
    [[nodiscard]] int GetMaxCutCount() const { return maxCutCount_; }

    //! @brief 残り回数を取得（-1なら無制限）
    [[nodiscard]] int GetRemainingCuts() const {
        if (maxCutCount_ < 0) return -1;
        return maxCutCount_ - currentCutCount_;
    }

    //! @brief 現在の使用回数を取得
    [[nodiscard]] int GetCurrentCutCount() const { return currentCutCount_; }

    //! @brief 回数をリセット
    void ResetCutCount() { currentCutCount_ = 0; }

    //! @brief 回数制限を考慮して切れるか判定
    [[nodiscard]] bool CanCutWithLimit() const {
        if (maxCutCount_ < 0) return true;  // 無制限
        return currentCutCount_ < maxCutCount_;
    }

    //------------------------------------------------------------------------
    // コールバック
    //------------------------------------------------------------------------

    //! @brief モード変更時コールバック
    void SetOnModeChanged(std::function<void(bool)> callback) { onModeChanged_ = callback; }

    //! @brief 縁選択時コールバック
    void SetOnBondSelected(std::function<void(Bond*)> callback) { onBondSelected_ = callback; }

    //! @brief 縁切断時コールバック
    void SetOnBondCut(std::function<void(const BondableEntity&, const BondableEntity&)> callback)
    {
        onBondCut_ = callback;
    }

private:
    CutSystem();
    CutSystem(const CutSystem&) = delete;
    CutSystem& operator=(const CutSystem&) = delete;

    //! @brief BondRemovedEventハンドラ
    void OnBondRemoved(const BondableEntity& a, const BondableEntity& b);

    static inline std::unique_ptr<CutSystem> instance_ = nullptr;

    bool isEnabled_ = false;        //!< モード有効フラグ
    Bond* selectedBond_ = nullptr;  //!< 選択中の縁
    BondableEntity selectedEntityA_;  //!< 選択時のエンティティA（use-after-free防止用）
    BondableEntity selectedEntityB_;  //!< 選択時のエンティティB（use-after-free防止用）
    float cutCost_ = 10.0f;         //!< 縁を切るFEコスト
    int maxCutCount_ = -1;          //!< 切る回数上限（-1=無制限）
    int currentCutCount_ = 0;       //!< 現在の使用回数

    //! @brief BondRemovedEventの購読ID
    uint32_t bondRemovedSubscriptionId_ = 0;

    // コールバック
    std::function<void(bool)> onModeChanged_;
    std::function<void(Bond*)> onBondSelected_;
    std::function<void(const BondableEntity&, const BondableEntity&)> onBondCut_;
};
