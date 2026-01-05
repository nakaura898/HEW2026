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
#include "engine/texture/texture_types.h"
#include "game/systems/animation/individual_state_machine.h"
#include "game/systems/animation/individual_intent.h"
#include "game/systems/animation/animation_controller.h"  // AnimationState enum
#include <memory>
#include <string>
#include <vector>
#include <cstdint>

// 前方宣言
class Group;
class Player;
class SpriteBatch;
struct AnimationDecisionContext;

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

    //! @brief 現在の攻撃対象の位置を取得（向き決定用）
    //! @param[out] outPosition 攻撃対象の位置
    //! @return 攻撃対象が存在すればtrue
    //! @note 派生クラス（Elf等）でオーバーライド可能
    [[nodiscard]] virtual bool GetCurrentAttackTargetPosition(Vector2& outPosition) const;

    //! @brief 攻撃ターゲットを選択（ターゲットGroupからランダム）
    void SelectAttackTarget();

    //! @brief 攻撃中か判定
    [[nodiscard]] bool IsAttacking() const;

    //! @brief 攻撃開始（Individual対象）
    //! @param target 攻撃対象
    //! @return 攻撃開始できたらtrue
    bool StartAttack(Individual* target);

    //! @brief 攻撃開始（Player対象）
    //! @param target 攻撃対象プレイヤー
    //! @return 攻撃開始できたらtrue
    bool StartAttackPlayer(Player* target);

    //! @brief 攻撃終了（コールバックから呼ばれる）
    void EndAttack();

    //! @brief 攻撃を強制中断（Love追従時などに使用）
    //! @details アクションをIdleに、アニメーションロックを解除
    void InterruptAttack();

    //! @brief 攻撃可能か（クールダウン完了かつ範囲に入った直後）
    [[nodiscard]] bool CanAttackNow() const;

    //! @brief 攻撃を中断可能か（一定時間経過後）
    [[nodiscard]] bool CanInterruptAttack() const;

    //! @brief 攻撃クールダウンを開始
    void StartAttackCooldown(float duration);

    //! @brief 攻撃クールダウンを更新
    void UpdateAttackCooldown(float dt);

    //! @brief Transform2D取得
    [[nodiscard]] Transform2D* GetTransform() const { return transform_; }

    //! @brief Collider2D取得
    [[nodiscard]] Collider2D* GetCollider() const { return collider_; }

    //! @brief GameObject取得
    [[nodiscard]] GameObject* GetGameObject() const { return gameObject_.get(); }

    //! @brief 攻撃ダメージ取得
    [[nodiscard]] float GetAttackDamage() const { return attackDamage_; }

    //! @brief 攻撃ダメージ設定
    void SetAttackDamage(float damage) { attackDamage_ = damage; }

    //! @brief 最大HP設定
    void SetMaxHp(float hp) { maxHp_ = hp; hp_ = hp; }

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

    //! @brief StateMachine取得
    [[nodiscard]] IndividualStateMachine* GetStateMachine() const { return stateMachine_.get(); }

    //! @brief Animator取得
    [[nodiscard]] Animator* GetAnimator() const { return animator_; }

    //! @brief フレンズ分配ダメージ受信中フラグ設定（FriendsDamageSharing用）
    void SetReceivingSharedDamage(bool receiving) { isReceivingSharedDamage_ = receiving; }

    //------------------------------------------------------------------------
    // 意図ベースアニメーション
    //------------------------------------------------------------------------

    //! @brief グループレベルの意図を取得
    //! @return グループAIの状態に基づく意図
    [[nodiscard]] GroupIntent GetGroupIntent() const;

    //! @brief 個体レベルの意図を取得
    //! @return 個体の現在の状態に基づく意図
    [[nodiscard]] IndividualIntent GetIndividualIntent() const;

    //! @brief 意図に基づいてアニメーション状態を決定
    //! @return 適用すべきアニメーション状態
    [[nodiscard]] AnimationState DetermineAnimationState() const;

    //! @brief 意図に基づいて向き方向を更新
    void UpdateFacingDirection();

    //! @brief グループ移動状態を設定（GroupAIから呼ばれる）
    void SetGroupMoving(bool moving) { isGroupMoving_ = moving; }

    //! @brief グループが移動中かどうか取得（GroupAIからの意図）
    [[nodiscard]] bool IsGroupMoving() const { return isGroupMoving_; }

    //! @brief 実際に位置が変化しているかどうか取得（フレーム間の実移動量に基づく）
    [[nodiscard]] bool IsActuallyMoving() const { return isActuallyMoving_; }

    //------------------------------------------------------------------------
    // アニメーション判定コンテキスト
    //------------------------------------------------------------------------

    //! @brief アニメーション判定コンテキストを構築
    //! @return 判定に必要な全コンテキスト（個体状態・グループ状態・関係性）
    [[nodiscard]] AnimationDecisionContext BuildAnimationContext() const;

protected:
    // 定数
    static constexpr float kDefaultColliderSize = 32.0f;    //!< デフォルトコライダーサイズ
    static constexpr float kMinDistanceThreshold = 0.001f;  //!< 零除算防止用の最小距離
    static constexpr float kFormationThreshold = 5.0f;      //!< フォーメーション復帰判定の距離閾値
    static constexpr float kAttackDuration = 0.8f;          //!< 攻撃モーション持続時間（秒）

    //! @brief テクスチャをセットアップ（派生クラスで実装）
    virtual void SetupTexture() = 0;

    //! @brief アニメーションをセットアップ（派生クラスで実装）
    virtual void SetupAnimator();

    //! @brief StateMachineをセットアップ（派生クラスでオーバーライド可）
    virtual void SetupStateMachine();

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
    IndividualAction prevAction_ = IndividualAction::Idle;  //!< 前フレームの行動
    bool isReceivingSharedDamage_ = false;  //!< フレンズ分配ダメージ受信中（無限ループ防止）
    bool facingRight_ = false;          //!< 向き状態（true=右向き）
    bool isGroupMoving_ = false;        //!< グループが移動中か（GroupAIから設定）

    // 攻撃ターゲット
    Individual* attackTarget_ = nullptr; //!< 攻撃対象の個体

    // 攻撃クールダウン
    float attackCooldown_ = 0.0f;       //!< 攻撃クールダウン残り時間
    bool justEnteredAttackRange_ = false; //!< 攻撃範囲に入った直後か

    // 移動
    Vector2 desiredVelocity_ = Vector2::Zero;
    Vector2 separationOffset_ = Vector2::Zero;
    Vector2 prevPosition_ = Vector2::Zero;    //!< 前フレームの位置（実移動量算出用）
    bool isActuallyMoving_ = false;           //!< 実際に位置が変化しているか

    // 分離パラメータ
    float separationRadius_ = 20.0f;    //!< この距離以内で回避開始
    float separationForce_ = 50.0f;     //!< 回避の強さ

    // アニメーション設定（派生クラスで設定）
    int animRows_ = 1;
    int animCols_ = 1;
    int animFrameInterval_ = 6;

    // StateMachine
    std::unique_ptr<IndividualStateMachine> stateMachine_;

#ifdef _DEBUG
    // デバッグログ用カウンター（const関数内で使用するためmutable）
    mutable int debugLogCounter_ = 0;
#endif
};
