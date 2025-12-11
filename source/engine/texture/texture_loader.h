//----------------------------------------------------------------------------
//! @file   texture_loader.h
//! @brief  テクスチャローダー（インターフェース + 実装）
//----------------------------------------------------------------------------
#pragma once

#include "dx11/gpu_common.h"
#include <vector>
#include <wincodec.h>

//===========================================================================
//! 画像デコード結果（ファイルから読み込んだ生データ）
//===========================================================================
struct TextureData
{
    std::vector<uint8_t> pixels;      //!< ピクセルデータ
    uint32_t width = 0;               //!< 幅
    uint32_t height = 0;              //!< 高さ
    uint32_t mipLevels = 1;           //!< mipレベル数（DDSの場合）
    uint32_t arraySize = 1;           //!< 配列サイズ（キューブマップは6）
    DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM;
    bool isCubemap = false;           //!< キューブマップか

    //!< 各mip/array要素のサブリソースデータ
    std::vector<D3D11_SUBRESOURCE_DATA> subresources;
};

//===========================================================================
//! テクスチャローダーインターフェース
//===========================================================================
class ITextureLoader
{
public:
    virtual ~ITextureLoader() = default;

    //! ファイルデータからテクスチャデータをデコード
    [[nodiscard]] virtual bool Load(
        const void* data,
        size_t size,
        TextureData& outData) = 0;

    //! 対応拡張子かどうか
    [[nodiscard]] virtual bool SupportsExtension(const char* extension) const = 0;
};

//===========================================================================
//! WIC（Windows Imaging Component）を使用したローダー
//!
//! 対応フォーマット: PNG, JPEG, BMP, TIFF, GIF
//! 出力フォーマット: DXGI_FORMAT_R8G8B8A8_UNORM
//!
//! @note スレッドセーフ。各スレッドでthread_localなCOM初期化とファクトリを使用。
//===========================================================================
class WICTextureLoader : public ITextureLoader
{
public:
    WICTextureLoader() = default;
    ~WICTextureLoader() override = default;

    [[nodiscard]] bool Load(
        const void* data,
        size_t size,
        TextureData& outData) override;

    [[nodiscard]] bool SupportsExtension(const char* extension) const override;
};

//===========================================================================
//! DDSファイル専用ローダー（DirectXTex使用）
//!
//! 対応フォーマット: DDS（BC圧縮、キューブマップ、mipmaps）
//===========================================================================
class DDSTextureLoader : public ITextureLoader
{
public:
    DDSTextureLoader() = default;
    ~DDSTextureLoader() override = default;

    [[nodiscard]] bool Load(
        const void* data,
        size_t size,
        TextureData& outData) override;

    [[nodiscard]] bool SupportsExtension(const char* extension) const override;
};
