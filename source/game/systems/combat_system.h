//----------------------------------------------------------------------------
//! @file   combat_system.h
//! @brief  戦闘システム - グループ間の戦闘を管理
//----------------------------------------------------------------------------
#pragma once

#include <vector>
#include <functional>
#include <set>

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
    CombatSystem() = default;
    ~CombatSystem() = default;
    CombatSystem(const CombatSystem&) = delete;
    CombatSystem& operator=(const CombatSystem&) = delete;

    //! @brief グループ間の戦闘処理
    void ProcessCombat(Group* attacker, Group* defender, float dt);

    //! @brief グループ→プレイヤーの戦闘処理
    void ProcessCombatAgainstPlayer(Group* attacker, float dt);

    std::vector<Group*> groups_;    //!< 登録されたグループ
    std::set<Group*> defeatedGroups_;  //!< 既に全滅処理済みのグループ
    Player* player_ = nullptr;      //!< プレイヤー参照

    float attackInterval_ = 0.3f;   //!< 攻撃間隔（0.3秒）
    float attackTimer_ = 0.0f;      //!< 攻撃タイマー

    // コールバック
    std::function<void(Individual*, Individual*, float)> onAttack_;
    std::function<void(Group*)> onGroupDefeated_;
};
