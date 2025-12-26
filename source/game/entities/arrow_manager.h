//----------------------------------------------------------------------------
//! @file   arrow_manager.h
//! @brief  矢マネージャー - 全ての矢を管理
//----------------------------------------------------------------------------
#pragma once

#include "arrow.h"
#include <vector>
#include <memory>

// 前方宣言
class SpriteBatch;
class Player;

//----------------------------------------------------------------------------
//! @brief 矢マネージャー（シングルトン）
//! @details 全ての飛翔中の矢を管理する
//----------------------------------------------------------------------------
class ArrowManager
{
public:
    //! @brief シングルトンインスタンス取得
    static ArrowManager& Get();

    //! @brief 矢を発射（Individual対象）
    //! @param owner 発射者
    //! @param target ターゲット
    //! @param startPos 発射位置
    //! @param damage ダメージ量
    void Shoot(Individual* owner, Individual* target, const Vector2& startPos, float damage);

    //! @brief 矢を発射（Player対象）
    //! @param owner 発射者
    //! @param targetPlayer ターゲットプレイヤー
    //! @param startPos 発射位置
    //! @param damage ダメージ量
    void ShootAtPlayer(Individual* owner, Player* targetPlayer, const Vector2& startPos, float damage);

    //! @brief 全矢を更新
    //! @param dt デルタタイム
    void Update(float dt);

    //! @brief 全矢を描画
    //! @param spriteBatch SpriteBatch参照
    void Render(SpriteBatch& spriteBatch);

    //! @brief 全矢をクリア
    void Clear();

    //! @brief 矢の数を取得
    [[nodiscard]] size_t GetArrowCount() const { return arrows_.size(); }

private:
    ArrowManager() = default;
    ~ArrowManager() = default;
    ArrowManager(const ArrowManager&) = delete;
    ArrowManager& operator=(const ArrowManager&) = delete;

    //! @brief 無効な矢を削除
    void RemoveInactiveArrows();

    std::vector<std::unique_ptr<Arrow>> arrows_;
};
