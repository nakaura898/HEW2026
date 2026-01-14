//----------------------------------------------------------------------------
//! @file   relationship_context.h
//! @brief  グローバル関係レジストリ
//----------------------------------------------------------------------------
#pragma once

#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <cassert>

// 前方宣言
class Individual;
class Player;

//----------------------------------------------------------------------------
//! @brief グローバル関係レジストリ
//! @details 攻撃関係を双方向クエリ可能にする
//!          - attacker → target (誰を攻撃しているか)
//!          - target → [attackers] (誰から攻撃されているか)
//! @note ライフタイム管理: Initialize()でイベント購読開始、Shutdown()で解除
//!       IndividualDiedEventを購読し、死亡時に自動的に関係を解除する
//----------------------------------------------------------------------------
class RelationshipContext
{
public:
    //! @brief シングルトンインスタンスを取得
    static RelationshipContext& Get();

    //! @brief インスタンス生成
    static void Create();

    //! @brief インスタンス破棄
    static void Destroy();

    //! @brief デストラクタ
    ~RelationshipContext() = default;

    //! @brief 初期化（EventBus購読開始）
    void Initialize();

    //! @brief 終了（EventBus購読解除、状態クリア）
    void Shutdown();

    //------------------------------------------------------------------------
    // 攻撃関係の登録（IAttackBehaviorから呼び出し）
    //------------------------------------------------------------------------

    //! @brief Individual対象の攻撃関係を登録
    //! @param attacker 攻撃者
    //! @param target 攻撃対象
    void RegisterAttack(Individual* attacker, Individual* target);

    //! @brief Player対象の攻撃関係を登録
    //! @param attacker 攻撃者
    //! @param target 攻撃対象プレイヤー
    void RegisterAttackPlayer(Individual* attacker, Player* target);

    //! @brief 攻撃関係を解除
    //! @param attacker 攻撃者
    void UnregisterAttack(Individual* attacker);

    //------------------------------------------------------------------------
    // クエリAPI（Individual::BuildAnimationContextから呼び出し）
    //------------------------------------------------------------------------

    //! @brief 攻撃対象を取得
    //! @param attacker 攻撃者
    //! @return 攻撃対象（なければnullptr）
    [[nodiscard]] Individual* GetAttackTarget(const Individual* attacker) const;

    //! @brief プレイヤー攻撃対象を取得
    //! @param attacker 攻撃者
    //! @return 攻撃対象プレイヤー（なければnullptr）
    [[nodiscard]] Player* GetPlayerTarget(const Individual* attacker) const;

    //! @brief この個体を攻撃している全員を取得
    //! @param target 攻撃対象
    //! @return 攻撃者リスト
    [[nodiscard]] std::vector<Individual*> GetAttackers(const Individual* target) const;

    //! @brief 攻撃されているか
    //! @param target 攻撃対象
    //! @return 攻撃されていればtrue
    [[nodiscard]] bool IsUnderAttack(const Individual* target) const;

    //------------------------------------------------------------------------
    // クリーンアップ
    //------------------------------------------------------------------------

    //! @brief 全関係をクリア
    void Clear();

    //! @brief 特定の個体を全ての関係から除去（外部から明示的に呼ぶ場合用）
    void RemoveIndividual(Individual* individual);

private:
    RelationshipContext() = default;
    RelationshipContext(const RelationshipContext&) = delete;
    RelationshipContext& operator=(const RelationshipContext&) = delete;

    //! @brief 個体死亡イベントハンドラ
    void OnIndividualDied(const struct IndividualDiedEvent& event);

    //! @brief attacker → target (Individual対象)
    std::unordered_map<Individual*, Individual*> attackerToTarget_;

    //! @brief attacker → target (Player対象)
    std::unordered_map<Individual*, Player*> attackerToPlayer_;

    //! @brief target → attackers (逆引き: Individualが攻撃されている)
    std::unordered_map<Individual*, std::unordered_set<Individual*>> targetToAttackers_;

    static inline std::unique_ptr<RelationshipContext> instance_ = nullptr;

    //! @brief player → attackers (逆引き: Playerが攻撃されている)
    std::unordered_map<Player*, std::unordered_set<Individual*>> playerToAttackers_;

    uint32_t diedSubscriptionId_ = 0;  //!< IndividualDiedEvent購読ID
};
