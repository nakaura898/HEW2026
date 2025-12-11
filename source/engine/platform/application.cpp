//----------------------------------------------------------------------------
//! @file   application.cpp
//! @brief  アプリケーション管理クラス 実装
//----------------------------------------------------------------------------
#include "application.h"
#include "dx11/graphics_device.h"
#include "dx11/graphics_context.h"
#include "common/logging/logging.h"
#include <algorithm>

//----------------------------------------------------------------------------
// シングルトン
//----------------------------------------------------------------------------

Application& Application::Get() noexcept
{
    static Application instance;
    return instance;
}

//----------------------------------------------------------------------------
// 初期化
//----------------------------------------------------------------------------

bool Application::Initialize(const ApplicationDesc& desc)
{
    if (initialized_) {
        LOG_WARN("[Application] 既に初期化されています");
        return true;
    }

    desc_ = desc;

    // hInstanceが未指定の場合はGetModuleHandleで取得
    if (!desc_.hInstance) {
        desc_.hInstance = GetModuleHandle(nullptr);
    }

    // 1. Window 作成
    WindowDesc windowDesc = desc_.window;
    windowDesc.hInstance = desc_.hInstance;
    window_ = std::make_unique<Window>(windowDesc);
    if (!window_->IsValid()) {
        LOG_ERROR("[Application] ウィンドウの作成に失敗しました");
        return false;
    }

    // 2. GraphicsDevice 初期化
    if (!GraphicsDevice::Get().Initialize(desc.enableDebugLayer)) {
        LOG_ERROR("[Application] GraphicsDeviceの初期化に失敗しました");
        window_.reset();
        return false;
    }

    // 3. GraphicsContext 初期化
    if (!GraphicsContext::Get().Initialize()) {
        LOG_ERROR("[Application] GraphicsContextの初期化に失敗しました");
        GraphicsDevice::Get().Shutdown();
        window_.reset();
        return false;
    }

    // 4. Renderer 初期化（固定解像度レンダーターゲット付き）
    if (!Renderer::Get().Initialize(
            window_->GetHWND(),
            window_->GetWidth(),
            window_->GetHeight(),
            desc.renderWidth,
            desc.renderHeight,
            desc.vsync)) {
        LOG_ERROR("[Application] Rendererの初期化に失敗しました");
        GraphicsContext::Get().Shutdown();
        GraphicsDevice::Get().Shutdown();
        window_.reset();
        return false;
    }

    // 5. リサイズコールバック設定
    window_->SetResizeCallback([this](uint32_t width, uint32_t height) {
        OnResize(width, height);
    });

    // 6. 時間管理初期化
    deltaTime_ = 0.0f;
    totalTime_ = 0.0f;
    frameCount_ = 0;
    fps_ = 0.0f;
    fpsFrameCount_ = 0;
    fpsTimer_ = 0.0f;

    auto now = std::chrono::high_resolution_clock::now();
    startTime_ = now;
    lastFrameTime_ = now;

    initialized_ = true;
    shouldQuit_ = false;

    LOG_INFO("[Application] 初期化完了");
    return true;
}

//----------------------------------------------------------------------------
// 終了処理
//----------------------------------------------------------------------------

void Application::Shutdown() noexcept
{
    if (!initialized_) {
        return;
    }

    // 逆順で終了
    Renderer::Get().Shutdown();
    GraphicsContext::Get().Shutdown();
    GraphicsDevice::Get().Shutdown();
    window_.reset();

    initialized_ = false;
    shouldQuit_ = false;

    LOG_INFO("[Application] 終了処理完了");
}

//----------------------------------------------------------------------------
// 終了要求
//----------------------------------------------------------------------------

void Application::Quit() noexcept
{
    shouldQuit_ = true;
}

//----------------------------------------------------------------------------
// 時間更新
//----------------------------------------------------------------------------

void Application::UpdateTime() noexcept
{
    auto now = std::chrono::high_resolution_clock::now();
    auto elapsed = std::chrono::duration<float>(now - lastFrameTime_).count();
    lastFrameTime_ = now;

    // デバッグ時の異常値防止（ブレークポイント停止後など）
    deltaTime_ = (std::min)(elapsed, desc_.maxDeltaTime);
    totalTime_ += deltaTime_;

    // FPS計算（1秒ごとに更新）
    ++fpsFrameCount_;
    fpsTimer_ += deltaTime_;
    if (fpsTimer_ >= 1.0f) {
        fps_ = static_cast<float>(fpsFrameCount_) / fpsTimer_;
        fpsFrameCount_ = 0;
        fpsTimer_ = 0.0f;
    }
}

//----------------------------------------------------------------------------
// リサイズ処理
//----------------------------------------------------------------------------

void Application::OnResize(uint32_t width, uint32_t height) noexcept
{
    if (width == 0 || height == 0) {
        return;
    }

    Renderer::Get().Resize(width, height);
}

//----------------------------------------------------------------------------
// 入力処理
//----------------------------------------------------------------------------

#include "engine/input/input_manager.h"

void Application::ProcessInput()
{
    if (auto* inputMgr = InputManager::GetInstance()) {
        inputMgr->Update(deltaTime_);
    }
}

//----------------------------------------------------------------------------
// 時間管理アクセサ
//----------------------------------------------------------------------------

float Application::GetDeltaTime() const noexcept
{
    return deltaTime_;
}

float Application::GetTotalTime() const noexcept
{
    return totalTime_;
}

float Application::GetFPS() const noexcept
{
    return fps_;
}

uint64_t Application::GetFrameCount() const noexcept
{
    return frameCount_;
}

//----------------------------------------------------------------------------
// サブシステムアクセス
//----------------------------------------------------------------------------

HINSTANCE Application::GetHInstance() const noexcept
{
    return desc_.hInstance;
}

HWND Application::GetHWND() const noexcept
{
    return window_ ? window_->GetHWND() : nullptr;
}

Window* Application::GetWindow() noexcept
{
    return window_.get();
}

const Window* Application::GetWindow() const noexcept
{
    return window_.get();
}
