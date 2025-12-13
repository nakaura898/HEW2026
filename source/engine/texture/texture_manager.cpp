//----------------------------------------------------------------------------
//! @file   texture_manager.cpp
//! @brief  テクスチャマネージャー実装
//----------------------------------------------------------------------------
#include "texture_manager.h"
#include "texture_cache.h"
#include "texture_loader.h"
#include "engine/fs/file_system.h"
#include "dx11/graphics_device.h"
#include "common/logging/logging.h"
#include "dx11/graphics_context.h"
#include "dx11/gpu/format.h"
#include "common/utility/hash.h"
#include "dx11/view/shader_resource_view.h"
#include "dx11/view/render_target_view.h"
#include "dx11/view/depth_stencil_view.h"
#include "dx11/view/unordered_access_view.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <algorithm>

namespace
{
    std::string GetFileExtension(const std::string& path)
    {
        size_t pos = path.rfind('.');
        if (pos == std::string::npos) return "";
        return path.substr(pos);
    }

    //! 2D/Cubeテクスチャとビューを作成
    [[nodiscard]] TexturePtr CreateTextureWithViews(
        const D3D11_TEXTURE2D_DESC& desc,
        const D3D11_SUBRESOURCE_DATA* initialData,
        const TextureDesc& mutraDesc)
    {
        ID3D11Device* device = GetD3D11Device();
        if (!device) return nullptr;

        ComPtr<ID3D11Texture2D> texture;
        HRESULT hr = device->CreateTexture2D(&desc, initialData, &texture);
        RETURN_NULL_IF_FAILED(hr, "[TextureManager] Texture2D作成失敗");

        ComPtr<ID3D11ShaderResourceView> srv;
        ComPtr<ID3D11RenderTargetView> rtv;
        ComPtr<ID3D11DepthStencilView> dsv;
        ComPtr<ID3D11UnorderedAccessView> uav;

        // SRV作成（Viewクラスを使用）
        if (desc.BindFlags & D3D11_BIND_SHADER_RESOURCE) {
            D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
            srvDesc.Format = TextureDesc::GetSrvFormat(desc.Format);

            if (mutraDesc.dimension == TextureDimension::Cube) {
                srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
                srvDesc.TextureCube.MostDetailedMip = 0;
                srvDesc.TextureCube.MipLevels = desc.MipLevels ? desc.MipLevels : static_cast<UINT>(-1);
            } else {
                srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
                srvDesc.Texture2D.MostDetailedMip = 0;
                srvDesc.Texture2D.MipLevels = desc.MipLevels ? desc.MipLevels : static_cast<UINT>(-1);
            }

            auto srvWrapper = ShaderResourceView::Create(texture.Get(), srvDesc);
            if (srvWrapper && srvWrapper->IsValid()) {
                srv = srvWrapper->Detach();
            } else {
                LOG_ERROR("[TextureManager] SRV作成失敗");
            }
        }

        // RTV作成（Viewクラスを使用）
        if (desc.BindFlags & D3D11_BIND_RENDER_TARGET) {
            auto rtvWrapper = RenderTargetView::CreateFromTexture2D(texture.Get(), nullptr);
            if (rtvWrapper && rtvWrapper->IsValid()) {
                rtv = rtvWrapper->Detach();
            } else {
                LOG_ERROR("[TextureManager] RTV作成失敗");
            }
        }

        // DSV作成（Viewクラスを使用）
        if (desc.BindFlags & D3D11_BIND_DEPTH_STENCIL) {
            auto dsvWrapper = DepthStencilView::CreateFromTexture2D(texture.Get(), nullptr);
            if (dsvWrapper && dsvWrapper->IsValid()) {
                dsv = dsvWrapper->Detach();
            } else {
                LOG_ERROR("[TextureManager] DSV作成失敗");
            }
        }

        // UAV作成（Viewクラスを使用）
        if (desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS) {
            auto uavWrapper = UnorderedAccessView::CreateFromTexture2D(texture.Get(), nullptr);
            if (uavWrapper && uavWrapper->IsValid()) {
                uav = uavWrapper->Detach();
            } else {
                LOG_ERROR("[TextureManager] UAV作成失敗");
            }
        }

        return std::make_shared<Texture>(
            std::move(texture), std::move(srv), std::move(rtv),
            std::move(dsv), std::move(uav), mutraDesc);
    }
} // namespace

