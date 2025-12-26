//----------------------------------------------------------------------------
//! @file   texture.h
//! @brief  GPUテクスチャクラス（統一設計）
//----------------------------------------------------------------------------
#pragma once

#include "dx11/gpu_common.h"
#include "gpu_resource.h"
#include "dx11/gpu/format.h"

//! テクスチャ次元
enum class TextureDimension : uint32_t {
    Tex1D = 0,
    Tex2D = 1,
    Tex3D = 2,
    Cube = 3,
};

//! キューブマップの面
enum class CubeFace : uint32_t {
    PositiveX = 0,
    NegativeX = 1,
    PositiveY = 2,
    NegativeY = 3,
    PositiveZ = 4,
    NegativeZ = 5,
    Count = 6
};

//===========================================================================
//! @brief テクスチャ記述子
//! @note  Layout (48 bytes)
//===========================================================================
struct TextureDesc {
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint32_t mipLevels;
    uint32_t arraySize;
    DXGI_FORMAT format;
    D3D11_USAGE usage;
    uint32_t bindFlags;
    uint32_t cpuAccess;
    uint32_t sampleCount;
    uint32_t sampleQuality;
    TextureDimension dimension;

    TextureDesc()
        : width(0), height(0), depth(1)
        , mipLevels(1), arraySize(1)
        , format(DXGI_FORMAT_R8G8B8A8_UNORM)
        , usage(D3D11_USAGE_DEFAULT)
        , bindFlags(D3D11_BIND_SHADER_RESOURCE)
        , cpuAccess(0)
        , sampleCount(1), sampleQuality(0)
        , dimension(TextureDimension::Tex2D) {}

    //! 1Dテクスチャ記述子を作成
    [[nodiscard]] static TextureDesc Tex1D(uint32_t w,
                                           DXGI_FORMAT fmt = DXGI_FORMAT_R8G8B8A8_UNORM) {
        TextureDesc d;
        d.width = w;
        d.height = 1;
        d.depth = 1;
        d.format = fmt;
        d.dimension = TextureDimension::Tex1D;
        return d;
    }

    //! 2Dテクスチャ記述子を作成
    [[nodiscard]] static TextureDesc Tex2D(uint32_t w, uint32_t h,
                                           DXGI_FORMAT fmt = DXGI_FORMAT_R8G8B8A8_UNORM) {
        TextureDesc d;
        d.width = w;
        d.height = h;
        d.format = fmt;
        d.dimension = TextureDimension::Tex2D;
        return d;
    }

    //! 3Dテクスチャ記述子を作成
    [[nodiscard]] static TextureDesc Tex3D(uint32_t w, uint32_t h, uint32_t dep,
                                           DXGI_FORMAT fmt = DXGI_FORMAT_R8G8B8A8_UNORM) {
        TextureDesc d;
        d.width = w;
        d.height = h;
        d.depth = dep;
        d.format = fmt;
        d.dimension = TextureDimension::Tex3D;
        return d;
    }

    //! レンダーターゲット記述子を作成
    [[nodiscard]] static TextureDesc RenderTarget(uint32_t w, uint32_t h,
                                                  DXGI_FORMAT fmt = DXGI_FORMAT_R8G8B8A8_UNORM) {
        TextureDesc d;
        d.width = w;
        d.height = h;
        d.format = fmt;
        d.bindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        d.dimension = TextureDimension::Tex2D;
        return d;
    }

    //! 深度ステンシル記述子を作成
    [[nodiscard]] static TextureDesc DepthStencil(uint32_t w, uint32_t h,
                                                  DXGI_FORMAT fmt = DXGI_FORMAT_D24_UNORM_S8_UINT) {
        TextureDesc d;
        d.width = w;
        d.height = h;
        d.format = fmt;
        d.bindFlags = D3D11_BIND_DEPTH_STENCIL;
        d.dimension = TextureDimension::Tex2D;
        return d;
    }

    //! UAV対応テクスチャ記述子を作成
    [[nodiscard]] static TextureDesc Uav(uint32_t w, uint32_t h,
                                         DXGI_FORMAT fmt = DXGI_FORMAT_R8G8B8A8_UNORM) {
        TextureDesc d;
        d.width = w;
        d.height = h;
        d.format = fmt;
        d.bindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS;
        d.dimension = TextureDimension::Tex2D;
        return d;
    }

    //! キューブマップ記述子を作成
    [[nodiscard]] static TextureDesc Cube(uint32_t size,
                                          DXGI_FORMAT fmt = DXGI_FORMAT_R8G8B8A8_UNORM) {
        TextureDesc d;
        d.width = size;
        d.height = size;
        d.depth = 1;
        d.arraySize = 6;
        d.format = fmt;
        d.bindFlags = D3D11_BIND_SHADER_RESOURCE;
        d.dimension = TextureDimension::Cube;
        return d;
    }

