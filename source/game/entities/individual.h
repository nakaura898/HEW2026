//----------------------------------------------------------------------------
//! @file   individual.h
//! @brief  Individual基底クラス - 戦闘する個体
//----------------------------------------------------------------------------
#pragma once

#include "engine/component/game_object.h"
#include "engine/component/transform2d.h"
#include "engine/component/sprite_renderer.h"
#include "engine/component/animator.h"
#include "engine/component/collider2d.h"
#include "dx11/gpu/texture.h"
#include "game/systems/animation/animation_controller.h"
#include <memory>
#include <string>
#include <vector>
#include <cstdint>

// 前方宣言
class Group;
class Player;
class SpriteBatch;

//----------------------------------------------------------------------------
//! @brief 個体の行動状態
//----------------------------------------------------------------------------
enum class IndividualAction : uint8_t
{
    Idle,       //!< 待機
    Walk,       //!< 移動
    Attack,     //!< 攻撃
    Death       //!< 死亡
};

//----------------------------------------------------------------------------
//! @brief Individual基底クラス - 戦闘する個体
//! @details 種族（Elf, Knight等）が継承して具体的な攻撃処理を実装する
//----------------------------------------------------------------------------
class Individual
{
public:
    //! @brief コンストラクタ
    //! @param id 一意の識別子
    Individual(const std::string& id);

    //! @brief デストラクタ
    virtual ~Individual();

    //------------------------------------------------------------------------
    // ライフサイクル
    //------------------------------------------------------------------------

    //! @brief 初期化
    //! @param position 初期位置
    virtual void Initialize(const Vector2& position);

    //! @brief 終了処理
    virtual void Shutdown();

    //! @brief 更新
    //! @param dt デルタタイム
    virtual void Update(float dt);

    //! @brief 描画
    //! @param spriteBatch SpriteBatch参照
    virtual void Render(SpriteBatch& spriteBatch);

    //------------------------------------------------------------------------
    // 戦闘
    //------------------------------------------------------------------------

    //! @brief 攻撃処理（Individual対象、種族ごとに実装）
    //! @param target 攻撃対象
    virtual void Attack(Individual* target) = 0;

    //! @brief 攻撃処理（Player対象、種族ごとに実装）
    //! @param target 攻撃対象プレイヤー
    virtual void AttackPlayer(Player* target);

    //! @brief 攻撃範囲を取得（種族ごとに異なる）
    //! @return 攻撃範囲
    [[nodiscard]] virtual float GetAttackRange() const = 0;

    //! @brief ダメージを受ける
    //! @param damage ダメージ量
    void TakeDamage(float damage);

    //! @brief 生存判定
    //! @return 生存中ならtrue
    [[nodiscard]] bool IsAlive() const { return hp_ > 0.0f; }

    //------------------------------------------------------------------------
    // アクセサ
    //------------------------------------------------------------------------

    //! @brief ID取得
    [[nodiscard]] const std::string& GetId() const { return id_; }

    //! @brief 位置取得
    [[nodiscard]] Vector2 GetPosition() const;

    //! @brief 位置設定
    void SetPosition(const Vector2& position);

    //! @brief 現在HP取得
    [[nodiscard]] float GetHp() const { return hp_; }

    //! @brief 最大HP取得
    [[nodiscard]] float GetMaxHp() const { return maxHp_; }

    //! @brief HP割合取得（0.0〜1.0）
    [[nodiscard]] float GetHpRatio() const { return maxHp_ > 0.0f ? hp_ / maxHp_ : 0.0f; }

    //! @brief 所属Group取得
    [[nodiscard]] Group* GetOwnerGroup() const { return ownerGroup_; }

    //! @brief 所属Group設定
    void SetOwnerGroup(Group* group) { ownerGroup_ = group; }

    //! @brief 現在の行動状態取得
    [[nodiscard]] IndividualAction GetAction() const { return action_; }

    //! @brief 行動状態設定
    void SetAction(IndividualAction action) { action_ = action; }

    //------------------------------------------------------------------------
    // 状態管理（IndividualAction）
    //------------------------------------------------------------------------

    //! @brief 行動状態を更新（グループ状態・射程から決定）
    void UpdateAction();

