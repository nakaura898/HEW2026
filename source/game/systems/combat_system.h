//----------------------------------------------------------------------------
//! @file   combat_system.h
//! @brief  戦闘システム - グループ間の戦闘を管理
//----------------------------------------------------------------------------
#pragma once

#include <vector>
#include <functional>
#include <set>
#include <memory>
#include <cassert>

// 前方宣言
class Group;
class Individual;
class Player;

//----------------------------------------------------------------------------
//! @brief 戦闘システム（シングルトン）
//! @details グループ間の攻撃ターゲット選定と戦闘処理を管理
//----------------------------------------------------------------------------
class CombatSystem
{
public:
    //! @brief シングルトンインスタンス取得
    static CombatSystem& Get();

    //! @brief インスタンス生成
    static void Create();

    //! @brief インスタンス破棄
    static void Destroy();

    //! @brief デストラクタ
    ~CombatSystem();

    //------------------------------------------------------------------------
    // 初期化・更新
    //------------------------------------------------------------------------

    //! @brief システム更新
    //! @param dt デルタタイム
    void Update(float dt);

    //------------------------------------------------------------------------
    // グループ管理
    //------------------------------------------------------------------------

    //! @brief グループを登録
    void RegisterGroup(Group* group);

    //! @brief グループを登録解除
    void UnregisterGroup(Group* group);

    //! @brief 全グループをクリア
    void ClearGroups();

    //! @brief 全グループを取得
    [[nodiscard]] const std::vector<Group*>& GetAllGroups() const { return groups_; }

    //! @brief プレイヤーを設定
    void SetPlayer(Player* player) { player_ = player; }

    //------------------------------------------------------------------------
    // ターゲット選定
    //------------------------------------------------------------------------

    //! @brief 攻撃ターゲットを選定（脅威度ベース）
    //! @param attacker 攻撃者グループ
    //! @return ターゲットグループ。攻撃対象がいなければnullptr
    [[nodiscard]] Group* SelectTarget(Group* attacker) const;

    //! @brief プレイヤーを攻撃可能か判定
    //! @param attacker 攻撃者グループ
    [[nodiscard]] bool CanAttackPlayer(Group* attacker) const;

    //------------------------------------------------------------------------
    // 攻撃判定
    //------------------------------------------------------------------------

    //! @brief 2つのグループが敵対しているか判定
    //! @param a グループA
    //! @param b グループB
    //! @return 敵対している（縁で繋がっていない）ならtrue
    [[nodiscard]] bool AreHostile(Group* a, Group* b) const;

    //! @brief グループがプレイヤーに敵対しているか判定
    [[nodiscard]] bool IsHostileToPlayer(Group* group) const;

    //------------------------------------------------------------------------
    // コールバック
    //------------------------------------------------------------------------

    //! @brief 攻撃発生時コールバック
    void SetOnAttack(std::function<void(Individual*, Individual*, float)> callback)
    {
        onAttack_ = callback;
    }

    //! @brief グループ全滅時コールバック
    void SetOnGroupDefeated(std::function<void(Group*)> callback)
    {
        onGroupDefeated_ = callback;
    }

private:
    CombatSystem();
    CombatSystem(const CombatSystem&) = delete;
    CombatSystem& operator=(const CombatSystem&) = delete;

    //! @brief グループ間の戦闘処理
    void ProcessCombat(Group* attacker, Group* defender, float dt);

    //! @brief グループ→プレイヤーの戦闘処理
    void ProcessCombatAgainstPlayer(Group* attacker, float dt);

    //! @brief 遅延削除を実行
    void FlushPendingRemovals();

    //! @brief グループが削除予約されているか判定
    [[nodiscard]] bool IsPendingRemoval(Group* group) const;

    //! @brief 個体死亡イベントハンドラ（attackTarget_クリア用）
    void OnIndividualDied(Individual* diedIndividual);

    static inline std::unique_ptr<CombatSystem> instance_ = nullptr;

    std::vector<Group*> groups_;            //!< 登録されたグループ
    std::vector<Group*> pendingRemovals_;   //!< 削除予約されたグループ
    std::set<Group*> defeatedGroups_;       //!< 既に全滅処理済みのグループ
    Player* player_ = nullptr;              //!< プレイヤー参照
    bool isUpdating_ = false;               //!< Update中フラグ（TOCTOU防止）

    float attackInterval_ = 1.0f;   //!< 攻撃間隔（1.0秒）
    float attackTimer_ = 1.0f;      //!< 攻撃タイマー（初期値=間隔で即攻撃可能）

    //! @brief IndividualDiedEventの購読ID
    uint32_t individualDiedSubscriptionId_ = 0;

    // コールバック
    std::function<void(Individual*, Individual*, float)> onAttack_;
    std::function<void(Group*)> onGroupDefeated_;
};