    //! キューブマップレンダーターゲット記述子を作成
    [[nodiscard]] static TextureDesc CubeRenderTarget(uint32_t size,
                                                      DXGI_FORMAT fmt = DXGI_FORMAT_R8G8B8A8_UNORM) {
        TextureDesc d;
        d.width = size;
        d.height = size;
        d.depth = 1;
        d.arraySize = 6;
        d.format = fmt;
        d.bindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        d.dimension = TextureDimension::Cube;
        return d;
    }

    //! ハッシュキーを計算
    [[nodiscard]] uint64_t HashKey() const noexcept {
        return (static_cast<uint64_t>(width) << 48) |
               (static_cast<uint64_t>(height) << 32) |
               (static_cast<uint64_t>(format) << 16) |
               (static_cast<uint64_t>(bindFlags) << 4) |
               static_cast<uint64_t>(dimension);
    }

    //! フォーマットのピクセルサイズを取得（バイト単位）
    [[nodiscard]] static uint32_t FormatSize(DXGI_FORMAT fmt) noexcept {
        int32_t bits = Format(fmt).bpp();
        // BC圧縮フォーマット等の特殊ケース対応
        return bits > 0 ? static_cast<uint32_t>((bits + 7) / 8) : 4;
    }

    //! 深度フォーマットに対応するSRVフォーマットを取得
    [[nodiscard]] static DXGI_FORMAT GetSrvFormat(DXGI_FORMAT fmt) noexcept {
        return Format(fmt).toColor();
    }
};

static_assert(sizeof(TextureDesc) == 48, "TextureDesc must be 48 bytes");

//===========================================================================
//! @brief GPUテクスチャクラス（統一設計）
//! @note  Layout (64 bytes)
//===========================================================================
class Texture : private NonCopyable {
public:
    //----------------------------------------------------------
    //! @name   ファクトリメソッド
    //----------------------------------------------------------
    //!@{

    //! 2Dテクスチャを作成（SRV付き）
    [[nodiscard]] static std::shared_ptr<Texture> Create2D(
        uint32_t width,
        uint32_t height,
        DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM,
        const void* initialData = nullptr);

    //! レンダーターゲットを作成（SRV+RTV付き）
    [[nodiscard]] static std::shared_ptr<Texture> CreateRenderTarget(
        uint32_t width,
        uint32_t height,
        DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM);

    //! 深度ステンシルを作成（DSV付き、SRVはオプション）
    [[nodiscard]] static std::shared_ptr<Texture> CreateDepthStencil(
        uint32_t width,
        uint32_t height,
        DXGI_FORMAT format = DXGI_FORMAT_D24_UNORM_S8_UINT,
        bool withSrv = false);

    //! UAV対応テクスチャを作成（SRV+UAV付き）
    [[nodiscard]] static std::shared_ptr<Texture> CreateUav(
        uint32_t width,
        uint32_t height,
        DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM);

    //! キューブマップを作成（SRV付き）
    [[nodiscard]] static std::shared_ptr<Texture> CreateCube(
        uint32_t size,
        DXGI_FORMAT format = DXGI_FORMAT_R8G8B8A8_UNORM);

    //!@}
    //----------------------------------------------------------
    //! @name   コンストラクタ
    //----------------------------------------------------------
    //!@{

    //! 1Dテクスチャ用コンストラクタ
    Texture(ComPtr<ID3D11Texture1D> texture,
            ComPtr<ID3D11ShaderResourceView> srv,
            ComPtr<ID3D11RenderTargetView> rtv,
            ComPtr<ID3D11UnorderedAccessView> uav,
            const TextureDesc& desc)
        : srv_(std::move(srv))
        , rtv_(std::move(rtv))
        , dsv_(nullptr)
        , uav_(std::move(uav))
        , width_(desc.width)
        , height_(1)
        , depth_(1)
        , format_(desc.format)
        , bindFlags_(desc.bindFlags)
        , dimension_(TextureDimension::Tex1D) {
        resource_.Attach(texture.Detach());
    }

    //! 2Dテクスチャ/キューブマップ用コンストラクタ
    Texture(ComPtr<ID3D11Texture2D> texture,
            ComPtr<ID3D11ShaderResourceView> srv,
            ComPtr<ID3D11RenderTargetView> rtv,
            ComPtr<ID3D11DepthStencilView> dsv,
            ComPtr<ID3D11UnorderedAccessView> uav,
            const TextureDesc& desc)
        : srv_(std::move(srv))
        , rtv_(std::move(rtv))
        , dsv_(std::move(dsv))
        , uav_(std::move(uav))
        , width_(desc.width)
        , height_(desc.height)
        , depth_(desc.depth)
        , format_(desc.format)
        , bindFlags_(desc.bindFlags)
        , dimension_(desc.dimension) {
        resource_.Attach(texture.Detach());
    }

