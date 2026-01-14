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
#include "dx11/view/view.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <algorithm>
#include <vector>

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

        View<SRV> srv;
        View<RTV> rtv;
        View<DSV> dsv;
        View<UAV> uav;

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

            srv = View<SRV>::Create(texture.Get(), &srvDesc);
            if (!srv.IsValid()) {
                LOG_ERROR("[TextureManager] SRV作成失敗");
            }
        }

        // RTV作成
        if (desc.BindFlags & D3D11_BIND_RENDER_TARGET) {
            rtv = View<RTV>::Create(texture.Get());
            if (!rtv.IsValid()) {
                LOG_ERROR("[TextureManager] RTV作成失敗");
            }
        }

        // DSV作成
        if (desc.BindFlags & D3D11_BIND_DEPTH_STENCIL) {
            dsv = View<DSV>::Create(texture.Get());
            if (!dsv.IsValid()) {
                LOG_ERROR("[TextureManager] DSV作成失敗");
            }
        }

        // UAV作成
        if (desc.BindFlags & D3D11_BIND_UNORDERED_ACCESS) {
            uav = View<UAV>::Create(texture.Get());
            if (!uav.IsValid()) {
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
void TextureManager::Create()
{
    if (!instance_) {
        instance_ = std::unique_ptr<TextureManager>(new TextureManager());
    }
}

void TextureManager::Destroy()
{
    instance_.reset();
}

TextureManager::~TextureManager() = default;

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
#ifdef _DEBUG
    // シャットダウン前にキャッシュ状態をログ出力（リーク検出用）
    if (cache_ && stats_.textureCount > 0) {
        LOG_WARN("[TextureManager] Shutdown with " + std::to_string(stats_.textureCount) +
                 " textures still cached. Ensure all TexturePtr are released before shutdown.");
    }
    // スロットストレージの状態もログ出力
    size_t activeSlots = 0;
    for (const auto& slot : slots_) {
        if (slot.inUse) activeSlots++;
    }
    if (activeSlots > 0) {
        LOG_INFO("[TextureManager] Releasing " + std::to_string(activeSlots) + " textures in slots");
    }
#endif

    // スコープをクリア
    scopes_.clear();
    currentScope_ = kGlobalScope;
    nextScopeId_ = 1;

    // スロットをクリア
    slots_.clear();
    std::queue<uint16_t>().swap(freeIndices_);
    handleCache_.clear();

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

//============================================================================
// スコープ管理
//============================================================================
TextureManager::ScopeId TextureManager::BeginScope()
{
    ScopeId id = nextScopeId_++;
    scopes_[id] = ScopeData{};
    currentScope_ = id;
    return id;
}

void TextureManager::EndScope(ScopeId scopeId)
{
    auto it = scopes_.find(scopeId);
    if (it == scopes_.end()) return;

    // このスコープの全テクスチャのrefcountを減らす
    for (TextureHandle handle : it->second.textures) {
        DecrementRefCount(handle);
    }
    scopes_.erase(it);

    // GC実行
    GarbageCollect();

    // 現在スコープをグローバルに戻す
    if (currentScope_ == scopeId) {
        currentScope_ = kGlobalScope;
    }
}

//============================================================================
// ハンドルベースAPI
//============================================================================
TextureHandle TextureManager::Load(const std::string& path, bool sRGB)
{
    return LoadInScope(path, sRGB, currentScope_);
}

TextureHandle TextureManager::LoadGlobal(const std::string& path, bool sRGB)
{
    return LoadInScope(path, sRGB, kGlobalScope);
}

Texture* TextureManager::Get(TextureHandle handle) const noexcept
{
    if (!handle.IsValid()) return nullptr;

    uint16_t index = handle.GetIndex();
    if (index >= slots_.size()) return nullptr;

    const TextureSlot& slot = slots_[index];
    if (!slot.inUse) return nullptr;

    // 世代番号チェック（古いハンドルの誤使用を検出）
    if (slot.generation != handle.GetGeneration()) {
        return nullptr;
    }

    return slot.texture.get();
}

void TextureManager::GarbageCollect()
{
    // 解放対象のスロットインデックスを収集
    std::vector<uint16_t> freedIndices;

    for (size_t i = 0; i < slots_.size(); ++i) {
        TextureSlot& slot = slots_[i];
        if (slot.inUse && slot.refCount == 0) {
            slot.texture.reset();
            slot.inUse = false;
            slot.generation++;  // 世代を進める
            freeIndices_.push(static_cast<uint16_t>(i));
            freedIndices.push_back(static_cast<uint16_t>(i));
        }
    }

    // handleCache_から解放されたスロットのエントリを削除
    if (!freedIndices.empty()) {
        for (auto it = handleCache_.begin(); it != handleCache_.end(); ) {
            uint16_t index = it->second.GetIndex();
            bool shouldErase = std::find(freedIndices.begin(), freedIndices.end(), index) != freedIndices.end();
            if (shouldErase) {
                it = handleCache_.erase(it);
            } else {
                ++it;
            }
        }
    }
}

//============================================================================
// 内部ヘルパー
//============================================================================
TextureHandle TextureManager::AllocateSlot(TexturePtr texture)
{
    uint16_t index;

    if (!freeIndices_.empty()) {
        // 空きスロットを再利用
        index = freeIndices_.front();
        freeIndices_.pop();
    } else {
        // 新しいスロットを追加
        if (slots_.size() >= 0xFFFF) {
            LOG_ERROR("[TextureManager] スロット上限（65535）に達しました");
            return TextureHandle::Invalid();
        }
        index = static_cast<uint16_t>(slots_.size());
        slots_.push_back(TextureSlot{});
    }

    TextureSlot& slot = slots_[index];
    slot.texture = std::move(texture);
    slot.refCount = 0;  // AddToScopeで増加される
    slot.inUse = true;
    // generationは保持（再利用時に既に++されている）

    return TextureHandle::Create(index, slot.generation);
}

void TextureManager::AddToScope(TextureHandle handle, ScopeId scope)
{
    IncrementRefCount(handle);

    if (scope == kGlobalScope) {
        // グローバルスコープは永続（refcount増やすがEndScopeされない）
        return;
    }

    auto it = scopes_.find(scope);
    if (it != scopes_.end()) {
        it->second.textures.push_back(handle);
    }
}

void TextureManager::IncrementRefCount(TextureHandle handle)
{
    if (!handle.IsValid()) return;

    uint16_t index = handle.GetIndex();
    if (index < slots_.size() && slots_[index].inUse) {
        slots_[index].refCount++;
    }
}

void TextureManager::DecrementRefCount(TextureHandle handle)
{
    if (!handle.IsValid()) return;

    uint16_t index = handle.GetIndex();
    if (index < slots_.size() && slots_[index].inUse && slots_[index].refCount > 0) {
        slots_[index].refCount--;
    }
}

TextureHandle TextureManager::LoadInScope(const std::string& path, bool sRGB, ScopeId scope)
{
    if (!initialized_) {
        LOG_ERROR("[TextureManager] 初期化されていません");
        return TextureHandle::Invalid();
    }

    uint64_t cacheKey = ComputeCacheKey(path, sRGB, false);

    // ハンドルキャッシュ検索
    auto it = handleCache_.find(cacheKey);
    if (it != handleCache_.end()) {
        TextureHandle cached = it->second;
        if (Get(cached)) {
            // 既存テクスチャをこのスコープに追加
            AddToScope(cached, scope);
            stats_.hitCount++;
            return cached;
        }
        // 無効なハンドル（GCで解放済み）→キャッシュから削除
        handleCache_.erase(it);
    }
    stats_.missCount++;

    // ファイル読み込み
    auto fileResult = fileSystem_->read(path);
    if (!fileResult.success || fileResult.bytes.empty()) {
        LOG_ERROR("[TextureManager] ファイルの読み込みに失敗: " + path);
        return TextureHandle::Invalid();
    }

    // ローダー選択
    ITextureLoader* loader = GetLoaderForExtension(path);
    if (!loader) {
        LOG_ERROR("[TextureManager] 対応するローダーがありません: " + path);
        return TextureHandle::Invalid();
    }

    // デコード
    TextureData texData;
    if (!loader->Load(fileResult.bytes.data(), fileResult.bytes.size(), texData)) {
        LOG_ERROR("[TextureManager] テクスチャのデコードに失敗: " + path);
        return TextureHandle::Invalid();
    }

    if (texData.isCubemap) {
        LOG_ERROR("[TextureManager] Loadでキューブマップをロードしようとしました: " + path);
        return TextureHandle::Invalid();
    }

    // フォーマット決定
    DXGI_FORMAT format = sRGB ? Format(texData.format).addSrgb() : Format(texData.format).removeSrgb();

    // D3D11 Desc作成
    D3D11_TEXTURE2D_DESC desc = {};
    desc.Width = texData.width;
    desc.Height = texData.height;
    desc.MipLevels = texData.mipLevels;
    desc.ArraySize = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;

    // TextureDesc作成
    TextureDesc mutraDesc;
    mutraDesc.width = texData.width;
    mutraDesc.height = texData.height;
    mutraDesc.depth = 1;
    mutraDesc.mipLevels = texData.mipLevels;
    mutraDesc.arraySize = 1;
    mutraDesc.format = format;
    mutraDesc.usage = D3D11_USAGE_DEFAULT;
    mutraDesc.bindFlags = D3D11_BIND_SHADER_RESOURCE;
    mutraDesc.cpuAccess = 0;
    mutraDesc.dimension = TextureDimension::Tex2D;

    // テクスチャ作成（ヘルパー関数を使用）
    TexturePtr texturePtr = CreateTextureWithViews(desc, texData.subresources.data(), mutraDesc);
    if (!texturePtr) {
        LOG_ERROR("[TextureManager] テクスチャ作成失敗: " + path);
        return TextureHandle::Invalid();
    }

    // スロットに割り当て
    TextureHandle handle = AllocateSlot(std::move(texturePtr));
    if (!handle.IsValid()) {
        return TextureHandle::Invalid();
    }

    // キャッシュに登録
    handleCache_[cacheKey] = handle;

    // スコープに追加
    AddToScope(handle, scope);

    return handle;
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
    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = format;
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Texture2D.MipSlice = 0;
    View<DSV> dsv = View<DSV>::Create(texture.Get(), &dsvDesc);
    if (!dsv.IsValid()) {
        LOG_ERROR("[TextureManager] DSV作成失敗");
    }

    // SRV作成（Viewクラスを使用）
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = TextureDesc::GetSrvFormat(format);
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = 1;
    View<SRV> srv = View<SRV>::Create(texture.Get(), &srvDesc);
    if (!srv.IsValid()) {
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
        std::move(texture), std::move(srv), View<RTV>{},
        std::move(dsv), View<UAV>{}, mutraDesc);
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

    View<SRV> srv = View<SRV>::Create(compressedTexture.Get(), &srvDesc);
    if (!srv.IsValid()) {
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
        std::move(srv),
        View<RTV>{},
        View<DSV>{},
        View<UAV>{},
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

    View<SRV> srv = View<SRV>::Create(compressedTexture.Get(), &srvDesc);
    if (!srv.IsValid()) {
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
        std::move(srv),
        View<RTV>{},
        View<DSV>{},
        View<UAV>{},
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

    View<SRV> srv = View<SRV>::Create(compressedTexture.Get(), &srvDesc);
    if (!srv.IsValid()) {
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
        std::move(srv),
        View<RTV>{},
        View<DSV>{},
        View<UAV>{},
        desc);

    LOG_INFO("[TextureManager] BC7圧縮完了: " + std::to_string(width) + "x" + std::to_string(height) +
             " (VRAM: " + std::to_string(width * height / 1024 / 1024) + "MB)");
    return result;
}
