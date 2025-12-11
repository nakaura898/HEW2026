#pragma once

#include "key.h"
#include <array>

/// @brief キーボード入力管理クラス
///
/// 責務:
/// - キーボード入力の状態管理
/// - キーの押下・離した瞬間の判定
/// - 長押し時間の計測
/// - 修飾キーの状態取得
class Keyboard {
public:
    /// @brief コンストラクタ
    Keyboard() noexcept = default;

    /// @brief デストラクタ
    ~Keyboard() noexcept = default;

    // コピー可能
    Keyboard(const Keyboard&) = default;
    Keyboard& operator=(const Keyboard&) = default;

    // ムーブ可能
    Keyboard(Keyboard&&) = default;
    Keyboard& operator=(Keyboard&&) = default;

    /// @brief キーが現在押されているか
    /// @param key キーコード
    /// @return 押されていればtrue
    bool IsKeyPressed(Key key) const noexcept;

    /// @brief キーがこのフレームで押された瞬間か
    /// @param key キーコード
    /// @return 押された瞬間ならtrue
    bool IsKeyDown(Key key) const noexcept;

    /// @brief キーがこのフレームで離された瞬間か
    /// @param key キーコード
    /// @return 離された瞬間ならtrue
    bool IsKeyUp(Key key) const noexcept;

    /// @brief キーの長押し時間を取得（秒）
    /// @param key キーコード
    /// @return 押し続けている時間（秒）、押されていなければ0.0f
    float GetKeyHoldTime(Key key) const noexcept;

    /// @brief Shiftキーが押されているか
    /// @return 左右いずれかのShiftが押されていればtrue
    bool IsShiftPressed() const noexcept;

    /// @brief Controlキーが押されているか
    /// @return 左右いずれかのControlが押されていればtrue
    bool IsControlPressed() const noexcept;

    /// @brief Altキーが押されているか
    /// @return 左右いずれかのAltが押されていればtrue
    bool IsAltPressed() const noexcept;

    /// @brief 入力状態を更新（内部用）
    /// @param deltaTime 前フレームからの経過時間（秒）
    void Update(float deltaTime) noexcept;

    /// @brief キーダウンイベントを処理（内部用）
    /// @param key キーコード
    void OnKeyDown(int virtualKey) noexcept;

    /// @brief キーアップイベントを処理（内部用）
    /// @param key キーコード
    void OnKeyUp(int virtualKey) noexcept;

private:
    /// @brief キー状態
    struct KeyState {
        bool pressed = false;      // 現在押されているか
        bool down = false;         // このフレームで押されたか
        bool up = false;           // このフレームで離されたか
        float holdTime = 0.0f;     // 長押し時間（秒）
    };

    std::array<KeyState, static_cast<size_t>(Key::KeyCount)> keys_;
};
