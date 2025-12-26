#pragma once

#include "key.h"
#include <array>
#include <math/math_types.h>

// 前方宣言
struct HWND__;
using HWND = HWND__*;

/// @brief マウス入力管理クラス
///
/// 責務:
/// - マウス入力の状態管理
/// - マウス座標の取得
/// - ボタンの押下・離した瞬間の判定
/// - ホイールスクロール量の取得
/// - マウス移動量（デルタ）の計算
class Mouse {
public:
    /// @brief コンストラクタ
    Mouse() noexcept = default;

    /// @brief デストラクタ
    ~Mouse() noexcept = default;

    // コピー可能
    Mouse(const Mouse&) = default;
    Mouse& operator=(const Mouse&) = default;

    // ムーブ可能
    Mouse(Mouse&&) = default;
    Mouse& operator=(Mouse&&) = default;

    /// @brief マウスX座標を取得
    /// @return X座標（ウィンドウクライアント領域座標系）
    int GetX() const noexcept { return x_; }

    /// @brief マウスY座標を取得
    /// @return Y座標（ウィンドウクライアント領域座標系）
    int GetY() const noexcept { return y_; }

    /// @brief 前フレームからのX移動量を取得
    /// @return X方向の移動量（ピクセル）
    int GetDeltaX() const noexcept { return deltaX_; }

    /// @brief 前フレームからのY移動量を取得
    /// @return Y方向の移動量（ピクセル）
    int GetDeltaY() const noexcept { return deltaY_; }


    /// @brief マウスのX,Y座標を取得
    /// @return　(マウスのX座標、マウスのY座標)
    Vector2 GestPosition() const noexcept { return Vector2(static_cast<float>(x_), static_cast<float>(y_)); }

    /// @brief ホイールスクロール量を取得
    /// @return このフレームのホイールスクロール量（正=上、負=下）
    float GetWheelDelta() const noexcept { return wheelDelta_; }

    /// @brief ボタンが現在押されているか
    /// @param button ボタン
    /// @return 押されていればtrue
    bool IsButtonPressed(MouseButton button) const noexcept;

    /// @brief ボタンがこのフレームで押された瞬間か
    /// @param button ボタン
    /// @return 押された瞬間ならtrue
    bool IsButtonDown(MouseButton button) const noexcept;

    /// @brief ボタンがこのフレームで離された瞬間か
    /// @param button ボタン
    /// @return 離された瞬間ならtrue
    bool IsButtonUp(MouseButton button) const noexcept;

    /// @brief 入力状態を更新（内部用）
    /// @param hwnd ウィンドウハンドル（nullptrの場合はアクティブウィンドウを使用）
    void Update(HWND hwnd = nullptr) noexcept;

    /// @brief マウス座標を設定（内部用）
    /// @param x X座標
    /// @param y Y座標
    void SetPosition(int x, int y) noexcept;

    /// @brief ボタンダウンイベントを処理（内部用）
    /// @param button ボタン
    void OnButtonDown(MouseButton button) noexcept;

    /// @brief ボタンアップイベントを処理（内部用）
    /// @param button ボタン
    void OnButtonUp(MouseButton button) noexcept;

    /// @brief ホイールイベントを処理（内部用）
    /// @param delta スクロール量
    void OnWheel(float delta) noexcept;

private:
    /// @brief ボタン状態
    struct ButtonState {
        bool pressed = false;  // 現在押されているか
        bool down = false;     // このフレームで押されたか
        bool up = false;       // このフレームで離されたか
    };

    int x_ = 0;              // 現在のX座標
    int y_ = 0;              // 現在のY座標
    int prevX_ = 0;          // 前フレームのX座標
    int prevY_ = 0;          // 前フレームのY座標
    int deltaX_ = 0;         // X移動量
    int deltaY_ = 0;         // Y移動量
    float wheelDelta_ = 0.0f;  // ホイールスクロール量

    std::array<ButtonState, static_cast<size_t>(MouseButton::ButtonCount)> buttons_;
};
