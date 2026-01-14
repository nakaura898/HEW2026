//----------------------------------------------------------------------------
//! @file   elf.h
//! @brief  Elf種族クラス - 遠距離攻撃
//----------------------------------------------------------------------------
#pragma once

#include "individual.h"

//----------------------------------------------------------------------------
//! @brief Elf種族クラス
//! @details 弓で遠距離攻撃を行う種族。elf_sprite.pngを使用。
//!          攻撃ロジックはRangedAttackBehaviorで管理。
//----------------------------------------------------------------------------
class Elf : public Individual
{
public:
    //! @brief コンストラクタ
    //! @param id 一意の識別子
    explicit Elf(const std::string& id);

    //! @brief デストラクタ
    ~Elf() override = default;

    //! @brief 攻撃処理（StateMachineに委譲）
    //! @param target 攻撃対象
    void Attack(Individual* target) override;

    //! @brief プレイヤーへの攻撃処理（StateMachineに委譲）
    //! @param target 攻撃対象プレイヤー
    void AttackPlayer(Player* target) override;

    //! @brief 攻撃範囲を取得
    //! @return 攻撃範囲（弓なので長い）
    [[nodiscard]] float GetAttackRange() const override { return kAttackRange; }

    //! @brief 現在の攻撃対象の位置を取得
    //! @param[out] outPosition 攻撃対象の位置
    //! @return 攻撃対象が存在すればtrue
    [[nodiscard]] bool GetCurrentAttackTargetPosition(Vector2& outPosition) const override;

protected:
    //! @brief テクスチャセットアップ
    void SetupTexture() override;

    //! @brief アニメーションセットアップ
    void SetupAnimator() override;

    //! @brief StateMachineセットアップ（RangedAttackBehaviorを設定）
    void SetupStateMachine() override;

private:
    // 定数
    static constexpr float kAttackRange = 600.0f;   //!< 攻撃範囲（弓なので長い）
    static constexpr float kDefaultHp = 80.0f;      //!< デフォルトHP（遠距離なので低め）
    static constexpr float kDefaultDamage = 12.0f;  //!< デフォルトダメージ
    static constexpr float kDefaultSpeed = 100.0f;  //!< デフォルト移動速度

    // アニメーション設定
    static constexpr int kAnimRows = 4;
    static constexpr int kAnimCols = 4;
};
