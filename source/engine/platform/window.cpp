//----------------------------------------------------------------------------
//! @file   window.cpp
//! @brief  Win32ウィンドウ管理クラス 実装
//----------------------------------------------------------------------------
#include "window.h"
#include "common/logging/logging.h"

//----------------------------------------------------------------------------
// コンストラクタ・デストラクタ
//----------------------------------------------------------------------------

Window::Window(const WindowDesc& desc)
    : hInstance_(desc.hInstance)
    , width_(desc.width)
    , height_(desc.height)
    , minWidth_(desc.minWidth)
    , minHeight_(desc.minHeight)
{
    if (!hInstance_) {
        LOG_ERROR("HINSTANCE is null");
        return;
    }

    // ウィンドウクラス名（一意性のためにアドレスを使用）
    className_ = L"HEW2026" + std::to_wstring(reinterpret_cast<uintptr_t>(this));

    if (!RegisterWindowClass()) {
        LOG_ERROR("Failed to register window class");
        return;
    }

    if (!CreateWindowInternal(desc)) {
        LOG_ERROR("Failed to create window");
        return;
    }

    // ウィンドウを表示
    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);

    LOG_INFO("Window created successfully");
}

Window::~Window() noexcept
{
    if (hwnd_) {
        DestroyWindow(hwnd_);
        hwnd_ = nullptr;
    }

    if (hInstance_ && !className_.empty()) {
        UnregisterClassW(className_.c_str(), hInstance_);
    }
}

//----------------------------------------------------------------------------
// ムーブ
//----------------------------------------------------------------------------

