//----------------------------------------------------------------------------
//! @file   bond_manager.h
//! @brief  縁マネージャー - 全ての縁を管理
//----------------------------------------------------------------------------
#pragma once

#include "bond.h"
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <string>
#include <cassert>

//----------------------------------------------------------------------------
//! @brief 縁マネージャー（シングルトン）
//! @details 全ての縁の作成・削除・検索を管理する
//----------------------------------------------------------------------------
class BondManager
{
public:
    //! @brief シングルトンインスタンス取得
    static BondManager& Get();

    //! @brief インスタンス生成
    static void Create();

    //! @brief インスタンス破棄
    static void Destroy();

    //! @brief デストラクタ
    ~BondManager() = default;

    //------------------------------------------------------------------------
    // 縁の作成・削除
    //------------------------------------------------------------------------

    //! @brief 縁を作成
    //! @param a 参加者A
    //! @param b 参加者B
    //! @param type 縁の種類
    //! @return 作成した縁へのポインタ。既に接続済みならnullptr
    Bond* CreateBond(BondableEntity a, BondableEntity b, BondType type = BondType::Basic);

    //! @brief 縁を削除
    //! @param bond 削除する縁
    //! @return 削除成功ならtrue
    bool RemoveBond(Bond* bond);

    //! @brief 2つのエンティティ間の縁を削除
    //! @return 削除成功ならtrue
    bool RemoveBond(const BondableEntity& a, const BondableEntity& b);

    //! @brief エンティティに関連する全ての縁を削除
    //! @param entity 対象エンティティ
    void RemoveAllBondsFor(const BondableEntity& entity);

    //! @brief 全ての縁を削除
    void Clear();

    //------------------------------------------------------------------------
    // 縁の検索
    //------------------------------------------------------------------------

    //! @brief 2つのエンティティが直接縁で繋がっているか判定
    [[nodiscard]] bool AreDirectlyConnected(const BondableEntity& a, const BondableEntity& b) const;

    //! @brief 2つのエンティティ間の縁を取得
    //! @return 縁へのポインタ。存在しなければnullptr
    [[nodiscard]] Bond* GetBond(const BondableEntity& a, const BondableEntity& b) const;

    //! @brief エンティティに関連する全ての縁を取得
    [[nodiscard]] std::vector<Bond*> GetBondsFor(const BondableEntity& entity) const;

    //! @brief 全ての縁を取得（参照版 - イテレーション中の変更禁止）
    //! @warning イテレーション中にCreateBond()/RemoveBond()を呼ぶとクラッシュする可能性あり
    //!          イテレーション中に変更の可能性がある場合はGetAllBondsCopy()を使用すること
    [[nodiscard]] const std::vector<std::unique_ptr<Bond>>& GetAllBonds() const { return bonds_; }

    //! @brief 全ての縁を取得（コピー版 - イテレーション中の変更に安全）
    //! @return 全ての縁へのポインタのコピー
    [[nodiscard]] std::vector<Bond*> GetAllBondsCopy() const;

    //! @brief 縁の数を取得
    [[nodiscard]] size_t GetBondCount() const { return bonds_.size(); }

    //! @brief 指定タイプの縁を取得
    //! @param type 取得する縁のタイプ
    //! @return 指定タイプの縁リスト
    [[nodiscard]] std::vector<Bond*> GetBondsByType(BondType type) const;

    //------------------------------------------------------------------------
    // ネットワーク探索（勝利条件判定用）
    //------------------------------------------------------------------------

    //! @brief 指定エンティティと推移的に接続された全エンティティを取得
    //! @param start 開始エンティティ
    //! @return 接続されたエンティティのリスト（start自身を含む）
    [[nodiscard]] std::vector<BondableEntity> GetConnectedNetwork(const BondableEntity& start) const;

    //! @brief 2つのエンティティが推移的に接続されているか判定
    [[nodiscard]] bool AreTransitivelyConnected(const BondableEntity& a, const BondableEntity& b) const;

    //------------------------------------------------------------------------
    // コールバック
    //------------------------------------------------------------------------

    //! @brief 縁作成時コールバック設定
    void SetOnBondCreated(std::function<void(Bond*)> callback) { onBondCreated_ = callback; }

    //! @brief 縁削除時コールバック設定
    void SetOnBondRemoved(std::function<void(const BondableEntity&, const BondableEntity&)> callback)
    {
        onBondRemoved_ = callback;
    }

private:
    BondManager() = default;
    BondManager(const BondManager&) = delete;
    BondManager& operator=(const BondManager&) = delete;

    //! @brief キャッシュを再構築
    void RebuildCache();

    static inline std::unique_ptr<BondManager> instance_ = nullptr;

    std::vector<std::unique_ptr<Bond>> bonds_;  //!< 全ての縁

    // キャッシュ（O(1)ルックアップ用）
    std::unordered_map<std::string, std::vector<Bond*>> entityBondsCache_;  //!< エンティティID→縁リスト
    std::unordered_map<int, std::vector<Bond*>> typeBondsCache_;            //!< BondType→縁リスト

    // コールバック
    std::function<void(Bond*)> onBondCreated_;
    std::function<void(const BondableEntity&, const BondableEntity&)> onBondRemoved_;
};
