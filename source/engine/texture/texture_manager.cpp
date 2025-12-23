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
#include <DirectXTex.h>
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

TexturePtr TextureManager::CompressToBC1(Texture* source)
{
    if (!source) return nullptr;

    auto* device = GraphicsDevice::Get().Device();
    auto* context = GraphicsContext::Get().GetContext();
    if (!device || !context) return nullptr;

    uint32_t width = source->Width();
    uint32_t height = source->Height();

    // 1. ステージングテクスチャを作成（CPUで読み取り可能）
    D3D11_TEXTURE2D_DESC stagingDesc{};
    stagingDesc.Width = width;
    stagingDesc.Height = height;
    stagingDesc.MipLevels = 1;
    stagingDesc.ArraySize = 1;
    stagingDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    stagingDesc.SampleDesc.Count = 1;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    ComPtr<ID3D11Texture2D> stagingTexture;
    HRESULT hr = device->CreateTexture2D(&stagingDesc, nullptr, &stagingTexture);
    if (FAILED(hr)) {
        LOG_ERROR("[TextureManager] BC1圧縮: ステージングテクスチャの作成に失敗");
        return nullptr;
    }

    // 2. ソーステクスチャからステージングにコピー
    context->CopyResource(stagingTexture.Get(), source->Get());

    // 3. ステージングテクスチャをマップしてピクセルデータを読み取り
    D3D11_MAPPED_SUBRESOURCE mapped{};
    hr = context->Map(stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
        LOG_ERROR("[TextureManager] BC1圧縮: ステージングテクスチャのマップに失敗");
        return nullptr;
    }

    // 4. ScratchImageを作成
    DirectX::ScratchImage srcImage;
    hr = srcImage.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height, 1, 1);
    if (FAILED(hr)) {
        context->Unmap(stagingTexture.Get(), 0);
        LOG_ERROR("[TextureManager] BC1圧縮: ScratchImageの初期化に失敗");
        return nullptr;
    }

    // ピクセルデータをコピー（アルファを1.0に強制）
    const DirectX::Image* img = srcImage.GetImage(0, 0, 0);
    const uint8_t* srcPtr = static_cast<const uint8_t*>(mapped.pData);
    uint8_t* dstPtr = img->pixels;
    for (uint32_t y = 0; y < height; ++y) {
        const uint8_t* srcRow = srcPtr + y * mapped.RowPitch;
        uint8_t* dstRow = dstPtr + y * img->rowPitch;
        for (uint32_t x = 0; x < width; ++x) {
            dstRow[x * 4 + 0] = srcRow[x * 4 + 0];  // R
            dstRow[x * 4 + 1] = srcRow[x * 4 + 1];  // G
            dstRow[x * 4 + 2] = srcRow[x * 4 + 2];  // B
            dstRow[x * 4 + 3] = 255;                 // A = 1.0
        }
    }
    context->Unmap(stagingTexture.Get(), 0);

    // 5. BC1に圧縮（高速、8倍圧縮）
    DirectX::ScratchImage compressedImage;
    hr = DirectX::Compress(
        srcImage.GetImages(),
        srcImage.GetImageCount(),
        srcImage.GetMetadata(),
        DXGI_FORMAT_BC1_UNORM,
        DirectX::TEX_COMPRESS_DEFAULT,
        0.5f,
        compressedImage);
    if (FAILED(hr)) {
        LOG_ERROR("[TextureManager] BC1圧縮に失敗: HRESULT=" + std::to_string(hr));
        return nullptr;
    }

    // 6. 圧縮データからテクスチャを作成
    const DirectX::Image* compImg = compressedImage.GetImage(0, 0, 0);

    D3D11_TEXTURE2D_DESC texDesc{};
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_BC1_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_IMMUTABLE;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initData{};
    initData.pSysMem = compImg->pixels;
    initData.SysMemPitch = static_cast<UINT>(compImg->rowPitch);

    ComPtr<ID3D11Texture2D> compressedTexture;
    hr = device->CreateTexture2D(&texDesc, &initData, &compressedTexture);
    if (FAILED(hr)) {
        LOG_ERROR("[TextureManager] BC1圧縮: 圧縮テクスチャの作成に失敗");
        return nullptr;
    }

    // SRVを作成
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_BC1_UNORM;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    ComPtr<ID3D11ShaderResourceView> srv;
    hr = device->CreateShaderResourceView(compressedTexture.Get(), &srvDesc, &srv);
    if (FAILED(hr)) {
        LOG_ERROR("[TextureManager] BC1圧縮: SRVの作成に失敗");
        return nullptr;
    }

    // Textureオブジェクトを作成して返す
    TextureDesc desc{};
    desc.width = width;
    desc.height = height;
    desc.depth = 1;
    desc.format = DXGI_FORMAT_BC1_UNORM;
    desc.bindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.dimension = TextureDimension::Tex2D;

    auto result = std::make_shared<Texture>(
        compressedTexture,
        srv,
        ComPtr<ID3D11RenderTargetView>(nullptr),
        ComPtr<ID3D11DepthStencilView>(nullptr),
        ComPtr<ID3D11UnorderedAccessView>(nullptr),
        desc);

    LOG_INFO("[TextureManager] BC1圧縮完了: " + std::to_string(width) + "x" + std::to_string(height) +
             " (VRAM: " + std::to_string(width * height / 2 / 1024 / 1024) + "MB)");
    return result;
}