//============================================================================
// TextureManager 実装
//============================================================================
TextureManager& TextureManager::Get() noexcept
{
    static TextureManager instance;
    return instance;
}

void TextureManager::Initialize(IReadableFileSystem* fileSystem)
{
    if (!GraphicsDevice::Get().IsValid()) {
        LOG_ERROR("[TextureManager] GraphicsDeviceが初期化されていません");
        return;
    }

    if (!fileSystem) {
        LOG_ERROR("[TextureManager] FileSystemがnullです");
        return;
    }

    initialized_ = true;
    fileSystem_ = fileSystem;
    ddsLoader_ = std::make_unique<DDSTextureLoader>();
    wicLoader_ = std::make_unique<WICTextureLoader>();
    cache_ = std::make_unique<WeakTextureCache>();
}

void TextureManager::Shutdown()
{
    if (cache_) {
        cache_->Clear();
    }
    cache_.reset();
    ddsLoader_.reset();
    wicLoader_.reset();
    fileSystem_ = nullptr;
    initialized_ = false;
    stats_ = {};
}

TexturePtr TextureManager::LoadTexture2D(
    const std::string& path,
    bool sRGB,
    bool generateMips)
{
    if (!initialized_) {
        LOG_ERROR("[TextureManager] 初期化されていません");
        return nullptr;
    }

    uint64_t cacheKey = ComputeCacheKey(path, sRGB, generateMips);

    // キャッシュ検索
    if (cache_) {
        auto cached = cache_->Get(cacheKey);
        if (cached) {
            stats_.hitCount++;
            return cached;
        }
    }
    stats_.missCount++;

    // ファイル読み込み
    auto fileResult = fileSystem_->read(path);
    if (!fileResult.success || fileResult.bytes.empty()) {
        LOG_ERROR("[TextureManager] ファイルの読み込みに失敗: " + path);
        return nullptr;
    }

    // ローダー選択
    ITextureLoader* loader = GetLoaderForExtension(path);
    if (!loader) {
        LOG_ERROR("[TextureManager] 対応するローダーがありません: " + GetFileExtension(path));
        return nullptr;
    }

    // デコード
    TextureData texData;
    if (!loader->Load(fileResult.bytes.data(), fileResult.bytes.size(), texData)) {
        LOG_ERROR("[TextureManager] テクスチャのデコードに失敗: " + path);
        return nullptr;
    }

    if (texData.isCubemap) {
        LOG_ERROR("[TextureManager] LoadTexture2Dでキューブマップを読み込もうとしました: " + path);
        return nullptr;
    }

    // フォーマット決定
    DXGI_FORMAT format = sRGB ? Format(texData.format).addSrgb() : Format(texData.format).removeSrgb();

    // BindFlags決定
    UINT bindFlags = D3D11_BIND_SHADER_RESOURCE;
    UINT mipLevels = texData.mipLevels;
    UINT miscFlags = 0;

    if (generateMips && texData.mipLevels == 1) {
        bindFlags |= D3D11_BIND_RENDER_TARGET;
        mipLevels = 0;
        miscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;
    }

    // D3D11 Desc作成
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = texData.width;
    desc.Height = texData.height;
    desc.MipLevels = mipLevels;
    desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = bindFlags;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = miscFlags;

    // TextureDesc作成
    TextureDesc mutraDesc;
    mutraDesc.width = texData.width;
    mutraDesc.height = texData.height;
    mutraDesc.depth = 1;
    mutraDesc.mipLevels = mipLevels;
    mutraDesc.arraySize = 1;
    mutraDesc.format = format;
    mutraDesc.usage = D3D11_USAGE_DEFAULT;
    mutraDesc.bindFlags = bindFlags;
    mutraDesc.cpuAccess = 0;
    mutraDesc.dimension = TextureDimension::Tex2D;

    TexturePtr texture;

    if (generateMips && texData.mipLevels == 1) {
        // mipmap生成が必要な場合
        texture = CreateTextureWithViews(desc, nullptr, mutraDesc);
        if (texture) {
            auto* context = GraphicsContext::Get().GetContext();
            auto* tex2D = texture->As<ID3D11Texture2D>();
            if (tex2D) {
                context->UpdateSubresource(
                    tex2D, 0, nullptr,
                    texData.subresources[0].pSysMem,
                    texData.subresources[0].SysMemPitch, 0);

                if (texture->HasSrv()) {
                    context->GenerateMips(texture->Srv());
                }
            }
        }
    } else {
        texture = CreateTextureWithViews(desc, texData.subresources.data(), mutraDesc);
    }

    if (!texture) {
        LOG_ERROR("[TextureManager] テクスチャの作成に失敗: " + path);
        return nullptr;
    }

    // キャッシュ登録
    if (cache_) {
        cache_->Put(cacheKey, texture);
    }

    return texture;
}

