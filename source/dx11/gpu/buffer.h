//----------------------------------------------------------------------------
//! @file   buffer.h
//! @brief  GPUバッファクラス（統一設計）
//----------------------------------------------------------------------------
#pragma once

#include "dx11/gpu_common.h"
#include "dx11/view/view.h"
#include "gpu_resource.h"

//===========================================================================
//! @brief バッファ記述子
//! @note  Layout (24 bytes)
//===========================================================================
struct BufferDesc {
    uint32_t size;          //!< バッファサイズ（バイト）
    uint32_t stride;        //!< 要素サイズ（構造化バッファ用）
    D3D11_USAGE usage;      //!< 使用法
    uint32_t bindFlags;     //!< バインドフラグ
    uint32_t cpuAccess;     //!< CPUアクセスフラグ
    uint32_t miscFlags;     //!< その他フラグ

    BufferDesc()
        : size(0), stride(0), usage(D3D11_USAGE_DEFAULT)
        , bindFlags(0), cpuAccess(0), miscFlags(0) {}

    //! 頂点バッファ記述子を作成
    [[nodiscard]] static BufferDesc Vertex(uint32_t byteSize, bool dynamic = false) {
        BufferDesc d;
        d.size = static_cast<uint32_t>(AlignGpuSize(byteSize));
        d.usage = dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
        d.bindFlags = D3D11_BIND_VERTEX_BUFFER;
        d.cpuAccess = dynamic ? D3D11_CPU_ACCESS_WRITE : 0;
        return d;
    }

    //! インデックスバッファ記述子を作成
    [[nodiscard]] static BufferDesc Index(uint32_t byteSize, bool dynamic = false) {
        BufferDesc d;
        d.size = static_cast<uint32_t>(AlignGpuSize(byteSize));
        d.usage = dynamic ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
        d.bindFlags = D3D11_BIND_INDEX_BUFFER;
        d.cpuAccess = dynamic ? D3D11_CPU_ACCESS_WRITE : 0;
        return d;
    }

    //! 定数バッファ記述子を作成
    [[nodiscard]] static BufferDesc Constant(uint32_t byteSize) {
        BufferDesc d;
        d.size = static_cast<uint32_t>(AlignGpuSize(byteSize));
        d.usage = D3D11_USAGE_DYNAMIC;
        d.bindFlags = D3D11_BIND_CONSTANT_BUFFER;
        d.cpuAccess = D3D11_CPU_ACCESS_WRITE;
        return d;
    }

    //! 構造化バッファ記述子を作成
    [[nodiscard]] static BufferDesc Structured(uint32_t elementSize, uint32_t count, bool uav = false) {
        BufferDesc d;
        d.stride = static_cast<uint32_t>(AlignGpuSize(elementSize));
        d.size = d.stride * count;
        d.usage = D3D11_USAGE_DEFAULT;
        d.bindFlags = D3D11_BIND_SHADER_RESOURCE | (uav ? D3D11_BIND_UNORDERED_ACCESS : 0);
        d.miscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
        return d;
    }

    //! ハッシュキーを計算
    [[nodiscard]] uint64_t HashKey() const noexcept {
        return (static_cast<uint64_t>(size) << 32) |
               (static_cast<uint64_t>(bindFlags) << 16) |
               (static_cast<uint64_t>(usage) << 8) |
               (stride & 0xFF);
    }
};

static_assert(sizeof(BufferDesc) == 24, "BufferDesc must be 24 bytes");

//===========================================================================
//! @brief GPUバッファクラス
//===========================================================================
class Buffer : private NonCopyable {
public:
    //----------------------------------------------------------
    //! @name   ファクトリメソッド
    //----------------------------------------------------------
    //!@{

    //! 汎用バッファを作成
    //! @param desc バッファ記述子
    //! @param initialData 初期データ（nullptrで空）
    [[nodiscard]] static std::shared_ptr<Buffer> Create(
        const BufferDesc& desc,
        const void* initialData = nullptr);

