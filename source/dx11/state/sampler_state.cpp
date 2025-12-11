//----------------------------------------------------------------------------
//! @file   sampler_state.cpp
//! @brief  サンプラーステート実装
//----------------------------------------------------------------------------
#include "sampler_state.h"
#include "dx11/graphics_device.h"
#include "common/logging/logging.h"

//----------------------------------------------------------------------------
std::unique_ptr<SamplerState> SamplerState::Create(const D3D11_SAMPLER_DESC& desc)
{
    auto* device = GetD3D11Device();
    RETURN_NULL_IF_NULL(device, "[SamplerState] D3D11Deviceがnullです");

    auto state = std::unique_ptr<SamplerState>(new SamplerState());

    HRESULT hr = device->CreateSamplerState(&desc, &state->sampler_);
    RETURN_NULL_IF_FAILED(hr, "[SamplerState] サンプラーステートの作成に失敗しました");

    return state;
}

//----------------------------------------------------------------------------
std::unique_ptr<SamplerState> SamplerState::CreateDefault()
{
    D3D11_SAMPLER_DESC desc{};
    desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    desc.MipLODBias = 0.0f;
    desc.MaxAnisotropy = 1;
    desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    desc.MinLOD = 0.0f;
    desc.MaxLOD = D3D11_FLOAT32_MAX;

    return Create(desc);
}

//----------------------------------------------------------------------------
std::unique_ptr<SamplerState> SamplerState::CreatePoint()
{
    D3D11_SAMPLER_DESC desc{};
    desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    desc.MipLODBias = 0.0f;
    desc.MaxAnisotropy = 1;
    desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    desc.MinLOD = 0.0f;
    desc.MaxLOD = D3D11_FLOAT32_MAX;

    return Create(desc);
}

//----------------------------------------------------------------------------
std::unique_ptr<SamplerState> SamplerState::CreateAnisotropic(uint32_t maxAnisotropy)
{
    D3D11_SAMPLER_DESC desc{};
    desc.Filter = D3D11_FILTER_ANISOTROPIC;
    desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    desc.MipLODBias = 0.0f;
    desc.MaxAnisotropy = (maxAnisotropy > 16) ? 16 : (maxAnisotropy < 1 ? 1 : maxAnisotropy);
    desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    desc.MinLOD = 0.0f;
    desc.MaxLOD = D3D11_FLOAT32_MAX;

    return Create(desc);
}

//----------------------------------------------------------------------------
std::unique_ptr<SamplerState> SamplerState::CreateComparison()
{
    D3D11_SAMPLER_DESC desc{};
    desc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
    desc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
    desc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
    desc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    desc.MipLODBias = 0.0f;
    desc.MaxAnisotropy = 1;
    desc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;
    desc.BorderColor[0] = 1.0f;
    desc.BorderColor[1] = 1.0f;
    desc.BorderColor[2] = 1.0f;
    desc.BorderColor[3] = 1.0f;
    desc.MinLOD = 0.0f;
    desc.MaxLOD = D3D11_FLOAT32_MAX;

    return Create(desc);
}
