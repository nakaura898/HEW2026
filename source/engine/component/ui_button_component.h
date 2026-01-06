//----------------------------------------------------------------------------
//! @file   ui_button_component.h
//! @brief  UIボタンコンポーネント
//----------------------------------------------------------------------------
#pragma once

#include "component.h"
#include "engine/math/math_types.h"
#include "engine/math/color.h"
#include <functional>

//----------------------------------------------------------------------------
//! @brief UIボタンコンポーネント
//! @details GameObjectにアタッチして使用するボタンUI
//!          Transformから位置を取得し、マウス入力でクリック判定を行う
//----------------------------------------------------------------------------
class UIButtonComponent : public Component
{
public:
    //! @brief ボタンの状態
    enum class State
    {
        Normal,     //!< 通常
        Hover,      //!< マウスオーバー
        Pressed     //!< 押下中
    };

    //! @brief コンストラクタ
    UIButtonComponent() = default;

    //! @brief デストラクタ
    ~UIButtonComponent() override = default;

    //------------------------------------------------------------------------
    // Component オーバーライド
    //------------------------------------------------------------------------

    //! @brief 更新処理
    //! @param deltaTime 前フレームからの経過時間
    void Update(float deltaTime) override;

    //------------------------------------------------------------------------
    // 設定
    //------------------------------------------------------------------------

    //! @brief ボタンのサイズを設定
    //! @param size 幅と高さ
    void SetSize(const Vector2& size) { size_ = size; }

    //! @brief ボタンのサイズを取得
    [[nodiscard]] const Vector2& GetSize() const { return size_; }

    //! @brief クリック時のコールバックを設定
    //! @param callback コールバック関数
    void SetOnClick(std::function<void()> callback) { onClick_ = std::move(callback); }

    //! @brief 色を設定
    //! @param normal 通常時の色
    //! @param hover ホバー時の色
    //! @param pressed 押下時の色
    void SetColors(const Color& normal, const Color& hover, const Color& pressed)
    {
        normalColor_ = normal;
        hoverColor_ = hover;
        pressColor_ = pressed;
    }

    //! @brief 通常時の色を設定
    void SetNormalColor(const Color& color) { normalColor_ = color; }

    //! @brief ホバー時の色を設定
    void SetHoverColor(const Color& color) { hoverColor_ = color; }

    //! @brief 押下時の色を設定
    void SetPressColor(const Color& color) { pressColor_ = color; }

    //------------------------------------------------------------------------
    // 状態取得
    //------------------------------------------------------------------------

    //! @brief 現在の状態を取得
    [[nodiscard]] State GetState() const { return state_; }

    //! @brief 現在の色を取得
    [[nodiscard]] const Color& GetCurrentColor() const { return currentColor_; }

    //! @brief マウスがボタン上にあるか
    [[nodiscard]] bool IsHovered() const { return state_ == State::Hover || state_ == State::Pressed; }

    //! @brief ボタンが押されているか
    [[nodiscard]] bool IsPressed() const { return state_ == State::Pressed; }

private:
    //! @brief マウスがボタン範囲内か判定
    [[nodiscard]] bool IsMouseOver() const;

    //! @brief ボタンの中心位置を取得（Transformから）
    [[nodiscard]] Vector2 GetPosition() const;

    Vector2 size_ = Vector2(100.0f, 40.0f);  //!< ボタンサイズ

    Color normalColor_ = Color(0.3f, 0.3f, 0.3f, 0.9f);   //!< 通常時の色
    Color hoverColor_ = Color(0.5f, 0.5f, 0.5f, 1.0f);    //!< ホバー時の色
    Color pressColor_ = Color(0.2f, 0.2f, 0.2f, 1.0f);    //!< 押下時の色
    Color currentColor_ = Color(0.3f, 0.3f, 0.3f, 0.9f);  //!< 現在の色

    State state_ = State::Normal;  //!< 現在の状態

    std::function<void()> onClick_;  //!< クリック時コールバック
};