TexturePtr TextureManager::CompressToBC3(Texture* source)
{
    if (!source) return nullptr;

    auto* device = GraphicsDevice::Get().Device();
    auto* context = GraphicsContext::Get().GetContext();
    if (!device || !context) return nullptr;

    uint32_t width = source->Width();
    uint32_t height = source->Height();

    // 1. ステージングテクスチャを作成（CPUで読み取り可能）
    D3D11_TEXTURE2D_DESC stagingDesc{};
    stagingDesc.Width = width;
    stagingDesc.Height = height;
    stagingDesc.MipLevels = 1;
    stagingDesc.ArraySize = 1;
    stagingDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    stagingDesc.SampleDesc.Count = 1;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    ComPtr<ID3D11Texture2D> stagingTexture;
    HRESULT hr = device->CreateTexture2D(&stagingDesc, nullptr, &stagingTexture);
    if (FAILED(hr)) {
        LOG_ERROR("[TextureManager] BC3圧縮: ステージングテクスチャの作成に失敗");
        return nullptr;
    }

    // 2. ソーステクスチャからステージングにコピー
    context->CopyResource(stagingTexture.Get(), source->Get());

    // 3. ステージングテクスチャをマップしてピクセルデータを読み取り
    D3D11_MAPPED_SUBRESOURCE mapped{};
    hr = context->Map(stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
        LOG_ERROR("[TextureManager] BC3圧縮: ステージングテクスチャのマップに失敗");
        return nullptr;
    }

    // 4. ScratchImageを作成
    DirectX::ScratchImage srcImage;
    hr = srcImage.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height, 1, 1);
    if (FAILED(hr)) {
        context->Unmap(stagingTexture.Get(), 0);
        LOG_ERROR("[TextureManager] BC3圧縮: ScratchImageの初期化に失敗");
        return nullptr;
    }

    // ピクセルデータをコピー
    const DirectX::Image* img = srcImage.GetImage(0, 0, 0);
    const uint8_t* srcPtr = static_cast<const uint8_t*>(mapped.pData);
    uint8_t* dstPtr = img->pixels;

    // デバッグ: 中央付近のピクセル値を出力
    uint32_t midY = height / 2;
    uint32_t midX = width / 2;
    const uint8_t* debugPixel = srcPtr + midY * mapped.RowPitch + midX * 4;
    LOG_INFO("[BC3 Debug] Center pixel RGBA: " +
        std::to_string(debugPixel[0]) + ", " +
        std::to_string(debugPixel[1]) + ", " +
        std::to_string(debugPixel[2]) + ", " +
        std::to_string(debugPixel[3]));

    for (uint32_t y = 0; y < height; ++y) {
        const uint8_t* srcRow = srcPtr + y * mapped.RowPitch;
        uint8_t* dstRow = dstPtr + y * img->rowPitch;
        // そのままコピー
        std::memcpy(dstRow, srcRow, width * 4);
    }
    context->Unmap(stagingTexture.Get(), 0);

    // 5. BC3に圧縮（フルアルファ対応）
    // ソースがリニア色空間の場合はsRGBフラグなしで圧縮
    DirectX::ScratchImage compressedImage;
    hr = DirectX::Compress(
        srcImage.GetImages(),
        srcImage.GetImageCount(),
        srcImage.GetMetadata(),
        DXGI_FORMAT_BC3_UNORM,
        DirectX::TEX_COMPRESS_DEFAULT,
        0.5f,
        compressedImage);
    if (FAILED(hr)) {
        LOG_ERROR("[TextureManager] BC3圧縮に失敗: HRESULT=" + std::to_string(hr));
        return nullptr;
    }

    // 6. 圧縮データからテクスチャを作成
    const DirectX::Image* compImg = compressedImage.GetImage(0, 0, 0);

    D3D11_TEXTURE2D_DESC texDesc{};
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_BC3_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_IMMUTABLE;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initData{};
    initData.pSysMem = compImg->pixels;
    initData.SysMemPitch = static_cast<UINT>(compImg->rowPitch);

    ComPtr<ID3D11Texture2D> compressedTexture;
    hr = device->CreateTexture2D(&texDesc, &initData, &compressedTexture);
    if (FAILED(hr)) {
        LOG_ERROR("[TextureManager] BC3圧縮: 圧縮テクスチャの作成に失敗");
        return nullptr;
    }

    // SRVを作成
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_BC3_UNORM;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    ComPtr<ID3D11ShaderResourceView> srv;
    hr = device->CreateShaderResourceView(compressedTexture.Get(), &srvDesc, &srv);
    if (FAILED(hr)) {
        LOG_ERROR("[TextureManager] BC3圧縮: SRVの作成に失敗");
        return nullptr;
    }

    // Textureオブジェクトを作成して返す
    TextureDesc desc{};
    desc.width = width;
    desc.height = height;
    desc.depth = 1;
    desc.format = DXGI_FORMAT_BC3_UNORM;
    desc.bindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.dimension = TextureDimension::Tex2D;

    auto result = std::make_shared<Texture>(
        compressedTexture,
        srv,
        ComPtr<ID3D11RenderTargetView>(nullptr),
        ComPtr<ID3D11DepthStencilView>(nullptr),
        ComPtr<ID3D11UnorderedAccessView>(nullptr),
        desc);

    LOG_INFO("[TextureManager] BC3圧縮完了: " + std::to_string(width) + "x" + std::to_string(height) +
             " (VRAM: " + std::to_string(width * height / 1024 / 1024) + "MB)");
    return result;
}

