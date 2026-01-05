//----------------------------------------------------------------------------
//! @file   player.h
//! @brief  Playerクラス - プレイヤーキャラクター（縁操作専用）
//----------------------------------------------------------------------------
#pragma once

#include "engine/component/game_object.h"
#include "engine/component/transform2d.h"
#include "engine/component/sprite_renderer.h"
#include "engine/component/animator.h"
#include "engine/component/collider2d.h"
#include "engine/component/camera2d.h"
#include "engine/texture/texture_types.h"
#include <memory>
#include <string>

// 前方宣言
class SpriteBatch;

//----------------------------------------------------------------------------
//! @brief プレイヤークラス（A-RAS!ゲーム用）
//! @details 攻撃できず、縁の「結」「切」操作のみ行う。
//!          BondableEntity互換（GetId, GetPosition, GetThreat）
//----------------------------------------------------------------------------
class Player
{
public:
    //! @brief コンストラクタ
    Player();

    //! @brief デストラクタ
    ~Player();

    //------------------------------------------------------------------------
    // ライフサイクル
    //------------------------------------------------------------------------

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
    void Render(SpriteBatch& spriteBatch);

    //------------------------------------------------------------------------
    // HP管理
    //------------------------------------------------------------------------

    //! @brief ダメージを受ける
    //! @param damage ダメージ量
    void TakeDamage(float damage);

    //! @brief 生存判定
    [[nodiscard]] bool IsAlive() const { return hp_ > 0.0f; }

    //! @brief 現在HP取得
    [[nodiscard]] float GetHp() const { return hp_; }

    //! @brief 最大HP取得
    [[nodiscard]] float GetMaxHp() const { return maxHp_; }

    //! @brief HP割合取得（0.0〜1.0）
    [[nodiscard]] float GetHpRatio() const { return maxHp_ > 0.0f ? hp_ / maxHp_ : 0.0f; }

    //------------------------------------------------------------------------
    // FE（フェイトエネルギー）管理
    //------------------------------------------------------------------------

    //! @brief 現在FE取得
    [[nodiscard]] float GetFe() const { return fe_; }

    //! @brief 最大FE取得
    [[nodiscard]] float GetMaxFe() const { return maxFe_; }

    //! @brief FE割合取得（0.0〜1.0）
    [[nodiscard]] float GetFeRatio() const { return maxFe_ > 0.0f ? fe_ / maxFe_ : 0.0f; }

    //! @brief FEを消費
    //! @param amount 消費量
    //! @return 消費成功ならtrue
    bool ConsumeFe(float amount);

    //! @brief FEを回復
    //! @param amount 回復量
    void RecoverFe(float amount);

    //! @brief FEが足りるか判定
    [[nodiscard]] bool HasEnoughFe(float amount) const { return fe_ >= amount; }

    //------------------------------------------------------------------------
    // BondableEntity互換インターフェース
    //------------------------------------------------------------------------

    //! @brief ID取得
    [[nodiscard]] const std::string& GetId() const { return id_; }

    //! @brief 位置取得
    [[nodiscard]] Vector2 GetPosition() const;

    //! @brief 脅威度取得
    [[nodiscard]] float GetThreat() const { return baseThreat_; }

    //! @brief 基本脅威度を設定
    void SetBaseThreat(float threat) { baseThreat_ = threat; }

    //------------------------------------------------------------------------
    // アクセサ
    //------------------------------------------------------------------------

    //! @brief Transform2D取得
    [[nodiscard]] Transform2D* GetTransform() const { return transform_; }

    //! @brief Collider2D取得
    [[nodiscard]] Collider2D* GetCollider() const { return collider_; }

    //! @brief GameObject取得
    [[nodiscard]] GameObject* GetGameObject() const { return gameObject_.get(); }

    //! @brief 移動速度設定
    void SetMoveSpeed(float speed) { moveSpeed_ = speed; }

    //! @brief 最大HP設定
    void SetMaxHp(float hp) { maxHp_ = hp; hp_ = hp; }

    //! @brief 最大FE設定
    void SetMaxFe(float fe) { maxFe_ = fe; fe_ = fe; }

private:
    //! @brief 入力処理
    void HandleInput(float dt, Camera2D& camera);

    // 識別
    std::string id_ = "Player";

    // GameObject & コンポーネント
    std::unique_ptr<GameObject> gameObject_;
    Transform2D* transform_ = nullptr;
    SpriteRenderer* sprite_ = nullptr;
    Animator* animator_ = nullptr;
    Collider2D* collider_ = nullptr;

    // テクスチャ
    TexturePtr texture_;

    // ステータス
    float hp_ = 100.0f;
    float maxHp_ = 100.0f;
    float fe_ = 100.0f;
    float maxFe_ = 100.0f;
    float baseThreat_ = 10.0f;  //!< 低めに設定（グループ同士を優先させる）
    float moveSpeed_ = 200.0f;

    // アニメーション状態
    bool isMoving_ = false;

    // 定数
    static constexpr int kAnimRows = 4;
    static constexpr int kAnimCols = 4;
};
