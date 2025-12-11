//----------------------------------------------------------------------------
//! @file   window.h
//! @brief  Win32ウィンドウ管理クラス
//----------------------------------------------------------------------------
#pragma once

#include "dx11/gpu_common.h"
#include <Windows.h>
#include <string>
#include <functional>
#include <cstdint>

//! @brief リサイズコールバック型
using ResizeCallback = std::function<void(uint32_t width, uint32_t height)>;

//! @brief フォーカスコールバック型
using FocusCallback = std::function<void(bool focused)>;

//----------------------------------------------------------------------------
//! @brief ウィンドウ作成パラメータ
//----------------------------------------------------------------------------
struct WindowDesc {
    HINSTANCE hInstance = nullptr;               //!< アプリケーションインスタンス
    std::wstring title = L"mutra Application";   //!< ウィンドウタイトル
    uint32_t width = 1280;                       //!< クライアント領域幅
    uint32_t height = 720;                       //!< クライアント領域高さ
    bool resizable = true;                       //!< リサイズ可能か
    uint32_t minWidth = 320;                     //!< 最小幅（WM_GETMINMAXINFO用）
    uint32_t minHeight = 240;                    //!< 最小高さ（WM_GETMINMAXINFO用）
};

//----------------------------------------------------------------------------
//! @brief Win32ウィンドウ管理クラス
//!
//! RAIIパターンでウィンドウを管理する。コピー禁止、ムーブ許可。
//----------------------------------------------------------------------------
class Window final : private NonCopyable {
public:
    //! @brief ウィンドウを作成
    //! @param [in] desc ウィンドウ設定
    explicit Window(const WindowDesc& desc = WindowDesc{});

    //! @brief デストラクタ
    ~Window() noexcept;

    //! @brief ムーブコンストラクタ
    Window(Window&& other) noexcept;
    //! @brief ムーブ代入演算子
    Window& operator=(Window&& other) noexcept;

    //----------------------------------------------------------
    //! @name メッセージ処理
    //----------------------------------------------------------
    //!@{

    //! @brief メッセージキューを処理
    //! @return WM_QUITでfalse
    [[nodiscard]] bool ProcessMessages() noexcept;

    //!@}

    //----------------------------------------------------------
    //! @name 状態取得
    //----------------------------------------------------------
    //!@{

    //! @brief 有効性確認
    [[nodiscard]] bool IsValid() const noexcept;

    //! @brief ウィンドウが閉じられるべきか
    [[nodiscard]] bool ShouldClose() const noexcept;

    //! @brief ウィンドウハンドルを取得
    [[nodiscard]] HWND GetHWND() const noexcept;

    //! @brief クライアント領域幅を取得
    [[nodiscard]] uint32_t GetWidth() const noexcept;

    //! @brief クライアント領域高さを取得
    [[nodiscard]] uint32_t GetHeight() const noexcept;

    //! @brief アスペクト比を取得
    [[nodiscard]] float GetAspectRatio() const noexcept;

    //! @brief フォーカス状態を取得
    [[nodiscard]] bool IsFocused() const noexcept;

    //! @brief 最小化状態を取得
    [[nodiscard]] bool IsMinimized() const noexcept;

    //!@}

    //----------------------------------------------------------
    //! @name ウィンドウ操作
    //----------------------------------------------------------
    //!@{

    //! @brief ウィンドウを閉じるリクエスト
    void RequestClose() noexcept;

    //!@}

    //----------------------------------------------------------
    //! @name コールバック設定
    //----------------------------------------------------------
    //!@{

    //! @brief リサイズコールバックを設定
    void SetResizeCallback(ResizeCallback callback) noexcept;

    //! @brief フォーカスコールバックを設定
    void SetFocusCallback(FocusCallback callback) noexcept;

    //!@}

private:
    //! @brief ウィンドウプロシージャ（静的）
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    //! @brief ウィンドウプロシージャ（メンバ）
    LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    //! @brief ウィンドウクラス登録
    bool RegisterWindowClass() noexcept;

    //! @brief ウィンドウ作成
    bool CreateWindowInternal(const WindowDesc& desc) noexcept;

    HWND hwnd_ = nullptr;
    HINSTANCE hInstance_ = nullptr;
    std::wstring className_;

    uint32_t width_ = 0;
    uint32_t height_ = 0;
    uint32_t minWidth_ = 320;
    uint32_t minHeight_ = 240;

    bool shouldClose_ = false;
    bool focused_ = true;
    bool minimized_ = false;

    ResizeCallback resizeCallback_;
    FocusCallback focusCallback_;
};