Window::Window(Window&& other) noexcept
    : hwnd_(other.hwnd_)
    , hInstance_(other.hInstance_)
    , className_(std::move(other.className_))
    , width_(other.width_)
    , height_(other.height_)
    , minWidth_(other.minWidth_)
    , minHeight_(other.minHeight_)
    , shouldClose_(other.shouldClose_)
    , focused_(other.focused_)
    , minimized_(other.minimized_)
    , resizeCallback_(std::move(other.resizeCallback_))
    , focusCallback_(std::move(other.focusCallback_))
{
    // ムーブ元を無効化
    other.hwnd_ = nullptr;
    other.hInstance_ = nullptr;

    // WndProcのポインタを更新
    if (hwnd_) {
        SetWindowLongPtrW(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    }
}

Window& Window::operator=(Window&& other) noexcept
{
    if (this != &other) {
        // 既存のリソースを解放
        if (hwnd_) {
            DestroyWindow(hwnd_);
        }
        if (hInstance_ && !className_.empty()) {
            UnregisterClassW(className_.c_str(), hInstance_);
        }

        // メンバを移動
        hwnd_ = other.hwnd_;
        hInstance_ = other.hInstance_;
        className_ = std::move(other.className_);
        width_ = other.width_;
        height_ = other.height_;
        minWidth_ = other.minWidth_;
        minHeight_ = other.minHeight_;
        shouldClose_ = other.shouldClose_;
        focused_ = other.focused_;
        minimized_ = other.minimized_;
        resizeCallback_ = std::move(other.resizeCallback_);
        focusCallback_ = std::move(other.focusCallback_);

        // ムーブ元を無効化
        other.hwnd_ = nullptr;
        other.hInstance_ = nullptr;

        // WndProcのポインタを更新
        if (hwnd_) {
            SetWindowLongPtrW(hwnd_, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
        }
    }
    return *this;
}

//----------------------------------------------------------------------------
// メッセージ処理
//----------------------------------------------------------------------------

bool Window::ProcessMessages() noexcept
{
    MSG msg = {};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            shouldClose_ = true;
            return false;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return true;
}

//----------------------------------------------------------------------------
// 状態取得
//----------------------------------------------------------------------------

bool Window::IsValid() const noexcept
{
    return hwnd_ != nullptr;
}

bool Window::ShouldClose() const noexcept
{
    return shouldClose_;
}

HWND Window::GetHWND() const noexcept
{
    return hwnd_;
}

uint32_t Window::GetWidth() const noexcept
{
    return width_;
}

uint32_t Window::GetHeight() const noexcept
{
    return height_;
}

float Window::GetAspectRatio() const noexcept
{
    if (height_ == 0) return 1.0f;
    return static_cast<float>(width_) / static_cast<float>(height_);
}

bool Window::IsFocused() const noexcept
{
    return focused_;
}

bool Window::IsMinimized() const noexcept
{
    return minimized_;
}

//----------------------------------------------------------------------------
// ウィンドウ操作
//----------------------------------------------------------------------------

void Window::RequestClose() noexcept
{
    shouldClose_ = true;
}

//----------------------------------------------------------------------------
// コールバック設定
//----------------------------------------------------------------------------

void Window::SetResizeCallback(ResizeCallback callback) noexcept
{
    resizeCallback_ = std::move(callback);
}

void Window::SetFocusCallback(FocusCallback callback) noexcept
{
    focusCallback_ = std::move(callback);
}

//----------------------------------------------------------------------------
// ウィンドウプロシージャ
//----------------------------------------------------------------------------

LRESULT CALLBACK Window::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    Window* window = nullptr;

    if (msg == WM_CREATE) {
        // CreateWindowExWで渡したthisポインタを取得
        auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
        window = static_cast<Window*>(createStruct->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
    } else {
        window = reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    if (window) {
        return window->HandleMessage(hwnd, msg, wParam, lParam);
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

LRESULT Window::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_SIZE:
    {
        uint32_t newWidth = LOWORD(lParam);
        uint32_t newHeight = HIWORD(lParam);

        // 最小化チェック
        minimized_ = (wParam == SIZE_MINIMIZED);

        if (!minimized_ && (newWidth != width_ || newHeight != height_)) {
            width_ = newWidth;
            height_ = newHeight;

            if (resizeCallback_) {
                resizeCallback_(width_, height_);
            }
        }
        return 0;
    }

    case WM_ACTIVATE:
    {
        bool active = (LOWORD(wParam) != WA_INACTIVE);
        if (active != focused_) {
            focused_ = active;

            if (focusCallback_) {
                focusCallback_(focused_);
            }
        }
        return 0;
    }

    case WM_SETFOCUS:
        if (!focused_) {
            focused_ = true;
            if (focusCallback_) {
                focusCallback_(true);
            }
        }
        return 0;

    case WM_KILLFOCUS:
        if (focused_) {
            focused_ = false;
            if (focusCallback_) {
                focusCallback_(false);
            }
        }
        return 0;

    case WM_GETMINMAXINFO:
    {
        auto* minMaxInfo = reinterpret_cast<MINMAXINFO*>(lParam);
        minMaxInfo->ptMinTrackSize.x = static_cast<LONG>(minWidth_);
        minMaxInfo->ptMinTrackSize.y = static_cast<LONG>(minHeight_);
        return 0;
    }

    case WM_CLOSE:
        shouldClose_ = true;
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

//----------------------------------------------------------------------------
// 内部メソッド
//----------------------------------------------------------------------------

bool Window::RegisterWindowClass() noexcept
{
    WNDCLASSEXW wcex = {};
    wcex.cbSize = sizeof(WNDCLASSEXW);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance_;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    wcex.lpszClassName = className_.c_str();

    if (!RegisterClassExW(&wcex)) {
        return false;
    }

    return true;
}

bool Window::CreateWindowInternal(const WindowDesc& desc) noexcept
{
    // ウィンドウスタイルの決定
    DWORD style = WS_OVERLAPPEDWINDOW;
    if (!desc.resizable) {
        style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
    }

    // クライアント領域が指定サイズになるように調整
    RECT rect = { 0, 0, static_cast<LONG>(desc.width), static_cast<LONG>(desc.height) };
    AdjustWindowRect(&rect, style, FALSE);

    int windowWidth = rect.right - rect.left;
    int windowHeight = rect.bottom - rect.top;

    // ウィンドウを画面中央に配置
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int posX = (screenWidth - windowWidth) / 2;
    int posY = (screenHeight - windowHeight) / 2;

    hwnd_ = CreateWindowExW(
        0,
        className_.c_str(),
        desc.title.c_str(),
        style,
        posX, posY,
        windowWidth, windowHeight,
        nullptr,
        nullptr,
        hInstance_,
        this  // WM_CREATEのlParamとして渡す
    );

    return hwnd_ != nullptr;
}
