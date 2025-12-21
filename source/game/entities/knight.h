//----------------------------------------------------------------------------
//! @file   knight.h
//! @brief  Knight種族クラス - 近接攻撃
//----------------------------------------------------------------------------
#pragma once

#include "individual.h"

//----------------------------------------------------------------------------
//! @brief Knight種族クラス
//! @details 近接攻撃を行う種族。剣を振って攻撃する。
//----------------------------------------------------------------------------
class Knight : public Individual
{
public:
    //! @brief コンストラクタ
    //! @param id 一意の識別子
    explicit Knight(const std::string& id);

    //! @brief デストラクタ
    ~Knight() override = default;

    //! @brief 更新（剣振りアニメーション処理）
    //! @param dt デルタタイム
    void Update(float dt) override;

    //! @brief 描画（剣エフェクト含む）
    //! @param spriteBatch SpriteBatch参照
    void Render(SpriteBatch& spriteBatch) override;

    //! @brief 攻撃処理（剣振り開始）
    //! @param target 攻撃対象
    void Attack(Individual* target) override;

    //! @brief プレイヤーへの攻撃処理（剣振り開始）
    //! @param target 攻撃対象プレイヤー
    void AttackPlayer(Player* target) override;

    //! @brief 攻撃範囲を取得
    //! @return 攻撃範囲（近接なので短い）
    [[nodiscard]] float GetAttackRange() const override { return kAttackRange; }

    //! @brief 色を設定
    //! @param color スプライトの色
    void SetColor(const Color& color);

    //! @brief 剣を振っているか
    [[nodiscard]] bool IsSwinging() const { return isSwinging_; }

protected:
    //! @brief テクスチャセットアップ
    void SetupTexture() override;

    //! @brief コライダーセットアップ
    void SetupCollider() override;

private:
    //! @brief 剣振りを開始（共通処理）
    //! @param targetPos ターゲットの位置
    void StartSwordSwing(const Vector2& targetPos);

    //! @brief 剣振りの当たり判定をチェック
    void CheckSwordHit();

    //! @brief 剣の先端位置を計算
    [[nodiscard]] Vector2 CalculateSwordTip() const;

    // 定数
    static constexpr float kDefaultHp = 150.0f;     //!< デフォルトHP（Elfより高い）
    static constexpr float kDefaultDamage = 20.0f;  //!< デフォルトダメージ（Elfより高い）
    static constexpr float kDefaultSpeed = 80.0f;   //!< デフォルト移動速度（Elfより遅い）

    // テクスチャサイズ
    static constexpr int kTextureSize = 32;

    // 剣振り設定
    static constexpr float kSwordLength = 80.0f;    //!< 剣の長さ
    static constexpr float kAttackRange = 110.0f;   //!< 攻撃範囲（剣が届く距離）
    static constexpr float kSwingDuration = 0.3f;   //!< 剣振り時間
    static constexpr float kSwingStartAngle = -60.0f;  //!< 振り始め角度（度）
    static constexpr float kSwingEndAngle = 60.0f;     //!< 振り終わり角度（度）

    // スプライト色
    Color color_ = Color(0.3f, 0.5f, 1.0f, 1.0f);  //!< デフォルトは青系

    // 剣振り状態
    bool isSwinging_ = false;           //!< 剣を振っているか
    float swingTimer_ = 0.0f;           //!< 剣振りタイマー
    float swingAngle_ = 0.0f;           //!< 現在の剣の角度（度）
    Vector2 swingDirection_;            //!< 剣を振る方向（ターゲット方向）
    bool hasHitTarget_ = false;         //!< この振りでターゲットにヒットしたか
    Player* playerTarget_ = nullptr;    //!< プレイヤーターゲット（AttackPlayer用）
};
