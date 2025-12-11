//----------------------------------------------------------------------------
//! @file   render_target_view.cpp
//! @brief  レンダーターゲットビュー（RTV）ラッパークラス実装
//----------------------------------------------------------------------------
#include "render_target_view.h"
#include "dx11/graphics_device.h"
#include "common/logging/logging.h"

std::unique_ptr<RenderTargetView> RenderTargetView::CreateFromBuffer(
    ID3D11Buffer* buffer,
    const D3D11_RENDER_TARGET_VIEW_DESC* desc)
{
    RETURN_NULL_IF_NULL(buffer, "RenderTargetView::CreateFromBuffer - buffer is null");

    auto* device = GetD3D11Device();
    RETURN_NULL_IF_NULL(device, "RenderTargetView::CreateFromBuffer - device is null");

    auto view = std::unique_ptr<RenderTargetView>(new RenderTargetView());
    HRESULT hr = device->CreateRenderTargetView(buffer, desc, view->rtv_.GetAddressOf());
    RETURN_NULL_IF_FAILED(hr, "RenderTargetView::CreateFromBuffer - CreateRenderTargetView failed");

    return view;
}

std::unique_ptr<RenderTargetView> RenderTargetView::CreateFromTexture1D(
    ID3D11Texture1D* texture,
    const D3D11_RENDER_TARGET_VIEW_DESC* desc)
{
    RETURN_NULL_IF_NULL(texture, "RenderTargetView::CreateFromTexture1D - texture is null");

    auto* device = GetD3D11Device();
    RETURN_NULL_IF_NULL(device, "RenderTargetView::CreateFromTexture1D - device is null");

    auto view = std::unique_ptr<RenderTargetView>(new RenderTargetView());
    HRESULT hr = device->CreateRenderTargetView(texture, desc, view->rtv_.GetAddressOf());
    RETURN_NULL_IF_FAILED(hr, "RenderTargetView::CreateFromTexture1D - CreateRenderTargetView failed");

    return view;
}

std::unique_ptr<RenderTargetView> RenderTargetView::CreateFromTexture2D(
    ID3D11Texture2D* texture,
    const D3D11_RENDER_TARGET_VIEW_DESC* desc)
{
    RETURN_NULL_IF_NULL(texture, "RenderTargetView::CreateFromTexture2D - texture is null");

    auto* device = GetD3D11Device();
    RETURN_NULL_IF_NULL(device, "RenderTargetView::CreateFromTexture2D - device is null");

    auto view = std::unique_ptr<RenderTargetView>(new RenderTargetView());
    HRESULT hr = device->CreateRenderTargetView(texture, desc, view->rtv_.GetAddressOf());
    RETURN_NULL_IF_FAILED(hr, "RenderTargetView::CreateFromTexture2D - CreateRenderTargetView failed");

    return view;
}

std::unique_ptr<RenderTargetView> RenderTargetView::CreateFromTexture3D(
    ID3D11Texture3D* texture,
    const D3D11_RENDER_TARGET_VIEW_DESC* desc)
{
    RETURN_NULL_IF_NULL(texture, "RenderTargetView::CreateFromTexture3D - texture is null");

    auto* device = GetD3D11Device();
    RETURN_NULL_IF_NULL(device, "RenderTargetView::CreateFromTexture3D - device is null");

    auto view = std::unique_ptr<RenderTargetView>(new RenderTargetView());
    HRESULT hr = device->CreateRenderTargetView(texture, desc, view->rtv_.GetAddressOf());
    RETURN_NULL_IF_FAILED(hr, "RenderTargetView::CreateFromTexture3D - CreateRenderTargetView failed");

    return view;
}

std::unique_ptr<RenderTargetView> RenderTargetView::Create(
    ID3D11Resource* resource,
    const D3D11_RENDER_TARGET_VIEW_DESC& desc)
{
    RETURN_NULL_IF_NULL(resource, "RenderTargetView::Create - resource is null");

    auto* device = GetD3D11Device();
    RETURN_NULL_IF_NULL(device, "RenderTargetView::Create - device is null");

    auto view = std::unique_ptr<RenderTargetView>(new RenderTargetView());
    HRESULT hr = device->CreateRenderTargetView(resource, &desc, view->rtv_.GetAddressOf());
    RETURN_NULL_IF_FAILED(hr, "RenderTargetView::Create - CreateRenderTargetView failed");

    return view;
}

std::unique_ptr<RenderTargetView> RenderTargetView::FromD3DView(
    ComPtr<ID3D11RenderTargetView> rtv)
{
    RETURN_NULL_IF_NULL(rtv.Get(), "RenderTargetView::FromD3DView - rtv is null");

    auto view = std::unique_ptr<RenderTargetView>(new RenderTargetView());
    view->rtv_ = std::move(rtv);
    return view;
}

D3D11_RENDER_TARGET_VIEW_DESC RenderTargetView::GetDesc() const noexcept
{
    D3D11_RENDER_TARGET_VIEW_DESC desc = {};
    if (rtv_) {
        rtv_->GetDesc(&desc);
    }
    return desc;
}
