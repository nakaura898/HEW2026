//----------------------------------------------------------------------------
//! @file   stagger_system.h
//! @brief  硬直システム - グループの硬直状態を管理
//----------------------------------------------------------------------------
#pragma once

#include <unordered_map>
#include <functional>
#include <memory>
#include <cassert>

// 前方宣言
class Group;

//----------------------------------------------------------------------------
//! @brief 硬直システム（シングルトン）
//! @details 縁を切られたグループは一定時間動けない
//----------------------------------------------------------------------------
class StaggerSystem
{
public:
    //! @brief シングルトンインスタンス取得
    static StaggerSystem& Get();

    //! @brief インスタンス生成
    static void Create();

    //! @brief インスタンス破棄
    static void Destroy();

    //! @brief デストラクタ
    ~StaggerSystem() = default;

    //------------------------------------------------------------------------
    // 更新
    //------------------------------------------------------------------------

    //! @brief システム更新
    //! @param dt デルタタイム（TimeManager経由で取得推奨）
    void Update(float dt);

    //------------------------------------------------------------------------
    // 硬直操作
    //------------------------------------------------------------------------

    //! @brief グループに硬直を付与
    //! @param group 対象グループ
    //! @param duration 硬直時間（秒）
    void ApplyStagger(Group* group, float duration);

    //! @brief グループの硬直を解除
    //! @param group 対象グループ
    void RemoveStagger(Group* group);

    //! @brief グループが硬直中か判定
    //! @param group 対象グループ
    //! @return 硬直中ならtrue
    [[nodiscard]] bool IsStaggered(Group* group) const;

    //! @brief グループの残り硬直時間を取得
    //! @param group 対象グループ
    //! @return 残り時間（硬直していなければ0）
    [[nodiscard]] float GetRemainingTime(Group* group) const;

    //------------------------------------------------------------------------
    // グループ死亡処理
    //------------------------------------------------------------------------

    //! @brief グループが倒された時に硬直情報を削除
    //! @param group 倒されたグループ
    void OnGroupDefeated(Group* group);

    //------------------------------------------------------------------------
    // 設定
    //------------------------------------------------------------------------

    //! @brief デフォルト硬直時間を設定
    void SetDefaultDuration(float duration) { defaultDuration_ = duration; }

    //! @brief デフォルト硬直時間を取得
    [[nodiscard]] float GetDefaultDuration() const { return defaultDuration_; }

    //------------------------------------------------------------------------
    // クリア
    //------------------------------------------------------------------------

    //! @brief 全ての硬直情報をクリア
    void Clear();

    //------------------------------------------------------------------------
    // コールバック
    //------------------------------------------------------------------------

    //! @brief 硬直付与時コールバック
    void SetOnStaggerApplied(std::function<void(Group*, float)> callback)
    {
        onStaggerApplied_ = callback;
    }

    //! @brief 硬直解除時コールバック
    void SetOnStaggerRemoved(std::function<void(Group*)> callback)
    {
        onStaggerRemoved_ = callback;
    }

private:
    StaggerSystem() = default;
    StaggerSystem(const StaggerSystem&) = delete;
    StaggerSystem& operator=(const StaggerSystem&) = delete;

    static inline std::unique_ptr<StaggerSystem> instance_ = nullptr;

    //! @brief 硬直情報（グループ -> 残り時間）
    std::unordered_map<Group*, float> staggerTimers_;

    //! @brief デフォルト硬直時間
    float defaultDuration_ = 3.0f;

    // コールバック
    std::function<void(Group*, float)> onStaggerApplied_;
    std::function<void(Group*)> onStaggerRemoved_;
};
