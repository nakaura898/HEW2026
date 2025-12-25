//----------------------------------------------------------------------------
//! @file   elf.h
//! @brief  Elf種族クラス - 遠距離攻撃
//----------------------------------------------------------------------------
#pragma once

#include "individual.h"

//----------------------------------------------------------------------------
//! @brief Elf種族クラス
//! @details 弓で遠距離攻撃を行う種族。elf_sprite.pngを使用。
//----------------------------------------------------------------------------
class Elf : public Individual
{
public:
    //! @brief コンストラクタ
    //! @param id 一意の識別子
    explicit Elf(const std::string& id);

    //! @brief デストラクタ
    ~Elf() override = default;

    //! @brief 更新処理
    void Update(float dt) override;

    //! @brief 攻撃処理（矢を発射予約）
    //! @param target 攻撃対象
    void Attack(Individual* target) override;

    //! @brief プレイヤーへの攻撃処理（矢を発射予約）
    //! @param target 攻撃対象プレイヤー
    void AttackPlayer(Player* target) override;

    //! @brief 攻撃範囲を取得
    //! @return 攻撃範囲（弓なので長い）
    [[nodiscard]] float GetAttackRange() const override { return kAttackRange; }

    //! @brief 現在の攻撃対象の位置を取得（pendingTarget_を使用）
    //! @param[out] outPosition 攻撃対象の位置
    //! @return 攻撃対象が存在すればtrue
    [[nodiscard]] bool GetCurrentAttackTargetPosition(Vector2& outPosition) const override;

protected:
    //! @brief テクスチャセットアップ
    void SetupTexture() override;

    //! @brief アニメーションセットアップ
    void SetupAnimator() override;

private:
    //! @brief 矢を発射
    void ShootArrow();

    // 定数
    static constexpr float kAttackRange = 600.0f;   //!< 攻撃範囲（弓なので長い）
    static constexpr float kDefaultHp = 80.0f;      //!< デフォルトHP（遠距離なので低め）
    static constexpr float kDefaultDamage = 12.0f;  //!< デフォルトダメージ
    static constexpr float kDefaultSpeed = 100.0f;  //!< デフォルト移動速度
    static constexpr uint8_t kShootFrame = 1;       //!< 矢を発射するフレーム

    // アニメーション設定
    static constexpr int kAnimRows = 4;
    static constexpr int kAnimCols = 4;

    // 攻撃予約
    Individual* pendingTarget_ = nullptr;      //!< 攻撃予約ターゲット（Individual）
    Player* pendingTargetPlayer_ = nullptr;    //!< 攻撃予約ターゲット（Player）
    bool arrowShot_ = false;                   //!< 今回の攻撃で矢を発射済みか
};
