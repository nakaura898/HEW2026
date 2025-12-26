//----------------------------------------------------------------------------
//! @file   formation.h
//! @brief  Formation - グループ内の個体配置を管理
//----------------------------------------------------------------------------
#pragma once

#include <SimpleMath.h>
#include <vector>

using DirectX::SimpleMath::Vector2;

// 前方宣言
class Individual;

//----------------------------------------------------------------------------
//! @brief 陣形スロット - 各個体の配置情報
//----------------------------------------------------------------------------
struct FormationSlot
{
    Vector2 offset;             //!< 中心からの相対位置
    Individual* owner = nullptr; //!< このスロットを使う個体
};

//----------------------------------------------------------------------------
//! @brief 陣形パターン
//----------------------------------------------------------------------------
enum class FormationType
{
    Circle,     //!< 円形（汎用）
    Line,       //!< 横一列（遠距離用）
    Wedge       //!< V字（突撃用）
};

//----------------------------------------------------------------------------
//! @brief Formation - グループ内の個体配置を管理
//! @details Groupが所有し、個体のFormation上の目標位置を提供する
//----------------------------------------------------------------------------
class Formation
{
public:
    //! @brief コンストラクタ
    Formation();

    //! @brief デストラクタ
    ~Formation();

    //------------------------------------------------------------------------
    // 初期化・更新
    //------------------------------------------------------------------------

    //! @brief 初期化
    //! @param individuals 所属する個体リスト
    //! @param center 陣形の中心位置
    void Initialize(const std::vector<Individual*>& individuals, const Vector2& center);

    //! @brief 陣形を再生成（個体死亡時など）
    //! @param aliveIndividuals 生存個体リスト
    void Rebuild(const std::vector<Individual*>& aliveIndividuals);

    //! @brief 中心位置を更新
    //! @param center 新しい中心位置
    void SetCenter(const Vector2& center) { center_ = center; }

    //! @brief 更新（グループ目標に向けて中心を移動）
    //! @param targetPosition 目標位置
    //! @param speed 移動速度
    //! @param dt デルタタイム
    void Update(const Vector2& targetPosition, float speed, float dt);

    //------------------------------------------------------------------------
    // スロット情報取得
    //------------------------------------------------------------------------

    //! @brief 指定個体のFormation上の目標位置を取得
    //! @param individual 対象の個体
    //! @return 目標位置（スロットが見つからなければ中心位置）
    [[nodiscard]] Vector2 GetSlotPosition(const Individual* individual) const;

    //! @brief 指定個体がスロットを持っているか
    //! @param individual 対象の個体
    //! @return スロットを持っていればtrue
    [[nodiscard]] bool HasSlot(const Individual* individual) const;

    //! @brief 中心位置を取得
    [[nodiscard]] const Vector2& GetCenter() const { return center_; }

    //! @brief スロット数を取得
    [[nodiscard]] size_t GetSlotCount() const { return slots_.size(); }

    //------------------------------------------------------------------------
    // 設定
    //------------------------------------------------------------------------

    //! @brief 陣形パターンを設定
    void SetFormationType(FormationType type) { formationType_ = type; }

    //! @brief 陣形パターンを取得
    [[nodiscard]] FormationType GetFormationType() const { return formationType_; }

    //! @brief 間隔を設定
    void SetSpacing(float spacing) { spacing_ = spacing; }

    //! @brief 間隔を取得
    [[nodiscard]] float GetSpacing() const { return spacing_; }

private:
    //! @brief スロットを生成
    //! @param count 生成するスロット数
    void GenerateSlots(size_t count);

    //! @brief 円形配置のオフセットを計算
    //! @param index スロットインデックス
    //! @param total 総数
    //! @return 中心からのオフセット
    [[nodiscard]] Vector2 CalculateCircleOffset(size_t index, size_t total) const;

    //! @brief 横一列配置のオフセットを計算
    [[nodiscard]] Vector2 CalculateLineOffset(size_t index, size_t total) const;

    //! @brief V字配置のオフセットを計算
    [[nodiscard]] Vector2 CalculateWedgeOffset(size_t index, size_t total) const;

    // 中心位置
    Vector2 center_ = Vector2::Zero;

    // スロットリスト
    std::vector<FormationSlot> slots_;

    // 陣形パターン
    FormationType formationType_ = FormationType::Circle;

    // 間隔
    float spacing_ = 50.0f;
};
