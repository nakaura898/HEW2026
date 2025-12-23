//----------------------------------------------------------------------------
//! @file   bind_system.h
//! @brief  結システム - プレイヤーが縁を結ぶ操作を管理
//----------------------------------------------------------------------------
#pragma once

#include "game/bond/bond.h"
#include "game/bond/bondable_entity.h"
#include <functional>
#include <optional>

//----------------------------------------------------------------------------
//! @brief 結システム（シングルトン）
//! @details 時間停止中に2つのエンティティ間に縁を結ぶ操作を管理
//----------------------------------------------------------------------------
class BindSystem
{
public:
    //! @brief シングルトンインスタンス取得
    static BindSystem& Get();

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
    ~BindSystem() = default;
    BindSystem(const BindSystem&) = delete;
    BindSystem& operator=(const BindSystem&) = delete;

    bool isEnabled_ = false;                            //!< モード有効フラグ
    std::optional<BondableEntity> markedEntity_;        //!< マーク済みエンティティ
    float bindCost_ = 20.0f;                            //!< 縁を結ぶFEコスト
    BondType pendingBondType_ = BondType::Basic;        //!< 次に作成する縁のタイプ

    // コールバック
    std::function<void(bool)> onModeChanged_;
    std::function<void(const BondableEntity&)> onEntityMarked_;
    std::function<void(const BondableEntity&, const BondableEntity&)> onBondCreated_;
};
