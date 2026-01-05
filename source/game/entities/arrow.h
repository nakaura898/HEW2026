//----------------------------------------------------------------------------
//! @file   arrow.h
//! @brief  矢クラス - Elfが発射する飛翔体
//----------------------------------------------------------------------------
#pragma once

#include "engine/component/game_object.h"
#include "engine/component/transform2d.h"
#include "engine/component/sprite_renderer.h"
#include "engine/component/collider2d.h"
#include "engine/math/math_types.h"
#include "engine/texture/texture_types.h"
#include <memory>
#include <string>

// 前方宣言
class Individual;
class Player;
class SpriteBatch;

//----------------------------------------------------------------------------
//! @brief 矢クラス
//! @details Elfが発射する飛翔体。ターゲットに向かって飛び、命中時にダメージを与える
//----------------------------------------------------------------------------
class Arrow
{
public:
    //! @brief コンストラクタ（Individual対象）
    //! @param owner 発射したIndividual
    //! @param target ターゲットIndividual
    //! @param damage ダメージ量
    Arrow(Individual* owner, Individual* target, float damage);

    //! @brief コンストラクタ（Player対象）
    //! @param owner 発射したIndividual
    //! @param targetPlayer ターゲットPlayer
    //! @param damage ダメージ量
    Arrow(Individual* owner, Player* targetPlayer, float damage);

    //! @brief デストラクタ
    ~Arrow();

    //! @brief 初期化
    //! @param startPos 発射位置
    //! @param targetPos ターゲット位置
    void Initialize(const Vector2& startPos, const Vector2& targetPos);

    //! @brief 更新
    //! @param dt デルタタイム
    void Update(float dt);

    //! @brief 描画
    //! @param spriteBatch SpriteBatch参照
    void Render(SpriteBatch& spriteBatch);

    //! @brief 有効か判定（まだ飛行中か）
    [[nodiscard]] bool IsActive() const { return isActive_; }

    //! @brief 位置取得
    [[nodiscard]] Vector2 GetPosition() const;

private:
    // 所有者・ターゲット
    Individual* owner_ = nullptr;
    Individual* target_ = nullptr;
    Player* targetPlayer_ = nullptr;

    // GameObject
    std::unique_ptr<GameObject> gameObject_;
    Transform2D* transform_ = nullptr;
    SpriteRenderer* sprite_ = nullptr;
    Collider2D* collider_ = nullptr;

    // テクスチャ
    TexturePtr texture_;

    // 移動
    Vector2 direction_;
    float speed_ = 500.0f;
    float damage_ = 10.0f;

    // 状態
    bool isActive_ = true;
    float lifetime_ = 0.0f;
    static constexpr float kMaxLifetime = 3.0f;  // 最大生存時間
};
