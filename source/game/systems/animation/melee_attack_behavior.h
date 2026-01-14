//----------------------------------------------------------------------------
//! @file   melee_attack_behavior.h
//! @brief  近接攻撃行動（Knight用）
//----------------------------------------------------------------------------
#pragma once

#include "attack_behavior.h"

// 前方宣言
class Knight;
class Collider2D;

//----------------------------------------------------------------------------
//! @brief 近接攻撃行動（Knight用）
//! @details 剣振りロジックを管理
//----------------------------------------------------------------------------
class MeleeAttackBehavior : public IAttackBehavior
{
public:
    //! @brief コンストラクタ
    //! @param owner 所有者（Knight）
    explicit MeleeAttackBehavior(Knight* owner);

    //! @brief デストラクタ
    ~MeleeAttackBehavior() override = default;

    //------------------------------------------------------------------------
    // IAttackBehavior実装
    //------------------------------------------------------------------------

    void OnAttackStart(Individual* attacker, Individual* target) override;
    void OnAttackStartPlayer(Individual* attacker, Player* target) override;
    void OnAttackUpdate(float dt, AnimState phase, float phaseTime) override;
    bool OnDamageFrame() override;
    void OnAttackEnd() override;
    void OnAttackInterrupt() override;

    [[nodiscard]] float GetWindupDuration() const override;
    [[nodiscard]] float GetActiveDuration() const override;
    [[nodiscard]] float GetRecoveryDuration() const override;
    [[nodiscard]] float GetDamageFrameTime() const override;

    [[nodiscard]] Individual* GetTarget() const override { return attackTarget_; }
    [[nodiscard]] Player* GetPlayerTarget() const override { return playerTarget_; }
    [[nodiscard]] bool GetTargetPosition(Vector2& outPosition) const override;

    //------------------------------------------------------------------------
    // 剣振り状態取得
    //------------------------------------------------------------------------

    //! @brief 剣を振っているか
    [[nodiscard]] bool IsSwinging() const { return isSwinging_; }

    //! @brief 現在の剣の角度を取得（度）
    [[nodiscard]] float GetSwingAngle() const { return swingAngle_; }

    //! @brief 剣を振る方向を取得
    [[nodiscard]] const Vector2& GetSwingDirection() const { return swingDirection_; }

    //! @brief 剣の先端位置を計算
    [[nodiscard]] Vector2 CalculateSwordTip() const;

private:
    //! @brief 剣振りを開始（共通処理）
    //! @param targetPos ターゲットの位置
    void StartSwordSwing(const Vector2& targetPos);

    //! @brief 剣振りの当たり判定をチェック
    //! @return ヒットしたらtrue
    bool CheckSwordHit();

    Knight* owner_ = nullptr;
    Individual* attackTarget_ = nullptr;
    Player* playerTarget_ = nullptr;

    // 剣振り状態
    bool isSwinging_ = false;           //!< 剣を振っているか
    float swingAngle_ = 0.0f;           //!< 現在の剣の角度（度）
    Vector2 swingDirection_;            //!< 剣を振る方向（ターゲット方向）
    bool hasHitTarget_ = false;         //!< この振りでターゲットにヒットしたか

    // 剣振り設定
    static constexpr float kSwordLength = 80.0f;       //!< 剣の長さ
    static constexpr float kSwingDuration = 0.3f;      //!< 剣振り時間
    static constexpr float kSwingStartAngle = -60.0f;  //!< 振り始め角度（度）
    static constexpr float kSwingEndAngle = 60.0f;     //!< 振り終わり角度（度）
    static constexpr float kRecoveryDuration = 0.1f;   //!< 後隙時間

    // 数学定数
    static constexpr float kPi = 3.14159265358979323846f;
    static constexpr float kDegToRad = kPi / 180.0f;
    static constexpr float kMinLength = 0.001f;
};
