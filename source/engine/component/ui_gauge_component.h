//----------------------------------------------------------------------------
//! @file   ui_gauge_component.h
//! @brief  UIゲージコンポーネント
//----------------------------------------------------------------------------
#pragma once

#include "component.h"
#include "engine/math/math_types.h"
#include "engine/math/color.h"

//----------------------------------------------------------------------------
//! @brief UIゲージコンポーネント
//! @details GameObjectにアタッチして使用するゲージUI
//!          Transformから位置を取得し、値に応じてゲージを描画する
//!          HPバー、スタミナバー、進捗表示などに使用可能
//----------------------------------------------------------------------------
class UIGaugeComponent : public Component
{
public:
    //! @brief コンストラクタ
    UIGaugeComponent() = default;

    //! @brief デストラクタ
    ~UIGaugeComponent() override = default;

    //------------------------------------------------------------------------
    // Component オーバーライド
    //------------------------------------------------------------------------

    //! @brief 更新処理
    //! @param deltaTime 前フレームからの経過時間
    void Update(float deltaTime) override;

    //------------------------------------------------------------------------
    // 描画
    //------------------------------------------------------------------------

    //! @brief ゲージを描画
    void Render();

    //------------------------------------------------------------------------
    // 値の設定・取得
    //------------------------------------------------------------------------

    //! @brief ゲージの値を設定
    //! @param value 値（0.0〜1.0にクランプされる）
    void SetValue(float value);

    //! @brief ゲージの値を取得
    //! @return 現在の値（0.0〜1.0）
    [[nodiscard]] float GetValue() const { return value_; }

    //! @brief ゲージの値を増減
    //! @param delta 変化量（正で増加、負で減少）
    void AddValue(float delta);

    //------------------------------------------------------------------------
    // サイズ設定
    //------------------------------------------------------------------------

    //! @brief ゲージのサイズを設定
    //! @param size 幅と高さ
    void SetSize(const Vector2& size) { size_ = size; }

    //! @brief ゲージのサイズを取得
    [[nodiscard]] const Vector2& GetSize() const { return size_; }

    //------------------------------------------------------------------------
    // 色設定
    //------------------------------------------------------------------------

    //! @brief 色を一括設定
    //! @param background 背景色
    //! @param fill 塗りつぶし色
    void SetColors(const Color& background, const Color& fill)
    {
        bgColor_ = background;
        fillColor_ = fill;
    }

    //! @brief 背景色を設定
    void SetBackgroundColor(const Color& color) { bgColor_ = color; }

    //! @brief 塗りつぶし色を設定
    void SetFillColor(const Color& color) { fillColor_ = color; }

    //! @brief 背景色を取得
    [[nodiscard]] const Color& GetBackgroundColor() const { return bgColor_; }

    //! @brief 塗りつぶし色を取得
    [[nodiscard]] const Color& GetFillColor() const { return fillColor_; }

    //------------------------------------------------------------------------
    // 状態取得
    //------------------------------------------------------------------------

    //! @brief ゲージが空かどうか
    [[nodiscard]] bool IsEmpty() const { return value_ <= 0.0f; }

    //! @brief ゲージが満タンかどうか
    [[nodiscard]] bool IsFull() const { return value_ >= 1.0f; }

private:
    //! @brief ゲージの中心位置を取得（Transformから）
    [[nodiscard]] Vector2 GetPosition() const;

    Vector2 size_ = Vector2(100.0f, 10.0f);  //!< ゲージサイズ

    float value_ = 1.0f;  //!< 現在の値（0.0〜1.0）

    Color bgColor_ = Color(0.2f, 0.2f, 0.2f, 0.8f);    //!< 背景色
    Color fillColor_ = Color(0.0f, 1.0f, 0.0f, 1.0f);  //!< 塗りつぶし色
};
