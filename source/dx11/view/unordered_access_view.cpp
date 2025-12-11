//----------------------------------------------------------------------------
//! @file   unordered_access_view.cpp
//! @brief  アンオーダードアクセスビュー（UAV）ラッパークラス実装
//----------------------------------------------------------------------------
#include "unordered_access_view.h"
#include "dx11/graphics_device.h"
#include "common/logging/logging.h"

std::unique_ptr<UnorderedAccessView> UnorderedAccessView::CreateFromBuffer(
    ID3D11Buffer* buffer,
    const D3D11_UNORDERED_ACCESS_VIEW_DESC* desc)
{
    RETURN_NULL_IF_NULL(buffer, "UnorderedAccessView::CreateFromBuffer - buffer is null");

    auto* device = GetD3D11Device();
    RETURN_NULL_IF_NULL(device, "UnorderedAccessView::CreateFromBuffer - device is null");

    auto view = std::unique_ptr<UnorderedAccessView>(new UnorderedAccessView());
    HRESULT hr = device->CreateUnorderedAccessView(buffer, desc, view->uav_.GetAddressOf());
    RETURN_NULL_IF_FAILED(hr, "UnorderedAccessView::CreateFromBuffer - CreateUnorderedAccessView failed");

    return view;
}

std::unique_ptr<UnorderedAccessView> UnorderedAccessView::CreateFromTexture1D(
    ID3D11Texture1D* texture,
    const D3D11_UNORDERED_ACCESS_VIEW_DESC* desc)
{
    RETURN_NULL_IF_NULL(texture, "UnorderedAccessView::CreateFromTexture1D - texture is null");

    auto* device = GetD3D11Device();
    RETURN_NULL_IF_NULL(device, "UnorderedAccessView::CreateFromTexture1D - device is null");

    auto view = std::unique_ptr<UnorderedAccessView>(new UnorderedAccessView());
    HRESULT hr = device->CreateUnorderedAccessView(texture, desc, view->uav_.GetAddressOf());
    RETURN_NULL_IF_FAILED(hr, "UnorderedAccessView::CreateFromTexture1D - CreateUnorderedAccessView failed");

    return view;
}

std::unique_ptr<UnorderedAccessView> UnorderedAccessView::CreateFromTexture2D(
    ID3D11Texture2D* texture,
    const D3D11_UNORDERED_ACCESS_VIEW_DESC* desc)
{
    RETURN_NULL_IF_NULL(texture, "UnorderedAccessView::CreateFromTexture2D - texture is null");

    auto* device = GetD3D11Device();
    RETURN_NULL_IF_NULL(device, "UnorderedAccessView::CreateFromTexture2D - device is null");

    auto view = std::unique_ptr<UnorderedAccessView>(new UnorderedAccessView());
    HRESULT hr = device->CreateUnorderedAccessView(texture, desc, view->uav_.GetAddressOf());
    RETURN_NULL_IF_FAILED(hr, "UnorderedAccessView::CreateFromTexture2D - CreateUnorderedAccessView failed");

    return view;
}

std::unique_ptr<UnorderedAccessView> UnorderedAccessView::CreateFromTexture3D(
    ID3D11Texture3D* texture,
    const D3D11_UNORDERED_ACCESS_VIEW_DESC* desc)
{
    RETURN_NULL_IF_NULL(texture, "UnorderedAccessView::CreateFromTexture3D - texture is null");

    auto* device = GetD3D11Device();
    RETURN_NULL_IF_NULL(device, "UnorderedAccessView::CreateFromTexture3D - device is null");

    auto view = std::unique_ptr<UnorderedAccessView>(new UnorderedAccessView());
    HRESULT hr = device->CreateUnorderedAccessView(texture, desc, view->uav_.GetAddressOf());
    RETURN_NULL_IF_FAILED(hr, "UnorderedAccessView::CreateFromTexture3D - CreateUnorderedAccessView failed");

    return view;
}

std::unique_ptr<UnorderedAccessView> UnorderedAccessView::Create(
    ID3D11Resource* resource,
    const D3D11_UNORDERED_ACCESS_VIEW_DESC& desc)
{
    RETURN_NULL_IF_NULL(resource, "UnorderedAccessView::Create - resource is null");

    auto* device = GetD3D11Device();
    RETURN_NULL_IF_NULL(device, "UnorderedAccessView::Create - device is null");

    auto view = std::unique_ptr<UnorderedAccessView>(new UnorderedAccessView());
    HRESULT hr = device->CreateUnorderedAccessView(resource, &desc, view->uav_.GetAddressOf());
    RETURN_NULL_IF_FAILED(hr, "UnorderedAccessView::Create - CreateUnorderedAccessView failed");

    return view;
}

std::unique_ptr<UnorderedAccessView> UnorderedAccessView::FromD3DView(
    ComPtr<ID3D11UnorderedAccessView> uav)
{
    RETURN_NULL_IF_NULL(uav.Get(), "UnorderedAccessView::FromD3DView - uav is null");

    auto view = std::unique_ptr<UnorderedAccessView>(new UnorderedAccessView());
    view->uav_ = std::move(uav);
    return view;
}

D3D11_UNORDERED_ACCESS_VIEW_DESC UnorderedAccessView::GetDesc() const noexcept
{
    D3D11_UNORDERED_ACCESS_VIEW_DESC desc = {};
    if (uav_) {
        uav_->GetDesc(&desc);
    }
    return desc;
}
