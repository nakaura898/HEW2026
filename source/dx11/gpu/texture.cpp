//----------------------------------------------------------------------------
//! @file   texture.cpp
//! @brief  GPUテクスチャクラス実装
//----------------------------------------------------------------------------
#include "texture.h"
#include "dx11/gpu_common.h"
#include "dx11/graphics_device.h"
#include "dx11/view/shader_resource_view.h"
#include "dx11/view/render_target_view.h"
#include "dx11/view/depth_stencil_view.h"
#include "dx11/view/unordered_access_view.h"
#include "common/logging/logging.h"

//----------------------------------------------------------------------------
// Texture静的ファクトリメソッド
//----------------------------------------------------------------------------

//! 2Dテクスチャを作成（SRV付き）
std::shared_ptr<Texture> Texture::Create2D(
    uint32_t width,
    uint32_t height,
    DXGI_FORMAT format,
    const void* initialData)
{
    auto* device = GetD3D11Device();
    RETURN_NULL_IF_NULL(device, "[Texture] D3D11Deviceがnullです");

    TextureDesc desc = TextureDesc::Tex2D(width, height, format);

    D3D11_TEXTURE2D_DESC d3dDesc = {};
    d3dDesc.Width = width;
    d3dDesc.Height = height;
    d3dDesc.MipLevels = 1;
    d3dDesc.ArraySize = 1;
    d3dDesc.Format = format;
    d3dDesc.SampleDesc.Count = 1;
    d3dDesc.SampleDesc.Quality = 0;
    d3dDesc.Usage = D3D11_USAGE_DEFAULT;
    d3dDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initData = {};
    D3D11_SUBRESOURCE_DATA* pInitData = nullptr;
    if (initialData) {
        initData.pSysMem = initialData;
        initData.SysMemPitch = width * TextureDesc::FormatSize(format);
        pInitData = &initData;
    }

    ComPtr<ID3D11Texture2D> texture;
    HRESULT hr = device->CreateTexture2D(&d3dDesc, pInitData, texture.GetAddressOf());
    RETURN_NULL_IF_FAILED(hr, "[Texture] 2Dテクスチャ作成失敗");

    // SRV作成
    auto srvWrapper = ShaderResourceView::CreateFromTexture2D(texture.Get());
    ComPtr<ID3D11ShaderResourceView> srv = srvWrapper->Detach();

    return std::make_shared<Texture>(
        std::move(texture), std::move(srv), nullptr, nullptr, nullptr, desc);
}

//! レンダーターゲットを作成（SRV+RTV付き）
std::shared_ptr<Texture> Texture::CreateRenderTarget(
    uint32_t width,
    uint32_t height,
    DXGI_FORMAT format)
{
    auto* device = GetD3D11Device();
    RETURN_NULL_IF_NULL(device, "[Texture] D3D11Deviceがnullです");

    TextureDesc desc = TextureDesc::RenderTarget(width, height, format);

    D3D11_TEXTURE2D_DESC d3dDesc = {};
    d3dDesc.Width = width;
    d3dDesc.Height = height;
    d3dDesc.MipLevels = 1;
    d3dDesc.ArraySize = 1;
    d3dDesc.Format = format;
    d3dDesc.SampleDesc.Count = 1;
    d3dDesc.SampleDesc.Quality = 0;
    d3dDesc.Usage = D3D11_USAGE_DEFAULT;
    d3dDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

    ComPtr<ID3D11Texture2D> texture;
    HRESULT hr = device->CreateTexture2D(&d3dDesc, nullptr, texture.GetAddressOf());
    RETURN_NULL_IF_FAILED(hr, "[Texture] レンダーターゲット作成失敗");

    // SRV作成
    auto srvWrapper = ShaderResourceView::CreateFromTexture2D(texture.Get());
    ComPtr<ID3D11ShaderResourceView> srv = srvWrapper->Detach();

    // RTV作成
    auto rtvWrapper = RenderTargetView::CreateFromTexture2D(texture.Get());
    ComPtr<ID3D11RenderTargetView> rtv = rtvWrapper->Detach();

    return std::make_shared<Texture>(
        std::move(texture), std::move(srv), std::move(rtv), nullptr, nullptr, desc);
}

