//----------------------------------------------------------------------------
//! @file   player.h
//! @brief  プレイヤークラス
//----------------------------------------------------------------------------
#pragma once

#include "engine/component/game_object.h"
#include "engine/component/transform2d.h"
#include "engine/component/sprite_renderer.h"
#include "engine/component/animator.h"
#include "engine/component/collider2d.h"
#include "engine/component/camera2d.h"
#include "dx11/gpu/texture.h"
#include <memory>
#include <vector>
#include <functional>

//----------------------------------------------------------------------------
//! @brief 矢の構造体
//----------------------------------------------------------------------------
struct Arrow
{
    Vector2 position;   //!< 位置
    Vector2 velocity;   //!< 速度ベクトル
    float rotation;     //!< 回転角度（ラジアン）
    float lifetime;     //!< 残り生存時間
};

//----------------------------------------------------------------------------
//! @brief プレイヤークラス
//----------------------------------------------------------------------------
class Player
{
public:
    //! @brief コンストラクタ
    Player() = default;

    //! @brief デストラクタ
    ~Player() = default;

    //! @brief 初期化
    //! @param position 初期位置
    void Initialize(const Vector2& position);

    //! @brief 終了処理
    void Shutdown();

    //! @brief 更新
    //! @param dt デルタタイム
    //! @param camera カメラ（座標変換用）
    void Update(float dt, Camera2D& camera);

    //! @brief 描画
    //! @param spriteBatch SpriteBatch参照
    void Render(class SpriteBatch& spriteBatch);

    //! @brief コライダーのデバッグ描画
    //! @param spriteBatch SpriteBatch参照
    //! @param debugTexture デバッグ用テクスチャ
    void RenderColliderDebug(class SpriteBatch& spriteBatch, Texture* debugTexture);

    //! @brief Transform取得
    [[nodiscard]] Transform2D* GetTransform() const { return transform_; }

    //! @brief GameObject取得
    [[nodiscard]] GameObject* GetGameObject() const { return gameObject_.get(); }

    //! @brief 衝突回数取得（デバッグ用）
    [[nodiscard]] int GetCollisionCount() const { return collisionCount_; }

    //! @brief 矢のリスト取得（描画用）
    [[nodiscard]] const std::vector<Arrow>& GetArrows() const { return arrows_; }

    //! @brief 矢用テクスチャ取得
    [[nodiscard]] Texture* GetArrowTexture() const { return arrowTexture_.get(); }

private:
    //! @brief 入力処理
    void HandleInput(float dt, Camera2D& camera);

    //! @brief 矢を発射
    void FireArrow(const Vector2& targetWorld);

    //! @brief 矢の更新
    void UpdateArrows(float dt);

    // GameObject
    std::unique_ptr<GameObject> gameObject_;

    // コンポーネントキャッシュ
    Transform2D* transform_ = nullptr;
    SpriteRenderer* sprite_ = nullptr;
    Animator* animator_ = nullptr;
    Collider2D* collider_ = nullptr;

    // テクスチャ
    TexturePtr playerTexture_;
    TexturePtr arrowTexture_;

    // 状態
    bool isAttacking_ = false;
    int collisionCount_ = 0;

    // 矢
    std::vector<Arrow> arrows_;

    // 定数
    static constexpr float kMoveSpeed = 300.0f;
    static constexpr float kArrowSpeed = 800.0f;
    static constexpr float kArrowLifetime = 3.0f;
    static constexpr int kAnimRows = 4;
    static constexpr int kAnimCols = 4;
};