TexturePtr TextureManager::LoadTextureCube(
    const std::string& path,
    bool sRGB,
    bool generateMips)
{
    if (!initialized_) {
        LOG_ERROR("[TextureManager] 初期化されていません");
        return nullptr;
    }

    uint64_t cacheKey = ComputeCacheKey(path, sRGB, generateMips);

    // キャッシュ検索
    if (cache_) {
        auto cached = cache_->Get(cacheKey);
        if (cached) {
            stats_.hitCount++;
            return cached;
        }
    }
    stats_.missCount++;

    // ファイル読み込み
    auto fileResult = fileSystem_->read(path);
    if (!fileResult.success || fileResult.bytes.empty()) {
        LOG_ERROR("[TextureManager] ファイルの読み込みに失敗: " + path);
        return nullptr;
    }

    if (!ddsLoader_) {
        LOG_ERROR("[TextureManager] DDSローダーがありません");
        return nullptr;
    }

    // デコード
    TextureData texData;
    if (!ddsLoader_->Load(fileResult.bytes.data(), fileResult.bytes.size(), texData)) {
        LOG_ERROR("[TextureManager] DDSのデコードに失敗: " + path);
        return nullptr;
    }

    if (!texData.isCubemap || texData.arraySize != 6) {
        LOG_ERROR("[TextureManager] ファイルはキューブマップではありません: " + path);
        return nullptr;
    }

    if (generateMips && texData.mipLevels == 1) {
        LOG_WARN("[TextureManager] キューブマップのランタイムミップ生成は未サポートです: " + path);
    }

    // フォーマット決定
    DXGI_FORMAT format = sRGB ? Format(texData.format).addSrgb() : Format(texData.format).removeSrgb();

    // D3D11 Desc作成
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = texData.width;
    desc.Height = texData.width;  // キューブマップは正方形
    desc.MipLevels = texData.mipLevels;
    desc.ArraySize = 6;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

    // TextureDesc作成
    TextureDesc mutraDesc;
    mutraDesc.width = texData.width;
    mutraDesc.height = texData.width;
    mutraDesc.depth = 1;
    mutraDesc.mipLevels = texData.mipLevels;
    mutraDesc.arraySize = 6;
    mutraDesc.format = format;
    mutraDesc.usage = D3D11_USAGE_DEFAULT;
    mutraDesc.bindFlags = D3D11_BIND_SHADER_RESOURCE;
    mutraDesc.cpuAccess = 0;
    mutraDesc.dimension = TextureDimension::Cube;

    auto texture = CreateTextureWithViews(desc, texData.subresources.data(), mutraDesc);

    if (!texture) {
        LOG_ERROR("[TextureManager] キューブマップの作成に失敗: " + path);
        return nullptr;
    }

    // キャッシュ登録
    if (cache_) {
        cache_->Put(cacheKey, texture);
    }

    return texture;
}

TexturePtr TextureManager::Create2D(
    uint32_t width,
    uint32_t height,
    DXGI_FORMAT format,
    uint32_t bindFlags,
    const void* initialData,
    uint32_t rowPitch)
{
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = bindFlags;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;

    D3D11_SUBRESOURCE_DATA subresource = {};
    D3D11_SUBRESOURCE_DATA* pSubresource = nullptr;
    if (initialData) {
        subresource.pSysMem = initialData;
        subresource.SysMemPitch = rowPitch ? rowPitch : (width * TextureDesc::FormatSize(format));
        subresource.SysMemSlicePitch = 0;
        pSubresource = &subresource;
    }

    TextureDesc mutraDesc;
    mutraDesc.width = width;
    mutraDesc.height = height;
    mutraDesc.depth = 1;
    mutraDesc.mipLevels = 1;
    mutraDesc.arraySize = 1;
    mutraDesc.format = format;
    mutraDesc.usage = D3D11_USAGE_DEFAULT;
    mutraDesc.bindFlags = bindFlags;
    mutraDesc.cpuAccess = 0;
    mutraDesc.dimension = TextureDimension::Tex2D;

    return CreateTextureWithViews(desc, pSubresource, mutraDesc);
}

