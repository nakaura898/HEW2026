//----------------------------------------------------------------------------
//! @file   application.cpp
//! @brief  アプリケーション管理クラス 実装
//----------------------------------------------------------------------------
#include "application.h"
#include "dx11/graphics_device.h"
#include "dx11/graphics_context.h"
#include "engine/texture/texture_manager.h"
#include "common/logging/logging.h"
#include <algorithm>

//----------------------------------------------------------------------------
// シングルトン
//----------------------------------------------------------------------------
void Application::Create()
{
    if (!instance_) {
        instance_ = std::unique_ptr<Application>(new Application());
    }
}

void Application::Destroy()
{
    instance_.reset();
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

    // 4. TextureManager生成（Rendererが依存）
    TextureManager::Create();

    // 5. Renderer 初期化（固定解像度レンダーターゲット付き）
    Renderer::Create();
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

    // 6. リサイズコールバック設定
    window_->SetResizeCallback([this](uint32_t width, uint32_t height) {
        OnResize(width, height);
    });

    // 7. 時間管理初期化
    Timer::Start();

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

    // パイプラインから全リソースをアンバインドしてから解放
    auto& ctx = GraphicsContext::Get();
    if (auto* d3dCtx = ctx.GetContext()) {
        d3dCtx->ClearState();
        d3dCtx->Flush();
    }

    // 逆順で終了
    Renderer::Get().Shutdown();
    Renderer::Destroy();
    TextureManager::Destroy();
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
    InputManager::Get().Update(Timer::GetDeltaTime());
}

//----------------------------------------------------------------------------
// 時間管理アクセサ
//----------------------------------------------------------------------------

float Application::GetDeltaTime() const noexcept
{
    return Timer::GetDeltaTime();
}

float Application::GetTotalTime() const noexcept
{
    return Timer::GetTotalTime();
}

float Application::GetFPS() const noexcept
{
    return Timer::GetFPS();
}

uint64_t Application::GetFrameCount() const noexcept
{
    return Timer::GetFrameCount();
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