    //! 構造化バッファを作成（SRV/UAV付き）
    [[nodiscard]] static std::shared_ptr<Buffer> CreateStructured(
        uint32_t elementSize,
        uint32_t elementCount,
        bool withUav = false,
        const void* initialData = nullptr);

    //! 頂点バッファを作成
    [[nodiscard]] static std::shared_ptr<Buffer> CreateVertex(
        uint32_t byteSize,
        uint32_t stride,
        bool dynamic = false,
        const void* initialData = nullptr);

    //! インデックスバッファを作成
    [[nodiscard]] static std::shared_ptr<Buffer> CreateIndex(
        uint32_t byteSize,
        bool dynamic = false,
        const void* initialData = nullptr);

    //! 定数バッファを作成
    [[nodiscard]] static std::shared_ptr<Buffer> CreateConstant(uint32_t byteSize);

    //!@}
    //----------------------------------------------------------
    //! @name   コンストラクタ
    //----------------------------------------------------------
    //!@{

    //! コンストラクタ（.cppで定義）
    Buffer(ComPtr<ID3D11Buffer> buffer,
           View<SRV> srv,
           View<UAV> uav,
           const BufferDesc& desc);

    //! デストラクタ（.cppで定義）
    ~Buffer();

    //! ムーブコンストラクタ
    Buffer(Buffer&&) noexcept;
    //! ムーブ代入演算子
    Buffer& operator=(Buffer&&) noexcept;

    //!@}
    //----------------------------------------------------------
    //! @name   アクセサ
    //----------------------------------------------------------
    //!@{

    //! GPUメモリサイズを取得
    [[nodiscard]] size_t GpuSize() const noexcept { return desc_.size; }

    //! D3D11バッファを取得
    [[nodiscard]] ID3D11Buffer* Get() const noexcept { return buffer_.Get(); }

    //! D3D11バッファのアドレスを取得
    [[nodiscard]] ID3D11Buffer* const* AddressOf() const noexcept { return buffer_.GetAddressOf(); }

    //! SRVを取得
    [[nodiscard]] ID3D11ShaderResourceView* Srv() const noexcept;

    //! UAVを取得
    [[nodiscard]] ID3D11UnorderedAccessView* Uav() const noexcept;

    //! SRVを持つか判定
    [[nodiscard]] bool HasSrv() const noexcept;

    //! UAVを持つか判定
    [[nodiscard]] bool HasUav() const noexcept;

    //! バッファサイズを取得（バイト）
    [[nodiscard]] uint32_t Size() const noexcept { return desc_.size; }

    //! ストライドを取得
    [[nodiscard]] uint32_t Stride() const noexcept { return desc_.stride; }

    //! 使用法を取得
    [[nodiscard]] D3D11_USAGE Usage() const noexcept { return desc_.usage; }

    //! バインドフラグを取得
    [[nodiscard]] uint32_t BindFlags() const noexcept { return desc_.bindFlags; }

    //! CPUアクセスフラグを取得
    [[nodiscard]] uint32_t CpuAccess() const noexcept { return desc_.cpuAccess; }

    //! その他フラグを取得
    [[nodiscard]] uint32_t MiscFlags() const noexcept { return desc_.miscFlags; }

    //! 動的バッファか判定
    [[nodiscard]] bool IsDynamic() const noexcept { return desc_.usage == D3D11_USAGE_DYNAMIC; }

    //! 構造化バッファか判定
    [[nodiscard]] bool IsStructured() const noexcept {
        return (desc_.miscFlags & D3D11_RESOURCE_MISC_BUFFER_STRUCTURED) != 0;
    }

    //! 記述子を取得
    [[nodiscard]] const BufferDesc& Desc() const noexcept { return desc_; }

    //!@}

private:
    ComPtr<ID3D11Buffer> buffer_;
    View<SRV> srv_;
    View<UAV> uav_;
    BufferDesc desc_;
};

//! バッファスマートポインタ
using BufferPtr = std::shared_ptr<Buffer>;
