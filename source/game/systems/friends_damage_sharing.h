//----------------------------------------------------------------------------
//! @file   friends_damage_sharing.h
//! @brief  フレンズ効果システム - フレンズ縁で繋がった全員でダメージを分配
//----------------------------------------------------------------------------
#pragma once

#include <vector>
#include <memory>
#include <cassert>

// 前方宣言
class Group;
class Individual;

//----------------------------------------------------------------------------
//! @brief フレンズ効果システム（シングルトン）
//! @details フレンズ縁で繋がったグループ間でダメージを均等分配する
//!          分配は2段階: 1. Group間で均等分配 2. 各Group内でIndividual均等分配
//----------------------------------------------------------------------------
class FriendsDamageSharing
{
public:
    //! @brief シングルトンインスタンス取得
    static FriendsDamageSharing& Get();

    //! @brief インスタンス生成
    static void Create();

    //! @brief インスタンス破棄
    static void Destroy();

    //! @brief デストラクタ
    ~FriendsDamageSharing() = default;

    //------------------------------------------------------------------------
    // フレンズグループ取得
    //------------------------------------------------------------------------

    //! @brief 指定グループのフレンズクラスタを取得
    //! @param group 対象グループ
    //! @return フレンズ縁で繋がった全グループ（自身を含む）
    [[nodiscard]] std::vector<Group*> GetFriendsCluster(Group* group) const;

    //! @brief グループがフレンズ縁を持っているか判定
    //! @param group 対象グループ
    //! @return フレンズ縁を持っていればtrue
    [[nodiscard]] bool HasFriendsPartners(Group* group) const;

    //------------------------------------------------------------------------
    // ダメージ分配
    //------------------------------------------------------------------------

    //! @brief ダメージを分配して適用
    //! @param targetIndividual 攻撃対象の個体
    //! @param damage 元のダメージ量
    //! @details ターゲットの所属グループからフレンズクラスタを取得し、
    //!          グループ間→個体間の2段階で均等分配
    void ApplyDamageWithSharing(Individual* targetIndividual, float damage);

    //------------------------------------------------------------------------
    // 分配ダメージ適用（内部用）
    //------------------------------------------------------------------------

    //! @brief 分配済みダメージを直接適用（無限ループ防止用）
    //! @param individual 対象個体
    //! @param damage 分配済みダメージ量
    void ApplySharedDamage(Individual* individual, float damage);

private:
    FriendsDamageSharing() = default;
    FriendsDamageSharing(const FriendsDamageSharing&) = delete;
    FriendsDamageSharing& operator=(const FriendsDamageSharing&) = delete;

    //! @brief BFSでフレンズ縁のみをたどってクラスタを構築
    std::vector<Group*> BuildFriendsClusterBFS(Group* start) const;

    static inline std::unique_ptr<FriendsDamageSharing> instance_ = nullptr;
};
