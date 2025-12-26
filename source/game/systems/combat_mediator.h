//----------------------------------------------------------------------------
//! @file   combat_mediator.h
//! @brief  戦闘調整システム（Mediatorパターン）
//----------------------------------------------------------------------------
#pragma once

#include <unordered_map>
#include <unordered_set>
#include <shared_mutex>
#include <cstdint>
#include <vector>

class Group;
class Player;
enum class AIState;
struct AIStateChangedEvent;
struct LoveFollowingChangedEvent;

//----------------------------------------------------------------------------
//! @brief 戦闘調整システム（Mediatorパターン）
//! @details EventBusからの通知を受け、CombatSystemに許可判定を提供
//!          GroupAIとCombatSystemの間を仲介し、戦闘状態を一元管理する
//----------------------------------------------------------------------------
class CombatMediator
{
public:
    //! @brief シングルトンインスタンス取得
    static CombatMediator& Get();

    //------------------------------------------------------------------------
    // 初期化・終了
    //------------------------------------------------------------------------

    //! @brief 初期化（EventBus購読開始）
    void Initialize();

    //! @brief 終了（EventBus購読解除、状態クリア）
    void Shutdown();

    //! @brief プレイヤー参照を設定
    void SetPlayer(Player* player);

    //------------------------------------------------------------------------
    // CombatSystem → Mediator（許可判定）
    //------------------------------------------------------------------------

    //! @brief グループが攻撃可能か判定
    //! @param group 判定対象グループ
    //! @return 攻撃可能ならtrue
    [[nodiscard]] bool CanAttack(Group* group) const;

    //------------------------------------------------------------------------
    // デバッグ
    //------------------------------------------------------------------------

    //! @brief 攻撃可能グループ数を取得
    [[nodiscard]] size_t GetAttackableCount() const;

private:
    CombatMediator() = default;
    ~CombatMediator() = default;
    CombatMediator(const CombatMediator&) = delete;
    CombatMediator& operator=(const CombatMediator&) = delete;

    //------------------------------------------------------------------------
    // EventBus購読ハンドラ
    //------------------------------------------------------------------------

    //! @brief AI状態変更イベントハンドラ
    void OnAIStateChanged(const AIStateChangedEvent& event);

    //! @brief Love追従状態変更イベントハンドラ
    void OnLoveFollowingChanged(const LoveFollowingChangedEvent& event);

    //------------------------------------------------------------------------
    // 内部処理
    //------------------------------------------------------------------------

    //! @brief 攻撃許可を再計算
    //! @param group 対象グループ
    void UpdateAttackPermission(Group* group);

    //! @brief Love距離チェック
    //! @param group 対象グループ
    //! @return Love相手が遠すぎる場合true
    [[nodiscard]] bool CheckLoveDistance(Group* group) const;

    //------------------------------------------------------------------------
    // メンバ変数
    //------------------------------------------------------------------------

    mutable std::shared_mutex mutex_;                      //!< 読み書きロック（マルチスレッド対応）
    std::unordered_set<Group*> attackableGroups_;          //!< 攻撃許可されたグループ
    std::unordered_map<Group*, AIState> groupStates_;      //!< グループごとのAI状態
    std::unordered_map<Group*, bool> loveFollowingFlags_;  //!< グループごとのLove追従フラグ
    Player* player_ = nullptr;                             //!< プレイヤー参照

    uint32_t stateSubscriptionId_ = 0;  //!< AIStateChangedEvent購読ID
    uint32_t loveSubscriptionId_ = 0;   //!< LoveFollowingChangedEvent購読ID
};
