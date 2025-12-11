//----------------------------------------------------------------------------
//! @file   rasterizer_state.cpp
//! @brief  ラスタライザーステート実装
//----------------------------------------------------------------------------
#include "rasterizer_state.h"
#include "dx11/graphics_device.h"
#include "common/logging/logging.h"

//----------------------------------------------------------------------------
std::unique_ptr<RasterizerState> RasterizerState::Create(const D3D11_RASTERIZER_DESC& desc)
{
    auto* device = GetD3D11Device();
    RETURN_NULL_IF_NULL(device, "[RasterizerState] D3D11Deviceがnullです");

    auto state = std::unique_ptr<RasterizerState>(new RasterizerState());

    HRESULT hr = device->CreateRasterizerState(&desc, &state->rasterizer_);
    RETURN_NULL_IF_FAILED(hr, "[RasterizerState] ラスタライザーステートの作成に失敗しました");

    return state;
}

//----------------------------------------------------------------------------
std::unique_ptr<RasterizerState> RasterizerState::CreateDefault()
{
    D3D11_RASTERIZER_DESC desc{};
    desc.FillMode = D3D11_FILL_SOLID;
    desc.CullMode = D3D11_CULL_BACK;
    desc.FrontCounterClockwise = FALSE;
    desc.DepthBias = 0;
    desc.DepthBiasClamp = 0.0f;
    desc.SlopeScaledDepthBias = 0.0f;
    desc.DepthClipEnable = TRUE;
    desc.ScissorEnable = FALSE;
    desc.MultisampleEnable = FALSE;
    desc.AntialiasedLineEnable = FALSE;

    return Create(desc);
}

//----------------------------------------------------------------------------
std::unique_ptr<RasterizerState> RasterizerState::CreateWireframe()
{
    D3D11_RASTERIZER_DESC desc{};
    desc.FillMode = D3D11_FILL_WIREFRAME;
    desc.CullMode = D3D11_CULL_BACK;
    desc.FrontCounterClockwise = FALSE;
    desc.DepthBias = 0;
    desc.DepthBiasClamp = 0.0f;
    desc.SlopeScaledDepthBias = 0.0f;
    desc.DepthClipEnable = TRUE;
    desc.ScissorEnable = FALSE;
    desc.MultisampleEnable = FALSE;
    desc.AntialiasedLineEnable = TRUE;

    return Create(desc);
}

//----------------------------------------------------------------------------
std::unique_ptr<RasterizerState> RasterizerState::CreateNoCull()
{
    D3D11_RASTERIZER_DESC desc{};
    desc.FillMode = D3D11_FILL_SOLID;
    desc.CullMode = D3D11_CULL_NONE;
    desc.FrontCounterClockwise = FALSE;
    desc.DepthBias = 0;
    desc.DepthBiasClamp = 0.0f;
    desc.SlopeScaledDepthBias = 0.0f;
    desc.DepthClipEnable = TRUE;
    desc.ScissorEnable = FALSE;
    desc.MultisampleEnable = FALSE;
    desc.AntialiasedLineEnable = FALSE;

    return Create(desc);
}

//----------------------------------------------------------------------------
std::unique_ptr<RasterizerState> RasterizerState::CreateFrontCull()
{
    D3D11_RASTERIZER_DESC desc{};
    desc.FillMode = D3D11_FILL_SOLID;
    desc.CullMode = D3D11_CULL_FRONT;
    desc.FrontCounterClockwise = FALSE;
    desc.DepthBias = 0;
    desc.DepthBiasClamp = 0.0f;
    desc.SlopeScaledDepthBias = 0.0f;
    desc.DepthClipEnable = TRUE;
    desc.ScissorEnable = FALSE;
    desc.MultisampleEnable = FALSE;
    desc.AntialiasedLineEnable = FALSE;

    return Create(desc);
}

//----------------------------------------------------------------------------
std::unique_ptr<RasterizerState> RasterizerState::CreateShadowMap(
    int32_t depthBias,
    float slopeScaledDepthBias)
{
    D3D11_RASTERIZER_DESC desc{};
    desc.FillMode = D3D11_FILL_SOLID;
    desc.CullMode = D3D11_CULL_BACK;
    desc.FrontCounterClockwise = FALSE;
    desc.DepthBias = depthBias;
    desc.DepthBiasClamp = 0.0f;
    desc.SlopeScaledDepthBias = slopeScaledDepthBias;
    desc.DepthClipEnable = TRUE;
    desc.ScissorEnable = FALSE;
    desc.MultisampleEnable = FALSE;
    desc.AntialiasedLineEnable = FALSE;

    return Create(desc);
}