    //! 3Dテクスチャ用コンストラクタ
    Texture(ComPtr<ID3D11Texture3D> texture,
            ComPtr<ID3D11ShaderResourceView> srv,
            ComPtr<ID3D11UnorderedAccessView> uav,
            const TextureDesc& desc)
        : srv_(std::move(srv))
        , rtv_(nullptr)
        , dsv_(nullptr)
        , uav_(std::move(uav))
        , width_(desc.width)
        , height_(desc.height)
        , depth_(desc.depth)
        , format_(desc.format)
        , bindFlags_(desc.bindFlags)
        , dimension_(TextureDimension::Tex3D) {
        resource_.Attach(texture.Detach());
    }

    //! デストラクタ
    ~Texture();

    //!@}

    //! GPUメモリサイズを取得
    [[nodiscard]] size_t GpuSize() const noexcept {
        return width_ * height_ * depth_ * TextureDesc::FormatSize(format_);
    }

    //! D3D11リソースを取得
    [[nodiscard]] ID3D11Resource* Get() const noexcept { return resource_.Get(); }

    //! 指定型にキャスト
    template<typename T>
    [[nodiscard]] T* As() const noexcept {
        T* ptr = nullptr;
        if (resource_) {
            resource_->QueryInterface(__uuidof(T), reinterpret_cast<void**>(&ptr));
        }
        return ptr;
    }

    //! SRVを取得
    [[nodiscard]] ID3D11ShaderResourceView* Srv() const noexcept { return srv_.Get(); }
    //! RTVを取得
    [[nodiscard]] ID3D11RenderTargetView* Rtv() const noexcept { return rtv_.Get(); }
    //! DSVを取得
    [[nodiscard]] ID3D11DepthStencilView* Dsv() const noexcept { return dsv_.Get(); }
    //! UAVを取得
    [[nodiscard]] ID3D11UnorderedAccessView* Uav() const noexcept { return uav_.Get(); }

    //! SRVアドレスを取得
    [[nodiscard]] ID3D11ShaderResourceView* const* SrvAddress() const noexcept { return srv_.GetAddressOf(); }
    //! RTVアドレスを取得
    [[nodiscard]] ID3D11RenderTargetView* const* RtvAddress() const noexcept { return rtv_.GetAddressOf(); }
    //! UAVアドレスを取得
    [[nodiscard]] ID3D11UnorderedAccessView* const* UavAddress() const noexcept { return uav_.GetAddressOf(); }

    //! 幅を取得
    [[nodiscard]] uint32_t Width() const noexcept { return width_; }
    //! 高さを取得
    [[nodiscard]] uint32_t Height() const noexcept { return height_; }
    //! 奥行きを取得
    [[nodiscard]] uint32_t Depth() const noexcept { return depth_; }
    //! フォーマットを取得
    [[nodiscard]] DXGI_FORMAT Format() const noexcept { return format_; }
    //! バインドフラグを取得
    [[nodiscard]] uint32_t BindFlags() const noexcept { return bindFlags_; }
    //! 次元を取得
    [[nodiscard]] TextureDimension Dimension() const noexcept { return dimension_; }

    //! 1Dテクスチャか判定
    [[nodiscard]] bool Is1D() const noexcept { return dimension_ == TextureDimension::Tex1D; }
    //! 2Dテクスチャか判定
    [[nodiscard]] bool Is2D() const noexcept { return dimension_ == TextureDimension::Tex2D; }
    //! 3Dテクスチャか判定
    [[nodiscard]] bool Is3D() const noexcept { return dimension_ == TextureDimension::Tex3D; }
    //! キューブマップか判定
    [[nodiscard]] bool IsCube() const noexcept { return dimension_ == TextureDimension::Cube; }
    //! SRVを持つか判定
    [[nodiscard]] bool HasSrv() const noexcept { return srv_ != nullptr; }
    //! RTVを持つか判定
    [[nodiscard]] bool HasRtv() const noexcept { return rtv_ != nullptr; }
    //! DSVを持つか判定
    [[nodiscard]] bool HasDsv() const noexcept { return dsv_ != nullptr; }
    //! UAVを持つか判定
    [[nodiscard]] bool HasUav() const noexcept { return uav_ != nullptr; }

    //! 記述子を再構築して取得
    [[nodiscard]] TextureDesc Desc() const noexcept {
        TextureDesc d;
        d.width = width_;
        d.height = height_;
        d.depth = depth_;
        d.format = format_;
        d.bindFlags = bindFlags_;
        d.dimension = dimension_;
        return d;
    }

private:
    ComPtr<ID3D11Resource> resource_;
    ComPtr<ID3D11ShaderResourceView> srv_;
    ComPtr<ID3D11RenderTargetView> rtv_;
    ComPtr<ID3D11DepthStencilView> dsv_;
    ComPtr<ID3D11UnorderedAccessView> uav_;
    uint32_t width_;
    uint32_t height_;
    uint32_t depth_;
    DXGI_FORMAT format_;
    uint32_t bindFlags_;
    TextureDimension dimension_;
};

//! テクスチャスマートポインタ
using TexturePtr = std::shared_ptr<Texture>;
