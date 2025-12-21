//----------------------------------------------------------------------------
//! @file   group.h
//! @brief  Groupクラス - Individualのコンテナ、縁の接続単位、AI制御単位
//----------------------------------------------------------------------------
#pragma once

#include "individual.h"
#include "game/systems/movement/formation.h"
#include <memory>
#include <vector>
#include <string>
#include <functional>

// 前方宣言
class SpriteBatch;
class GroupAI;

//----------------------------------------------------------------------------
//! @brief Groupクラス
//! @details Individualのコンテナ。縁の接続単位。AI制御の単位。
//----------------------------------------------------------------------------
class Group
{
public:
    //! @brief コンストラクタ
    //! @param id 一意の識別子
    explicit Group(const std::string& id);

    //! @brief デストラクタ
    ~Group();

    //------------------------------------------------------------------------
    // ライフサイクル
    //------------------------------------------------------------------------

    //! @brief 初期化
    //! @param centerPosition グループの中心位置
    void Initialize(const Vector2& centerPosition);

    //! @brief 終了処理
    void Shutdown();

    //! @brief 更新
    //! @param dt デルタタイム
    void Update(float dt);

    //! @brief 描画
    //! @param spriteBatch SpriteBatch参照
    void Render(SpriteBatch& spriteBatch);

    //------------------------------------------------------------------------
    // 個体管理
    //------------------------------------------------------------------------

    //! @brief 個体を追加
    //! @param individual 追加する個体（所有権を移譲）
    void AddIndividual(std::unique_ptr<Individual> individual);

    //! @brief 生存個体リストを取得
    //! @return 生存中の個体リスト
    [[nodiscard]] std::vector<Individual*> GetAliveIndividuals() const;

    //! @brief ランダムな生存個体を取得
    //! @return ランダムに選ばれた生存個体（全滅時はnullptr）
    [[nodiscard]] Individual* GetRandomAliveIndividual() const;

    //! @brief 個体数を取得
    [[nodiscard]] size_t GetIndividualCount() const { return individuals_.size(); }

    //! @brief 生存個体数を取得
    [[nodiscard]] size_t GetAliveCount() const;

    //------------------------------------------------------------------------
    // 位置・状態
    //------------------------------------------------------------------------

    //! @brief グループの中心位置を取得（全個体の平均位置）
    [[nodiscard]] Vector2 GetPosition() const;

    //! @brief グループの中心位置を設定（全個体を相対移動）
    void SetPosition(const Vector2& position);

    //! @brief 全個体のHP割合を取得（0.0〜1.0）
    [[nodiscard]] float GetHpRatio() const;

    //! @brief 全滅判定
    //! @return 全個体が死亡していればtrue
    [[nodiscard]] bool IsDefeated() const;

    //------------------------------------------------------------------------
    // 脅威度
    //------------------------------------------------------------------------

    //! @brief 脅威度を取得（基本脅威度 × 修正値）
    [[nodiscard]] float GetThreat() const { return baseThreat_ * threatModifier_; }

    //! @brief 基本脅威度を取得
    [[nodiscard]] float GetBaseThreat() const { return baseThreat_; }

    //! @brief 基本脅威度を設定
    void SetBaseThreat(float threat) { baseThreat_ = threat; }

    //! @brief 脅威度修正値を取得
    [[nodiscard]] float GetThreatModifier() const { return threatModifier_; }

    //! @brief 脅威度修正値を設定（Flee時に0.5など）
    void SetThreatModifier(float modifier);

    //------------------------------------------------------------------------
    // 索敵
    //------------------------------------------------------------------------

    //! @brief 索敵範囲を取得
    [[nodiscard]] float GetDetectionRange() const { return detectionRange_; }

    //! @brief 索敵範囲を設定
    void SetDetectionRange(float range) { detectionRange_ = range; }

    //------------------------------------------------------------------------
    // アクセサ
    //------------------------------------------------------------------------

    //! @brief ID取得
    [[nodiscard]] const std::string& GetId() const { return id_; }

    //! @brief 全個体へのアクセス（読み取り専用）
    [[nodiscard]] const std::vector<std::unique_ptr<Individual>>& GetIndividuals() const { return individuals_; }

    //! @brief グループ内の最大攻撃範囲を取得
    [[nodiscard]] float GetMaxAttackRange() const;

    //------------------------------------------------------------------------
    // Formation
    //------------------------------------------------------------------------

    //! @brief Formationを取得
    [[nodiscard]] Formation& GetFormation() { return formation_; }
    [[nodiscard]] const Formation& GetFormation() const { return formation_; }

    //! @brief Formationを再構築（個体死亡時に呼び出す）
    void RebuildFormation();

    //------------------------------------------------------------------------
    // AI参照
    //------------------------------------------------------------------------

    //! @brief GroupAI参照を取得
    [[nodiscard]] GroupAI* GetAI() const { return ai_; }

    //! @brief GroupAI参照を設定
    void SetAI(GroupAI* ai) { ai_ = ai; }

    //------------------------------------------------------------------------
    // コールバック
    //------------------------------------------------------------------------

    //! @brief 全滅時のコールバックを設定
    void SetOnDefeated(std::function<void(Group*)> callback) { onDefeated_ = std::move(callback); }

    //! @brief 脅威度変更時のコールバックを設定
    void SetOnThreatChanged(std::function<void(Group*)> callback) { onThreatChanged_ = std::move(callback); }

private:
    //! @brief 全滅チェックと通知
    void CheckDefeated();

    // 識別
    std::string id_;

    // 個体リスト
    std::vector<std::unique_ptr<Individual>> individuals_;

    // 脅威度
    float baseThreat_ = 100.0f;
    float threatModifier_ = 1.0f;

    // 索敵範囲
    float detectionRange_ = 300.0f;

    // 状態
    bool isDefeated_ = false;

    // 陣形
    Formation formation_;

    // AI参照（外部で管理）
    GroupAI* ai_ = nullptr;

    // コールバック
    std::function<void(Group*)> onDefeated_;
    std::function<void(Group*)> onThreatChanged_;
};
