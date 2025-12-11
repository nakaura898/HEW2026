//----------------------------------------------------------------------------
//! @file   shader_resource_view.cpp
//! @brief  シェーダーリソースビュー（SRV）ラッパークラス実装
//----------------------------------------------------------------------------
#include "shader_resource_view.h"
#include "dx11/graphics_device.h"
#include "common/logging/logging.h"

std::unique_ptr<ShaderResourceView> ShaderResourceView::CreateFromBuffer(
    ID3D11Buffer* buffer,
    const D3D11_SHADER_RESOURCE_VIEW_DESC* desc)
{
    RETURN_NULL_IF_NULL(buffer, "ShaderResourceView::CreateFromBuffer - buffer is null");

    auto* device = GetD3D11Device();
    RETURN_NULL_IF_NULL(device, "ShaderResourceView::CreateFromBuffer - device is null");

    auto view = std::unique_ptr<ShaderResourceView>(new ShaderResourceView());
    HRESULT hr = device->CreateShaderResourceView(buffer, desc, view->srv_.GetAddressOf());
    RETURN_NULL_IF_FAILED(hr, "ShaderResourceView::CreateFromBuffer - CreateShaderResourceView failed");

    return view;
}

std::unique_ptr<ShaderResourceView> ShaderResourceView::CreateFromTexture1D(
    ID3D11Texture1D* texture,
    const D3D11_SHADER_RESOURCE_VIEW_DESC* desc)
{
    RETURN_NULL_IF_NULL(texture, "ShaderResourceView::CreateFromTexture1D - texture is null");

    auto* device = GetD3D11Device();
    RETURN_NULL_IF_NULL(device, "ShaderResourceView::CreateFromTexture1D - device is null");

    auto view = std::unique_ptr<ShaderResourceView>(new ShaderResourceView());
    HRESULT hr = device->CreateShaderResourceView(texture, desc, view->srv_.GetAddressOf());
    RETURN_NULL_IF_FAILED(hr, "ShaderResourceView::CreateFromTexture1D - CreateShaderResourceView failed");

    return view;
}

std::unique_ptr<ShaderResourceView> ShaderResourceView::CreateFromTexture2D(
    ID3D11Texture2D* texture,
    const D3D11_SHADER_RESOURCE_VIEW_DESC* desc)
{
    RETURN_NULL_IF_NULL(texture, "ShaderResourceView::CreateFromTexture2D - texture is null");

    auto* device = GetD3D11Device();
    RETURN_NULL_IF_NULL(device, "ShaderResourceView::CreateFromTexture2D - device is null");

    auto view = std::unique_ptr<ShaderResourceView>(new ShaderResourceView());
    HRESULT hr = device->CreateShaderResourceView(texture, desc, view->srv_.GetAddressOf());
    RETURN_NULL_IF_FAILED(hr, "ShaderResourceView::CreateFromTexture2D - CreateShaderResourceView failed");

    return view;
}

std::unique_ptr<ShaderResourceView> ShaderResourceView::CreateFromTexture3D(
    ID3D11Texture3D* texture,
    const D3D11_SHADER_RESOURCE_VIEW_DESC* desc)
{
    RETURN_NULL_IF_NULL(texture, "ShaderResourceView::CreateFromTexture3D - texture is null");

    auto* device = GetD3D11Device();
    RETURN_NULL_IF_NULL(device, "ShaderResourceView::CreateFromTexture3D - device is null");

    auto view = std::unique_ptr<ShaderResourceView>(new ShaderResourceView());
    HRESULT hr = device->CreateShaderResourceView(texture, desc, view->srv_.GetAddressOf());
    RETURN_NULL_IF_FAILED(hr, "ShaderResourceView::CreateFromTexture3D - CreateShaderResourceView failed");

    return view;
}

std::unique_ptr<ShaderResourceView> ShaderResourceView::Create(
    ID3D11Resource* resource,
    const D3D11_SHADER_RESOURCE_VIEW_DESC& desc)
{
    RETURN_NULL_IF_NULL(resource, "ShaderResourceView::Create - resource is null");

    auto* device = GetD3D11Device();
    RETURN_NULL_IF_NULL(device, "ShaderResourceView::Create - device is null");

    auto view = std::unique_ptr<ShaderResourceView>(new ShaderResourceView());
    HRESULT hr = device->CreateShaderResourceView(resource, &desc, view->srv_.GetAddressOf());
    RETURN_NULL_IF_FAILED(hr, "ShaderResourceView::Create - CreateShaderResourceView failed");

    return view;
}

std::unique_ptr<ShaderResourceView> ShaderResourceView::FromD3DView(
    ComPtr<ID3D11ShaderResourceView> srv)
{
    RETURN_NULL_IF_NULL(srv.Get(), "ShaderResourceView::FromD3DView - srv is null");

    auto view = std::unique_ptr<ShaderResourceView>(new ShaderResourceView());
    view->srv_ = std::move(srv);
    return view;
}

D3D11_SHADER_RESOURCE_VIEW_DESC ShaderResourceView::GetDesc() const noexcept
{
    D3D11_SHADER_RESOURCE_VIEW_DESC desc = {};
    if (srv_) {
        srv_->GetDesc(&desc);
    }
    return desc;
}
