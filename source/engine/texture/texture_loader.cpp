//----------------------------------------------------------------------------
//! @file   texture_loader.cpp
//! @brief  テクスチャローダー実装（WIC + DDS）
//----------------------------------------------------------------------------
#include "texture_loader.h"
#include "common/logging/logging.h"
#include <DirectXTex.h>
#include <algorithm>

#pragma comment(lib, "windowscodecs.lib")

//============================================================================
// WICTextureLoader 実装
//============================================================================
namespace
{
    //! スレッドローカルなCOM/WICファクトリ管理
    struct ThreadLocalWIC
    {
        ComPtr<IWICImagingFactory> factory;
        bool comInitialized = false;
        bool initialized = false;

        ThreadLocalWIC() = default;

        ~ThreadLocalWIC()
        {
            factory.Reset();
            if (comInitialized) {
                CoUninitialize();
            }
        }

        bool Initialize()
        {
            if (initialized) {
                return factory != nullptr;
            }
            initialized = true;

            // COM初期化（スレッドごとに必要）
            HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
                LOG_ERROR("[WICTextureLoader] COM初期化に失敗");
                return false;
            }
            comInitialized = SUCCEEDED(hr);

            // ファクトリ作成
            hr = CoCreateInstance(
                CLSID_WICImagingFactory,
                nullptr,
                CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(&factory));

            if (FAILED(hr)) {
                LOG_ERROR("[WICTextureLoader] IWICImagingFactoryの作成に失敗");
                return false;
            }

            return true;
        }
    };

    thread_local ThreadLocalWIC t_wic;
}

//----------------------------------------------------------------------------
bool WICTextureLoader::Load(
    const void* data,
    size_t size,
    TextureData& outData)
{
    // スレッドローカルなファクトリを取得（遅延初期化）
    if (!t_wic.Initialize()) {
        return false;
    }
    auto* factory = t_wic.factory.Get();

    // メモリストリームを作成
    ComPtr<IWICStream> stream;
    HRESULT hr = factory->CreateStream(&stream);
    if (FAILED(hr)) return false;

    hr = stream->InitializeFromMemory(
        const_cast<BYTE*>(static_cast<const BYTE*>(data)),
        static_cast<DWORD>(size));
    if (FAILED(hr)) return false;

    // デコーダーを作成
    ComPtr<IWICBitmapDecoder> decoder;
    hr = factory->CreateDecoderFromStream(
        stream.Get(),
        nullptr,
        WICDecodeMetadataCacheOnDemand,
        &decoder);
    if (FAILED(hr)) return false;

    // フレームを取得
    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) return false;

    // サイズを取得
    UINT width, height;
    hr = frame->GetSize(&width, &height);
    if (FAILED(hr)) return false;

    // RGBA32に変換
    ComPtr<IWICFormatConverter> converter;
    hr = factory->CreateFormatConverter(&converter);
    if (FAILED(hr)) return false;

    hr = converter->Initialize(
        frame.Get(),
        GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone,
        nullptr,
        0.0,
        WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) return false;

    // ピクセルデータをコピー
    size_t rowPitch = width * 4;
    size_t imageSize = rowPitch * height;
    outData.pixels.resize(imageSize);

    hr = converter->CopyPixels(
        nullptr,
        static_cast<UINT>(rowPitch),
        static_cast<UINT>(imageSize),
        outData.pixels.data());
    if (FAILED(hr)) return false;

    outData.width = width;
    outData.height = height;
    outData.mipLevels = 1;
    outData.arraySize = 1;
    outData.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    outData.isCubemap = false;

    // サブリソースデータを設定
    outData.subresources.resize(1);
    outData.subresources[0].pSysMem = outData.pixels.data();
    outData.subresources[0].SysMemPitch = static_cast<UINT>(rowPitch);
    outData.subresources[0].SysMemSlicePitch = 0;

    return true;
}

//----------------------------------------------------------------------------
bool WICTextureLoader::SupportsExtension(const char* ext) const
{
    if (!ext) return false;
    std::string e = ext;
    std::transform(e.begin(), e.end(), e.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return e == ".png" || e == ".jpg" || e == ".jpeg" ||
           e == ".bmp" || e == ".tiff" || e == ".gif";
}

//============================================================================
// DDSTextureLoader 実装
//============================================================================
bool DDSTextureLoader::Load(
    const void* data,
    size_t size,
    TextureData& outData)
{
    DirectX::TexMetadata metadata;
    DirectX::ScratchImage scratchImage;

    HRESULT hr = DirectX::LoadFromDDSMemory(
        static_cast<const uint8_t*>(data),
        size,
        DirectX::DDS_FLAGS_NONE,
        &metadata,
        scratchImage);

    if (FAILED(hr)) {
        LOG_ERROR("[DDSTextureLoader] DDSファイルの読み込みに失敗");
        return false;
    }

    outData.width = static_cast<uint32_t>(metadata.width);
    outData.height = static_cast<uint32_t>(metadata.height);
    outData.mipLevels = static_cast<uint32_t>(metadata.mipLevels);
    outData.arraySize = static_cast<uint32_t>(metadata.arraySize);
    outData.format = metadata.format;
    outData.isCubemap = (metadata.miscFlags & DirectX::TEX_MISC_TEXTURECUBE) != 0;

    // ピクセルデータをコピー
    const DirectX::Image* images = scratchImage.GetImages();
    size_t imageCount = scratchImage.GetImageCount();

    // 全イメージの合計サイズを計算
    size_t totalSize = 0;
    for (size_t i = 0; i < imageCount; ++i) {
        totalSize += images[i].slicePitch;
    }
    outData.pixels.resize(totalSize);

    // サブリソースデータを設定
    outData.subresources.resize(imageCount);
    size_t offset = 0;
    for (size_t i = 0; i < imageCount; ++i) {
        std::memcpy(outData.pixels.data() + offset, images[i].pixels, images[i].slicePitch);
        outData.subresources[i].pSysMem = outData.pixels.data() + offset;
        outData.subresources[i].SysMemPitch = static_cast<UINT>(images[i].rowPitch);
        outData.subresources[i].SysMemSlicePitch = static_cast<UINT>(images[i].slicePitch);
        offset += images[i].slicePitch;
    }

    return true;
}

//----------------------------------------------------------------------------
bool DDSTextureLoader::SupportsExtension(const char* ext) const
{
    if (!ext) return false;
    std::string e = ext;
    std::transform(e.begin(), e.end(), e.begin(),
        [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return e == ".dds";
}
