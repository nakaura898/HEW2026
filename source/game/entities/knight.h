//----------------------------------------------------------------------------
//! @file   knight.h
//! @brief  Knight種族クラス - 近接攻撃
//----------------------------------------------------------------------------
#pragma once

#include "individual.h"

// 前方宣言
class MeleeAttackBehavior;

//----------------------------------------------------------------------------
//! @brief Knight種族クラス
//! @details 近接攻撃を行う種族。剣を振って攻撃する。
//!          攻撃ロジックはMeleeAttackBehaviorで管理。
//----------------------------------------------------------------------------
class Knight : public Individual
{
public:
    //! @brief コンストラクタ
    //! @param id 一意の識別子
    explicit Knight(const std::string& id);

    //! @brief デストラクタ
    ~Knight() override = default;

    //! @brief 描画（剣エフェクト含む）
    //! @param spriteBatch SpriteBatch参照
    void Render(SpriteBatch& spriteBatch) override;

    //! @brief 攻撃処理（StateMachineに委譲）
    //! @param target 攻撃対象
    void Attack(Individual* target) override;

    //! @brief プレイヤーへの攻撃処理（StateMachineに委譲）
    //! @param target 攻撃対象プレイヤー
    void AttackPlayer(Player* target) override;

    //! @brief 攻撃範囲を取得
    //! @return 攻撃範囲（近接なので短い）
    [[nodiscard]] float GetAttackRange() const override { return kAttackRange; }

    //! @brief 色を設定
    //! @param color スプライトの色
    void SetColor(const Color& color);

    //! @brief 剣を振っているか
    [[nodiscard]] bool IsSwinging() const;

protected:
    //! @brief テクスチャセットアップ
    void SetupTexture() override;

    //! @brief コライダーセットアップ
    void SetupCollider() override;

    //! @brief StateMachineセットアップ（MeleeAttackBehaviorを設定）
    void SetupStateMachine() override;

private:
    // 定数
    static constexpr float kDefaultHp = 150.0f;     //!< デフォルトHP（Elfより高い）
    static constexpr float kDefaultDamage = 20.0f;  //!< デフォルトダメージ（Elfより高い）
    static constexpr float kDefaultSpeed = 80.0f;   //!< デフォルト移動速度（Elfより遅い）

    // テクスチャサイズ
    static constexpr int kTextureSize = 32;

    // 攻撃範囲
    static constexpr float kAttackRange = 110.0f;   //!< 攻撃範囲（剣が届く距離）

    // スプライト色
    Color color_ = Color(0.3f, 0.5f, 1.0f, 1.0f);  //!< デフォルトは青系

    //! @brief キャッシュされたMeleeAttackBehaviorポインタ（dynamic_cast回避用）
    MeleeAttackBehavior* cachedMeleeAttackBehavior_ = nullptr;
};