TexturePtr TextureManager::CompressToBC7(Texture* source)
{
    if (!source) return nullptr;

    auto* device = GraphicsDevice::Get().Device();
    auto* context = GraphicsContext::Get().GetContext();
    if (!device || !context) return nullptr;

    uint32_t width = source->Width();
    uint32_t height = source->Height();

    // 1. ステージングテクスチャを作成（CPUで読み取り可能）
    D3D11_TEXTURE2D_DESC stagingDesc{};
    stagingDesc.Width = width;
    stagingDesc.Height = height;
    stagingDesc.MipLevels = 1;
    stagingDesc.ArraySize = 1;
    stagingDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    stagingDesc.SampleDesc.Count = 1;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    ComPtr<ID3D11Texture2D> stagingTexture;
    HRESULT hr = device->CreateTexture2D(&stagingDesc, nullptr, &stagingTexture);
    if (FAILED(hr)) {
        LOG_ERROR("[TextureManager] BC7圧縮: ステージングテクスチャの作成に失敗");
        return nullptr;
    }

    // 2. ソーステクスチャからステージングにコピー
    context->CopyResource(stagingTexture.Get(), source->Get());

    // 3. ステージングテクスチャをマップしてピクセルデータを読み取り
    D3D11_MAPPED_SUBRESOURCE mapped{};
    hr = context->Map(stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
        LOG_ERROR("[TextureManager] BC7圧縮: ステージングテクスチャのマップに失敗");
        return nullptr;
    }

    // 4. ScratchImageを作成
    DirectX::ScratchImage srcImage;
    hr = srcImage.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height, 1, 1);
    if (FAILED(hr)) {
        context->Unmap(stagingTexture.Get(), 0);
        LOG_ERROR("[TextureManager] BC7圧縮: ScratchImageの初期化に失敗");
        return nullptr;
    }

    // ピクセルデータをコピー
    const DirectX::Image* img = srcImage.GetImage(0, 0, 0);
    const uint8_t* srcPtr = static_cast<const uint8_t*>(mapped.pData);
    uint8_t* dstPtr = img->pixels;
    size_t rowSize = width * 4;
    for (uint32_t y = 0; y < height; ++y) {
        memcpy(dstPtr + y * img->rowPitch, srcPtr + y * mapped.RowPitch, rowSize);
    }
    context->Unmap(stagingTexture.Get(), 0);

    // 5. BC7に圧縮（高品質、アルファ対応）
    DirectX::ScratchImage compressedImage;
    hr = DirectX::Compress(
        srcImage.GetImages(),
        srcImage.GetImageCount(),
        srcImage.GetMetadata(),
        DXGI_FORMAT_BC7_UNORM,
        DirectX::TEX_COMPRESS_BC7_QUICK,  // 高速モード
        1.0f,
        compressedImage);
    if (FAILED(hr)) {
        LOG_ERROR("[TextureManager] BC7圧縮に失敗: HRESULT=" + std::to_string(hr));
        return nullptr;
    }

    // 6. 圧縮データからテクスチャを作成
    const DirectX::Image* compImg = compressedImage.GetImage(0, 0, 0);

    D3D11_TEXTURE2D_DESC texDesc{};
    texDesc.Width = width;
    texDesc.Height = height;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_BC7_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_IMMUTABLE;
    texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA initData{};
    initData.pSysMem = compImg->pixels;
    initData.SysMemPitch = static_cast<UINT>(compImg->rowPitch);

    ComPtr<ID3D11Texture2D> compressedTexture;
    hr = device->CreateTexture2D(&texDesc, &initData, &compressedTexture);
    if (FAILED(hr)) {
        LOG_ERROR("[TextureManager] BC7圧縮: 圧縮テクスチャの作成に失敗");
        return nullptr;
    }

    // SRVを作成
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_BC7_UNORM;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    ComPtr<ID3D11ShaderResourceView> srv;
    hr = device->CreateShaderResourceView(compressedTexture.Get(), &srvDesc, &srv);
    if (FAILED(hr)) {
        LOG_ERROR("[TextureManager] BC7圧縮: SRVの作成に失敗");
        return nullptr;
    }

    // Textureオブジェクトを作成して返す
    TextureDesc desc{};
    desc.width = width;
    desc.height = height;
    desc.depth = 1;
    desc.format = DXGI_FORMAT_BC7_UNORM;
    desc.bindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.dimension = TextureDimension::Tex2D;

    auto result = std::make_shared<Texture>(
        compressedTexture,
        srv,
        ComPtr<ID3D11RenderTargetView>(nullptr),
        ComPtr<ID3D11DepthStencilView>(nullptr),
        ComPtr<ID3D11UnorderedAccessView>(nullptr),
        desc);

    LOG_INFO("[TextureManager] BC7圧縮完了: " + std::to_string(width) + "x" + std::to_string(height) +
             " (VRAM: " + std::to_string(width * height / 1024 / 1024) + "MB)");
    return result;
}
