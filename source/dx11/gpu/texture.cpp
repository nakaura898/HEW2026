//----------------------------------------------------------------------------
//! @file   texture.cpp
//! @brief  GPUテクスチャクラス実装
//----------------------------------------------------------------------------
#include "texture.h"
#include "dx11/gpu_common.h"
#include "dx11/graphics_device.h"
#include "dx11/view/view.h"
#include "common/logging/logging.h"
#include <string>

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

    // SRV作成（直接ComPtr生成）
    ComPtr<ID3D11ShaderResourceView> srv = ShaderResourceView::CreateViewFromTexture2D(texture.Get());
    RETURN_NULL_IF_NULL(srv.Get(), "[Texture] SRV作成失敗");

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

    // SRV作成（直接ComPtr生成）
    ComPtr<ID3D11ShaderResourceView> srv = ShaderResourceView::CreateViewFromTexture2D(texture.Get());
    RETURN_NULL_IF_NULL(srv.Get(), "[Texture] SRV作成失敗");

    // RTV作成（直接ComPtr生成）
    ComPtr<ID3D11RenderTargetView> rtv = RenderTargetView::CreateViewFromTexture2D(texture.Get());
    RETURN_NULL_IF_NULL(rtv.Get(), "[Texture] RTV作成失敗");

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

    ComPtr<ID3D11DepthStencilView> dsv = View<DSV>::Create(texture.Get(), &dsvDesc);
    RETURN_NULL_IF_NULL(dsv.Get(), "[Texture] DSV作成失敗");

    // SRV作成（オプション）
    ComPtr<ID3D11ShaderResourceView> srv;
    if (withSrv && srvFormat != DXGI_FORMAT_UNKNOWN) {
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = srvFormat;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MipLevels = 1;
        srvDesc.Texture2D.MostDetailedMip = 0;

        srv = View<SRV>::Create(texture.Get(), &srvDesc);
        RETURN_NULL_IF_NULL(srv.Get(), "[Texture] 深度SRV作成失敗");
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

    // SRV作成（直接ComPtr生成）
    ComPtr<ID3D11ShaderResourceView> srv = ShaderResourceView::CreateViewFromTexture2D(texture.Get());
    RETURN_NULL_IF_NULL(srv.Get(), "[Texture] SRV作成失敗");

    // UAV作成（直接ComPtr生成）
    ComPtr<ID3D11UnorderedAccessView> uav = UnorderedAccessView::CreateViewFromTexture2D(texture.Get());
    RETURN_NULL_IF_NULL(uav.Get(), "[Texture] UAV作成失敗");

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

    ComPtr<ID3D11ShaderResourceView> srv = View<SRV>::Create(texture.Get(), &srvDesc);
    RETURN_NULL_IF_NULL(srv.Get(), "[Texture] キューブマップSRV作成失敗");

    return std::make_shared<Texture>(
        std::move(texture), std::move(srv), nullptr, nullptr, nullptr, desc);
}

//----------------------------------------------------------------------------
// デストラクタ
//----------------------------------------------------------------------------
Texture::~Texture()
{
    LOG_INFO("[Texture] 解放開始: " + std::to_string(width_) + "x" + std::to_string(height_));

    // 明示的な解放順序：ビュー → リソース
    if (uav_) { LOG_INFO("  UAV解放"); uav_.Reset(); }
    if (dsv_) { LOG_INFO("  DSV解放"); dsv_.Reset(); }
    if (rtv_) { LOG_INFO("  RTV解放"); rtv_.Reset(); }
    if (srv_) { LOG_INFO("  SRV解放"); srv_.Reset(); }
    if (resource_) {
        // リソースの現在のRefcountを確認（AddRef→Releaseで取得）
        resource_->AddRef();
        ULONG refCount = resource_->Release();
        LOG_INFO("  Resource解放前Refcount: " + std::to_string(refCount));
        resource_.Reset();
    }

    LOG_INFO("[Texture] 解放完了");
}