//! 深度ステンシルを作成（DSV付き、SRVはオプション）
std::shared_ptr<Texture> Texture::CreateDepthStencil(
    uint32_t width,
    uint32_t height,
    DXGI_FORMAT format,
    bool withSrv)
{
    auto* device = GetD3D11Device();
    RETURN_NULL_IF_NULL(device, "[Texture] D3D11Deviceがnullです");

    TextureDesc desc = TextureDesc::DepthStencil(width, height, format);
    if (withSrv) {
        desc.bindFlags |= D3D11_BIND_SHADER_RESOURCE;
    }

    // SRV用にTypelessフォーマットを使用
    DXGI_FORMAT textureFormat = format;
    DXGI_FORMAT dsvFormat = format;
    DXGI_FORMAT srvFormat = DXGI_FORMAT_UNKNOWN;

    if (withSrv) {
        switch (format) {
        case DXGI_FORMAT_D16_UNORM:
            textureFormat = DXGI_FORMAT_R16_TYPELESS;
            dsvFormat = DXGI_FORMAT_D16_UNORM;
            srvFormat = DXGI_FORMAT_R16_UNORM;
            break;
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
            textureFormat = DXGI_FORMAT_R24G8_TYPELESS;
            dsvFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
            srvFormat = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
            break;
        case DXGI_FORMAT_D32_FLOAT:
            textureFormat = DXGI_FORMAT_R32_TYPELESS;
            dsvFormat = DXGI_FORMAT_D32_FLOAT;
            srvFormat = DXGI_FORMAT_R32_FLOAT;
            break;
        case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
            textureFormat = DXGI_FORMAT_R32G8X24_TYPELESS;
            dsvFormat = DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
            srvFormat = DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;
            break;
        default:
            break;
        }
    }

    D3D11_TEXTURE2D_DESC d3dDesc = {};
    d3dDesc.Width = width;
    d3dDesc.Height = height;
    d3dDesc.MipLevels = 1;
    d3dDesc.ArraySize = 1;
    d3dDesc.Format = textureFormat;
    d3dDesc.SampleDesc.Count = 1;
    d3dDesc.SampleDesc.Quality = 0;
    d3dDesc.Usage = D3D11_USAGE_DEFAULT;
    d3dDesc.BindFlags = desc.bindFlags;

    ComPtr<ID3D11Texture2D> texture;
    HRESULT hr = device->CreateTexture2D(&d3dDesc, nullptr, texture.GetAddressOf());
    RETURN_NULL_IF_FAILED(hr, "[Texture] 深度ステンシル作成失敗");

    // DSV作成
    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = dsvFormat;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice = 0;

    auto dsvWrapper = DepthStencilView::Create(texture.Get(), dsvDesc);
    ComPtr<ID3D11DepthStencilView> dsv = dsvWrapper->Detach();

    // SRV作成（オプション）
    ComPtr<ID3D11ShaderResourceView> srv;
    if (withSrv && srvFormat != DXGI_FORMAT_UNKNOWN) {
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = srvFormat;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.MostDetailedMip = 0;

        auto srvWrapper = ShaderResourceView::Create(texture.Get(), srvDesc);
        srv = srvWrapper->Detach();
    }

    return std::make_shared<Texture>(
        std::move(texture), std::move(srv), nullptr, std::move(dsv), nullptr, desc);
}

//! UAV対応テクスチャを作成（SRV+UAV付き）
std::shared_ptr<Texture> Texture::CreateUav(
    uint32_t width,
    uint32_t height,
    DXGI_FORMAT format)
{
    auto* device = GetD3D11Device();
    RETURN_NULL_IF_NULL(device, "[Texture] D3D11Deviceがnullです");

    TextureDesc desc = TextureDesc::Uav(width, height, format);

    D3D11_TEXTURE2D_DESC d3dDesc = {};
    d3dDesc.Width = width;
    d3dDesc.Height = height;
    d3dDesc.MipLevels = 1;
    d3dDesc.ArraySize = 1;
    d3dDesc.Format = format;
    d3dDesc.SampleDesc.Count = 1;
    d3dDesc.SampleDesc.Quality = 0;
    d3dDesc.Usage = D3D11_USAGE_DEFAULT;
    d3dDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;

    ComPtr<ID3D11Texture2D> texture;
    HRESULT hr = device->CreateTexture2D(&d3dDesc, nullptr, texture.GetAddressOf());
    RETURN_NULL_IF_FAILED(hr, "[Texture] UAVテクスチャ作成失敗");

    // SRV作成
    auto srvWrapper = ShaderResourceView::CreateFromTexture2D(texture.Get());
    ComPtr<ID3D11ShaderResourceView> srv = srvWrapper->Detach();

    // UAV作成
    auto uavWrapper = UnorderedAccessView::CreateFromTexture2D(texture.Get());
    ComPtr<ID3D11UnorderedAccessView> uav = uavWrapper->Detach();

    return std::make_shared<Texture>(
        std::move(texture), std::move(srv), nullptr, nullptr, std::move(uav), desc);
}

//! キューブマップを作成（SRV付き）
std::shared_ptr<Texture> Texture::CreateCube(
    uint32_t size,
    DXGI_FORMAT format)
{
    auto* device = GetD3D11Device();
    RETURN_NULL_IF_NULL(device, "[Texture] D3D11Deviceがnullです");

    TextureDesc desc = TextureDesc::Cube(size, format);

    D3D11_TEXTURE2D_DESC d3dDesc = {};
    d3dDesc.Width = size;
    d3dDesc.Height = size;
    d3dDesc.MipLevels = 1;
    d3dDesc.ArraySize = 6;
    d3dDesc.Format = format;
    d3dDesc.SampleDesc.Count = 1;
    d3dDesc.SampleDesc.Quality = 0;
    d3dDesc.Usage = D3D11_USAGE_DEFAULT;
    d3dDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    d3dDesc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

    ComPtr<ID3D11Texture2D> texture;
    HRESULT hr = device->CreateTexture2D(&d3dDesc, nullptr, texture.GetAddressOf());
    RETURN_NULL_IF_FAILED(hr, "[Texture] キューブマップ作成失敗");

    // SRV作成（キューブマップ用）
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.TextureCube.MipLevels = 1;
    srvDesc.TextureCube.MostDetailedMip = 0;

    auto srvWrapper = ShaderResourceView::Create(texture.Get(), srvDesc);
    ComPtr<ID3D11ShaderResourceView> srv = srvWrapper->Detach();

    return std::make_shared<Texture>(
        std::move(texture), std::move(srv), nullptr, nullptr, nullptr, desc);
}
