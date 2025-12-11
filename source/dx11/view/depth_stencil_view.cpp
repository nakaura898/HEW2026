//----------------------------------------------------------------------------
//! @file   depth_stencil_view.cpp
//! @brief  深度ステンシルビュー（DSV）ラッパークラス実装
//----------------------------------------------------------------------------
#include "depth_stencil_view.h"
#include "dx11/graphics_device.h"
#include "common/logging/logging.h"

std::unique_ptr<DepthStencilView> DepthStencilView::CreateFromTexture1D(
    ID3D11Texture1D* texture,
    const D3D11_DEPTH_STENCIL_VIEW_DESC* desc)
{
    RETURN_NULL_IF_NULL(texture, "DepthStencilView::CreateFromTexture1D - texture is null");

    auto* device = GetD3D11Device();
    RETURN_NULL_IF_NULL(device, "DepthStencilView::CreateFromTexture1D - device is null");

    auto view = std::unique_ptr<DepthStencilView>(new DepthStencilView());
    HRESULT hr = device->CreateDepthStencilView(texture, desc, view->dsv_.GetAddressOf());
    RETURN_NULL_IF_FAILED(hr, "DepthStencilView::CreateFromTexture1D - CreateDepthStencilView failed");

    return view;
}

std::unique_ptr<DepthStencilView> DepthStencilView::CreateFromTexture2D(
    ID3D11Texture2D* texture,
    const D3D11_DEPTH_STENCIL_VIEW_DESC* desc)
{
    RETURN_NULL_IF_NULL(texture, "DepthStencilView::CreateFromTexture2D - texture is null");

    auto* device = GetD3D11Device();
    RETURN_NULL_IF_NULL(device, "DepthStencilView::CreateFromTexture2D - device is null");

    auto view = std::unique_ptr<DepthStencilView>(new DepthStencilView());
    HRESULT hr = device->CreateDepthStencilView(texture, desc, view->dsv_.GetAddressOf());
    RETURN_NULL_IF_FAILED(hr, "DepthStencilView::CreateFromTexture2D - CreateDepthStencilView failed");

    return view;
}

std::unique_ptr<DepthStencilView> DepthStencilView::Create(
    ID3D11Resource* resource,
    const D3D11_DEPTH_STENCIL_VIEW_DESC& desc)
{
    RETURN_NULL_IF_NULL(resource, "DepthStencilView::Create - resource is null");

    auto* device = GetD3D11Device();
    RETURN_NULL_IF_NULL(device, "DepthStencilView::Create - device is null");

    auto view = std::unique_ptr<DepthStencilView>(new DepthStencilView());
    HRESULT hr = device->CreateDepthStencilView(resource, &desc, view->dsv_.GetAddressOf());
    RETURN_NULL_IF_FAILED(hr, "DepthStencilView::Create - CreateDepthStencilView failed");

    return view;
}

std::unique_ptr<DepthStencilView> DepthStencilView::FromD3DView(
    ComPtr<ID3D11DepthStencilView> dsv)
{
    RETURN_NULL_IF_NULL(dsv.Get(), "DepthStencilView::FromD3DView - dsv is null");

    auto view = std::unique_ptr<DepthStencilView>(new DepthStencilView());
    view->dsv_ = std::move(dsv);
    return view;
}

D3D11_DEPTH_STENCIL_VIEW_DESC DepthStencilView::GetDesc() const noexcept
{
    D3D11_DEPTH_STENCIL_VIEW_DESC desc = {};
    if (dsv_) {
        dsv_->GetDesc(&desc);
    }
    return desc;
}