    //! @brief 目標速度を計算（行動状態に基づく）
    void UpdateDesiredVelocity();

    //! @brief 攻撃ターゲットを取得
    [[nodiscard]] Individual* GetAttackTarget() const { return attackTarget_; }

    //! @brief 攻撃ターゲットを設定
    void SetAttackTarget(Individual* target) { attackTarget_ = target; }

    //! @brief 攻撃ターゲットを選択（ターゲットGroupからランダム）
    void SelectAttackTarget();

    //! @brief 攻撃中か判定
    [[nodiscard]] bool IsAttacking() const { return isAttacking_; }

    //! @brief 攻撃開始
    void StartAttack();

    //! @brief 攻撃終了
    void EndAttack();

    //! @brief Transform2D取得
    [[nodiscard]] Transform2D* GetTransform() const { return transform_; }

    //! @brief Collider2D取得
    [[nodiscard]] Collider2D* GetCollider() const { return collider_; }

    //! @brief GameObject取得
    [[nodiscard]] GameObject* GetGameObject() const { return gameObject_.get(); }

    //! @brief 攻撃ダメージ取得
    [[nodiscard]] float GetAttackDamage() const { return attackDamage_; }

    //! @brief 移動速度取得
    [[nodiscard]] float GetMoveSpeed() const { return moveSpeed_; }

    //! @brief 移動速度設定
    void SetMoveSpeed(float speed) { moveSpeed_ = speed; }

    //! @brief 目標速度を取得（Formation/AI用）
    [[nodiscard]] Vector2 GetDesiredVelocity() const { return desiredVelocity_; }

    //! @brief 目標速度を設定
    void SetDesiredVelocity(const Vector2& velocity) { desiredVelocity_ = velocity; }

    //! @brief 分離オフセットを取得
    [[nodiscard]] Vector2 GetSeparationOffset() const { return separationOffset_; }

    //! @brief 分離オフセットを設定
    void SetSeparationOffset(const Vector2& offset) { separationOffset_ = offset; }

    //! @brief 分離オフセットを計算（同グループ内の重なり回避）
    //! @param others 同グループ内の他の個体
    void CalculateSeparation(const std::vector<Individual*>& others);

    //! @brief 分離半径を取得
    [[nodiscard]] float GetSeparationRadius() const { return separationRadius_; }

    //! @brief 分離半径を設定
    void SetSeparationRadius(float radius) { separationRadius_ = radius; }

    //! @brief 分離力を取得
    [[nodiscard]] float GetSeparationForce() const { return separationForce_; }

    //! @brief 分離力を設定
    void SetSeparationForce(float force) { separationForce_ = force; }

    //! @brief AnimationController取得
    [[nodiscard]] AnimationController& GetAnimationController() { return animationController_; }
    [[nodiscard]] const AnimationController& GetAnimationController() const { return animationController_; }

protected:
    //! @brief テクスチャをセットアップ（派生クラスで実装）
    virtual void SetupTexture() = 0;

    //! @brief アニメーションをセットアップ（派生クラスで実装）
    virtual void SetupAnimator();

    //! @brief AnimationControllerをセットアップ（派生クラスでオーバーライド可）
    virtual void SetupAnimationController();

    //! @brief コライダーをセットアップ
    virtual void SetupCollider();

    // 識別
    std::string id_;

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
    float moveSpeed_ = 100.0f;
    float attackDamage_ = 10.0f;

    // 所属
    Group* ownerGroup_ = nullptr;

    // 状態
    IndividualAction action_ = IndividualAction::Idle;
    bool isAttacking_ = false;          //!< 攻撃モーション中

    // 攻撃ターゲット
    Individual* attackTarget_ = nullptr; //!< 攻撃対象の個体

    // 移動
    Vector2 desiredVelocity_ = Vector2::Zero;
    Vector2 separationOffset_ = Vector2::Zero;

    // 分離パラメータ
    float separationRadius_ = 20.0f;    //!< この距離以内で回避開始
    float separationForce_ = 50.0f;     //!< 回避の強さ

    // アニメーション設定（派生クラスで設定）
    int animRows_ = 1;
    int animCols_ = 1;
    int animFrameInterval_ = 6;

    // AnimationController
    AnimationController animationController_;
};
