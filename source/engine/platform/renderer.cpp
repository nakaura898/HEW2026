//----------------------------------------------------------------------------
//! @file   renderer.cpp
//! @brief  レンダラー管理クラス 実装
//----------------------------------------------------------------------------
#include "renderer.h"
#include "dx11/graphics_context.h"
#include "engine/texture/texture_manager.h"
#include "common/logging/logging.h"
#include <string>

//----------------------------------------------------------------------------
// シングルトン
//----------------------------------------------------------------------------

Renderer& Renderer::Get() noexcept
{
    static Renderer instance;
    return instance;
}

//----------------------------------------------------------------------------
// 初期化
//----------------------------------------------------------------------------

bool Renderer::Initialize(
    HWND hwnd,
    uint32_t windowWidth,
    uint32_t windowHeight,
    uint32_t renderWidth,
    uint32_t renderHeight,
    VSyncMode vsync)
{
    if (initialized_) {
        LOG_WARN("[Renderer] 既に初期化されています");
        return true;
    }

    if (!hwnd) {
        LOG_ERROR("[Renderer] ウィンドウハンドルがnullです");
        return false;
    }

    if (windowWidth == 0 || windowHeight == 0) {
        LOG_ERROR("[Renderer] ウィンドウサイズが無効です");
        return false;
    }

    if (renderWidth == 0 || renderHeight == 0) {
        LOG_ERROR("[Renderer] レンダリング解像度が無効です");
        return false;
    }

    // スワップチェーン記述子を設定
    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Width = windowWidth;
    desc.Height = windowHeight;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.Stereo = FALSE;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;  // ダブルバッファリング
    desc.Scaling = DXGI_SCALING_STRETCH;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;  // 可変リフレッシュレート対応

    // スワップチェーンを作成
    swapChain_ = std::make_unique<SwapChain>(hwnd, desc);
    if (!swapChain_->IsValid()) {
        LOG_ERROR("[Renderer] スワップチェーンの作成に失敗しました");
        swapChain_.reset();
        return false;
    }

    // 固定解像度レンダーターゲットを作成
    if (!CreateRenderTargets(renderWidth, renderHeight)) {
        LOG_ERROR("[Renderer] レンダーターゲットの作成に失敗しました");
        swapChain_.reset();
        return false;
    }

    renderWidth_ = renderWidth;
    renderHeight_ = renderHeight;
    vsync_ = vsync;
    initialized_ = true;

    LOG_INFO("[Renderer] 初期化完了");
    return true;
}

//----------------------------------------------------------------------------
// レンダーターゲット作成
//----------------------------------------------------------------------------

bool Renderer::CreateRenderTargets(uint32_t width, uint32_t height)
{
    // カラーバッファ（SRV付きでブリット用に使用可能）
    colorBuffer_ = TextureManager::Get().CreateRenderTarget(
        width, height, DXGI_FORMAT_R8G8B8A8_UNORM);
    if (!colorBuffer_) {
        LOG_ERROR("[Renderer] カラーバッファの作成に失敗しました");
        return false;
    }

    // 深度バッファ
    depthBuffer_ = TextureManager::Get().CreateDepthStencil(
        width, height, DXGI_FORMAT_D24_UNORM_S8_UINT);
    if (!depthBuffer_) {
        LOG_ERROR("[Renderer] 深度バッファの作成に失敗しました");
        colorBuffer_.reset();
        return false;
    }

    return true;
}

//----------------------------------------------------------------------------
// 終了処理
//----------------------------------------------------------------------------

void Renderer::Shutdown() noexcept
{
    if (!initialized_) {
        return;
    }

    LOG_INFO("[Renderer] 終了処理開始...");
    LOG_INFO("[Renderer] depthBuffer use_count: " + std::to_string(depthBuffer_.use_count()));
    LOG_INFO("[Renderer] colorBuffer use_count: " + std::to_string(colorBuffer_.use_count()));

    depthBuffer_.reset();
    colorBuffer_.reset();
    swapChain_.reset();

    renderWidth_ = 0;
    renderHeight_ = 0;
    vsync_ = VSyncMode::On;
    initialized_ = false;

    LOG_INFO("[Renderer] 終了処理完了");
}

//----------------------------------------------------------------------------
// 初期化確認
//----------------------------------------------------------------------------

bool Renderer::IsValid() const noexcept
{
    return initialized_ &&
           swapChain_ && swapChain_->IsValid() &&
           colorBuffer_ && depthBuffer_;
}

//----------------------------------------------------------------------------
// 画面表示
//----------------------------------------------------------------------------

void Renderer::Present()
{
    if (!swapChain_) {
        return;
    }

    swapChain_->Present(vsync_);
}

//----------------------------------------------------------------------------
// リサイズ
//----------------------------------------------------------------------------

void Renderer::Resize(uint32_t width, uint32_t height)
{
    if (!swapChain_) {
        return;
    }

    if (width == 0 || height == 0) {
        return;
    }

    // スワップチェーンのみリサイズ（カラー/深度バッファは固定解像度のまま）
    if (!swapChain_->Resize(width, height)) {
        LOG_ERROR("[Renderer] スワップチェーンのリサイズに失敗しました");
    }
}

//----------------------------------------------------------------------------
// カラーバッファ取得
//----------------------------------------------------------------------------

Texture* Renderer::GetColorBuffer() noexcept
{
    return colorBuffer_.get();
}

//----------------------------------------------------------------------------
// 深度バッファ取得
//----------------------------------------------------------------------------

Texture* Renderer::GetDepthBuffer() noexcept
{
    return depthBuffer_.get();
}

//----------------------------------------------------------------------------
// バックバッファ取得
//----------------------------------------------------------------------------

Texture* Renderer::GetBackBuffer() noexcept
{
    if (!swapChain_) {
        return nullptr;
    }
    return swapChain_->GetBackBuffer();
}

//----------------------------------------------------------------------------
// スワップチェーン取得
//----------------------------------------------------------------------------

SwapChain* Renderer::GetSwapChain() noexcept
{
    return swapChain_.get();
}

//----------------------------------------------------------------------------
// レンダリング解像度取得
//----------------------------------------------------------------------------

uint32_t Renderer::GetRenderWidth() const noexcept
{
    return renderWidth_;
}

uint32_t Renderer::GetRenderHeight() const noexcept
{
    return renderHeight_;
}
