//----------------------------------------------------------------------------
//! @file   render_state_manager.cpp
//! @brief  レンダーステートマネージャー実装
//----------------------------------------------------------------------------
#include "render_state_manager.h"
#include "dx11/graphics_context.h"
#include "common/logging/logging.h"

//----------------------------------------------------------------------------
// シングルトン
//----------------------------------------------------------------------------
void RenderStateManager::Create()
{
    if (!instance_) {
        instance_ = std::unique_ptr<RenderStateManager>(new RenderStateManager());
    }
}

void RenderStateManager::Destroy()
{
    instance_.reset();
}

//----------------------------------------------------------------------------
// 初期化
//----------------------------------------------------------------------------
bool RenderStateManager::Initialize()
{
    if (initialized_) {
        return true;
    }

    LOG_INFO("[RenderStateManager] 初期化開始");

    // ブレンドステート
    opaque_ = BlendState::CreateOpaque();
    alphaBlend_ = BlendState::CreateAlphaBlend();
    additive_ = BlendState::CreateAdditive();
    premultipliedAlpha_ = BlendState::CreatePremultipliedAlpha();

    // 純粋加算ブレンド（ONE + ONE、蓄積用）
    {
        D3D11_BLEND_DESC desc{};
        desc.AlphaToCoverageEnable = FALSE;
        desc.IndependentBlendEnable = FALSE;
        desc.RenderTarget[0].BlendEnable = TRUE;
        desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
        desc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
        desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
        desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        pureAdditive_ = BlendState::Create(desc);
    }

    if (!opaque_ || !alphaBlend_ || !additive_ || !pureAdditive_ || !premultipliedAlpha_) {
        LOG_ERROR("[RenderStateManager] ブレンドステート作成失敗");
        return false;
    }

    // サンプラーステート
    linearWrap_ = SamplerState::CreateDefault();
    linearClamp_ = SamplerState::CreateClamp();
    pointWrap_ = SamplerState::CreatePoint();

    // ポイント + クランプ（カスタム作成）
    {
        D3D11_SAMPLER_DESC desc = {};
        desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
        desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        desc.MaxAnisotropy = 1;
        desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
        desc.MaxLOD = D3D11_FLOAT32_MAX;
        pointClamp_ = SamplerState::Create(desc);
    }

    if (!linearWrap_ || !linearClamp_ || !pointWrap_ || !pointClamp_) {
        LOG_ERROR("[RenderStateManager] サンプラーステート作成失敗");
        return false;
    }

    // ラスタライザーステート
    rasterizerDefault_ = RasterizerState::CreateDefault();
    noCull_ = RasterizerState::CreateNoCull();
    wireframe_ = RasterizerState::CreateWireframe();

    if (!rasterizerDefault_ || !noCull_ || !wireframe_) {
        LOG_ERROR("[RenderStateManager] ラスタライザーステート作成失敗");
        return false;
    }

    // 深度ステンシルステート
    depthDefault_ = DepthStencilState::CreateDefault();
    depthReadOnly_ = DepthStencilState::CreateReadOnly();
    depthDisabled_ = DepthStencilState::CreateDisabled();
    depthLessEqual_ = DepthStencilState::CreateLessEqual();

    if (!depthDefault_ || !depthReadOnly_ || !depthDisabled_ || !depthLessEqual_) {
        LOG_ERROR("[RenderStateManager] 深度ステンシルステート作成失敗");
        return false;
    }

    initialized_ = true;
    LOG_INFO("[RenderStateManager] 初期化完了");
    return true;
}

//----------------------------------------------------------------------------
// 終了
//----------------------------------------------------------------------------
void RenderStateManager::Shutdown()
{
    if (!initialized_) {
        return;
    }

    LOG_INFO("[RenderStateManager] シャットダウン開始");

    // パイプラインからステートをアンバインド（リソース解放前に必須）
    auto* d3dCtx = GraphicsContext::Get().GetContext();
    if (d3dCtx) {
        d3dCtx->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
        d3dCtx->OMSetDepthStencilState(nullptr, 0);
        d3dCtx->RSSetState(nullptr);
        ID3D11SamplerState* nullSamplers[1] = { nullptr };
        d3dCtx->PSSetSamplers(0, 1, nullSamplers);
        d3dCtx->VSSetSamplers(0, 1, nullSamplers);
        d3dCtx->Flush();
    }

    // 深度ステンシルステート
    depthLessEqual_.reset();
    depthDisabled_.reset();
    depthReadOnly_.reset();
    depthDefault_.reset();

    // ラスタライザーステート
    wireframe_.reset();
    noCull_.reset();
    rasterizerDefault_.reset();

    // サンプラーステート
    pointClamp_.reset();
    pointWrap_.reset();
    linearClamp_.reset();
    linearWrap_.reset();

    // ブレンドステート
    premultipliedAlpha_.reset();
    pureAdditive_.reset();
    additive_.reset();
    alphaBlend_.reset();
    opaque_.reset();

    initialized_ = false;
    LOG_INFO("[RenderStateManager] シャットダウン完了");
}
