//----------------------------------------------------------------------------
//! @file   bind_system.h
//! @brief  結システム - プレイヤーが縁を結ぶ操作を管理
//----------------------------------------------------------------------------
#pragma once

#include "game/bond/bond.h"
#include "game/bond/bondable_entity.h"
#include <functional>
#include <optional>
#include <memory>
#include <cassert>

//----------------------------------------------------------------------------
//! @brief 結システム（シングルトン）
//! @details 時間停止中に2つのエンティティ間に縁を結ぶ操作を管理
//----------------------------------------------------------------------------
class BindSystem
{
public:
    //! @brief シングルトンインスタンス取得
    static BindSystem& Get();

    //! @brief インスタンス生成
    static void Create();

    //! @brief インスタンス破棄
    static void Destroy();

    //! @brief デストラクタ
    ~BindSystem() = default;

    //------------------------------------------------------------------------
    // モード制御
    //------------------------------------------------------------------------

    //! @brief 結モードを有効化
    void Enable();

    //! @brief 結モードを無効化
    void Disable();

    //! @brief 結モードの有効/無効を切り替え
    void Toggle();

    //! @brief 結モードが有効か判定
    [[nodiscard]] bool IsEnabled() const { return isEnabled_; }

    //------------------------------------------------------------------------
    // マーク操作
    //------------------------------------------------------------------------

    //! @brief エンティティをマーク（1体目 or 2体目）
    //! @param entity マークするエンティティ
    //! @return 縁が作成されたらtrue
    bool MarkEntity(BondableEntity entity);

    //! @brief マークをクリア
    void ClearMark();

    //! @brief マーク済みエンティティを取得
    [[nodiscard]] std::optional<BondableEntity> GetMarkedEntity() const { return markedEntity_; }

    //! @brief マークがあるか判定
    [[nodiscard]] bool HasMark() const { return markedEntity_.has_value(); }

    //------------------------------------------------------------------------
    // 判定
    //------------------------------------------------------------------------

    //! @brief 2つのエンティティ間に縁を結べるか判定
    //! @param a エンティティA
    //! @param b エンティティB
    //! @return 結べる場合true
    [[nodiscard]] bool CanBind(const BondableEntity& a, const BondableEntity& b) const;

    //------------------------------------------------------------------------
    // FEコスト
    //------------------------------------------------------------------------

    //! @brief 縁を結ぶのに必要なFEを取得
    [[nodiscard]] float GetBindCost() const { return bindCost_; }

    //! @brief FEコストを設定
    void SetBindCost(float cost) { bindCost_ = cost; }

    //------------------------------------------------------------------------
    // 縁タイプ選択
    //------------------------------------------------------------------------

    //! @brief 次に作成する縁のタイプを取得
    [[nodiscard]] BondType GetPendingBondType() const { return pendingBondType_; }

    //! @brief 次に作成する縁のタイプを設定
    void SetPendingBondType(BondType type) { pendingBondType_ = type; }

    //------------------------------------------------------------------------
    // 回数制限
    //------------------------------------------------------------------------

    //! @brief 結ぶ回数上限を設定（-1で無制限）
    void SetMaxBindCount(int count) { maxBindCount_ = count; }

    //! @brief 結ぶ回数上限を取得
    [[nodiscard]] int GetMaxBindCount() const { return maxBindCount_; }

    //! @brief 残り回数を取得（-1なら無制限）
    [[nodiscard]] int GetRemainingBinds() const {
        if (maxBindCount_ < 0) return -1;
        return maxBindCount_ - currentBindCount_;
    }

    //! @brief 現在の使用回数を取得
    [[nodiscard]] int GetCurrentBindCount() const { return currentBindCount_; }

    //! @brief 回数をリセット
    void ResetBindCount() { currentBindCount_ = 0; }

    //! @brief 回数制限を考慮して結べるか判定
    [[nodiscard]] bool CanBindWithLimit() const {
        if (maxBindCount_ < 0) return true;  // 無制限
        return currentBindCount_ < maxBindCount_;
    }

    //------------------------------------------------------------------------
    // コールバック
    //------------------------------------------------------------------------

    //! @brief モード変更時コールバック
    void SetOnModeChanged(std::function<void(bool)> callback) { onModeChanged_ = callback; }

    //! @brief エンティティマーク時コールバック
    void SetOnEntityMarked(std::function<void(const BondableEntity&)> callback) { onEntityMarked_ = callback; }

    //! @brief 縁作成時コールバック
    void SetOnBondCreated(std::function<void(const BondableEntity&, const BondableEntity&)> callback)
    {
        onBondCreated_ = callback;
    }

private:
    BindSystem() = default;
    BindSystem(const BindSystem&) = delete;
    BindSystem& operator=(const BindSystem&) = delete;

    static inline std::unique_ptr<BindSystem> instance_ = nullptr;

    bool isEnabled_ = false;                            //!< モード有効フラグ
    std::optional<BondableEntity> markedEntity_;        //!< マーク済みエンティティ
    float bindCost_ = 20.0f;                            //!< 縁を結ぶFEコスト
    BondType pendingBondType_ = BondType::Basic;        //!< 次に作成する縁のタイプ
    int maxBindCount_ = -1;                             //!< 結ぶ回数上限（-1=無制限）
    int currentBindCount_ = 0;                          //!< 現在の使用回数

    // コールバック
    std::function<void(bool)> onModeChanged_;
    std::function<void(const BondableEntity&)> onEntityMarked_;
    std::function<void(const BondableEntity&, const BondableEntity&)> onBondCreated_;
};