TexturePtr TextureManager::CreateRenderTarget(
    uint32_t width,
    uint32_t height,
    DXGI_FORMAT format)
{
    return Create2D(width, height, format,
        D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);
}

TexturePtr TextureManager::CreateDepthStencil(
    uint32_t width,
    uint32_t height,
    DXGI_FORMAT format)
{
    // 深度用のTypelessフォーマットに変換
    DXGI_FORMAT texFormat = format;
    switch (format) {
    case DXGI_FORMAT_D24_UNORM_S8_UINT:
        texFormat = DXGI_FORMAT_R24G8_TYPELESS;
        break;
    case DXGI_FORMAT_D32_FLOAT:
        texFormat = DXGI_FORMAT_R32_TYPELESS;
        break;
    case DXGI_FORMAT_D16_UNORM:
        texFormat = DXGI_FORMAT_R16_TYPELESS;
        break;
    default:
        break;
    }

    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = texFormat;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;

    ID3D11Device* device = GetD3D11Device();
    if (!device) return nullptr;

    ComPtr<ID3D11Texture2D> texture;
    HRESULT hr = device->CreateTexture2D(&desc, nullptr, &texture);
    RETURN_NULL_IF_FAILED(hr, "[TextureManager] DepthStencil Texture2D作成失敗");

    // DSV作成（Viewクラスを使用、元のフォーマットで）
    ComPtr<ID3D11DepthStencilView> dsv;
    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = format;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice = 0;
    auto dsvWrapper = DepthStencilView::Create(texture.Get(), dsvDesc);
    if (dsvWrapper && dsvWrapper->IsValid()) {
        dsv = dsvWrapper->Detach();
    } else {
        LOG_ERROR("[TextureManager] DSV作成失敗");
    }

    // SRV作成（Viewクラスを使用）
    ComPtr<ID3D11ShaderResourceView> srv;
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = TextureDesc::GetSrvFormat(format);
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;
    auto srvWrapper = ShaderResourceView::Create(texture.Get(), srvDesc);
    if (srvWrapper && srvWrapper->IsValid()) {
        srv = srvWrapper->Detach();
    } else {
        LOG_ERROR("[TextureManager] SRV作成失敗");
    }

    TextureDesc mutraDesc;
    mutraDesc.width = width;
    mutraDesc.height = height;
    mutraDesc.depth = 1;
    mutraDesc.mipLevels = 1;
    mutraDesc.arraySize = 1;
    mutraDesc.format = format;
    mutraDesc.usage = D3D11_USAGE_DEFAULT;
    mutraDesc.bindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    mutraDesc.cpuAccess = 0;
    mutraDesc.dimension = TextureDimension::Tex2D;

    return std::make_shared<Texture>(
        std::move(texture), std::move(srv), nullptr,
        std::move(dsv), nullptr, mutraDesc);
}

void TextureManager::ClearCache()
{
    if (cache_) {
        cache_->Clear();
    }
    stats_.hitCount = 0;
    stats_.missCount = 0;
}

TextureCacheStats TextureManager::GetCacheStats() const
{
    if (cache_) {
        stats_.textureCount = cache_->Count();
        stats_.totalMemoryBytes = cache_->MemoryUsage();
    }
    return stats_;
}

ITextureLoader* TextureManager::GetLoaderForExtension(const std::string& path) const
{
    std::string ext = GetFileExtension(path);
    if (ddsLoader_ && ddsLoader_->SupportsExtension(ext.c_str())) {
        return ddsLoader_.get();
    }
    if (wicLoader_ && wicLoader_->SupportsExtension(ext.c_str())) {
        return wicLoader_.get();
    }
    return nullptr;
}

uint64_t TextureManager::ComputeCacheKey(
    const std::string& path,
    bool sRGB,
    bool generateMips) const
{
    uint64_t hash = HashUtil::Fnv1aString(path);
    uint8_t flags = (sRGB ? 1 : 0) | (generateMips ? 2 : 0);
    hash = HashUtil::Fnv1a(&flags, sizeof(flags), hash);
    return hash;
}
