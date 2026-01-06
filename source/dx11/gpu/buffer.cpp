//----------------------------------------------------------------------------
//! @file   buffer.cpp
//! @brief  GPUバッファクラス実装
//----------------------------------------------------------------------------
#include "buffer.h"
#include "dx11/gpu_common.h"
#include "dx11/graphics_device.h"
#include "dx11/view/view.h"
#include "common/logging/logging.h"

//----------------------------------------------------------------------------
// Bufferライフサイクル
//----------------------------------------------------------------------------

Buffer::Buffer(ComPtr<ID3D11Buffer> buffer,
               std::unique_ptr<ShaderResourceView> srv,
               std::unique_ptr<UnorderedAccessView> uav,
               const BufferDesc& desc)
    : buffer_(std::move(buffer))
    , srv_(std::move(srv))
    , uav_(std::move(uav))
    , desc_(desc) {}

Buffer::~Buffer() = default;
Buffer::Buffer(Buffer&&) noexcept = default;
Buffer& Buffer::operator=(Buffer&&) noexcept = default;

//----------------------------------------------------------------------------
// Bufferメンバ関数
//----------------------------------------------------------------------------

ID3D11ShaderResourceView* Buffer::Srv() const noexcept {
    return srv_ ? srv_->Get() : nullptr;
}

ID3D11UnorderedAccessView* Buffer::Uav() const noexcept {
    return uav_ ? uav_->Get() : nullptr;
}

bool Buffer::HasSrv() const noexcept {
    return srv_ != nullptr && srv_->IsValid();
}

bool Buffer::HasUav() const noexcept {
    return uav_ != nullptr && uav_->IsValid();
}

//----------------------------------------------------------------------------
// Buffer静的ファクトリメソッド
//----------------------------------------------------------------------------

//! 汎用バッファを作成
std::shared_ptr<Buffer> Buffer::Create(
    const BufferDesc& desc,
    const void* initialData)
{
    auto* device = GetD3D11Device();
    RETURN_NULL_IF_NULL(device, "[Buffer] D3D11Deviceがnullです");

    D3D11_BUFFER_DESC d3dDesc = {};
    d3dDesc.ByteWidth = desc.size;
    d3dDesc.Usage = desc.usage;
    d3dDesc.BindFlags = desc.bindFlags;
    d3dDesc.CPUAccessFlags = desc.cpuAccess;
    d3dDesc.MiscFlags = desc.miscFlags;
    d3dDesc.StructureByteStride = desc.stride;

    D3D11_SUBRESOURCE_DATA initData = {};
    D3D11_SUBRESOURCE_DATA* pInitData = nullptr;
    if (initialData) {
        initData.pSysMem = initialData;
        pInitData = &initData;
    }

    ComPtr<ID3D11Buffer> buffer;
    HRESULT hr = device->CreateBuffer(&d3dDesc, pInitData, buffer.GetAddressOf());
    RETURN_NULL_IF_FAILED(hr, "[Buffer] バッファ作成失敗");

    // SRV作成
    std::unique_ptr<ShaderResourceView> srv;
    if (desc.bindFlags & D3D11_BIND_SHADER_RESOURCE) {
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = desc.stride > 0 ? desc.size / desc.stride : desc.size;

        srv = ShaderResourceView::CreateFromBuffer(buffer.Get(), &srvDesc);
    }

    // UAV作成
    std::unique_ptr<UnorderedAccessView> uav;
    if (desc.bindFlags & D3D11_BIND_UNORDERED_ACCESS) {
        D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.Format = DXGI_FORMAT_UNKNOWN;
        uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
        uavDesc.Buffer.FirstElement = 0;
        uavDesc.Buffer.NumElements = desc.stride > 0 ? desc.size / desc.stride : desc.size;

        uav = UnorderedAccessView::CreateFromBuffer(buffer.Get(), &uavDesc);
    }

    return std::make_shared<Buffer>(
        std::move(buffer), std::move(srv), std::move(uav), desc);
}

//! 構造化バッファを作成（SRV/UAV付き）
std::shared_ptr<Buffer> Buffer::CreateStructured(
    uint32_t elementSize,
    uint32_t elementCount,
    bool withUav,
    const void* initialData)
{
    auto* device = GetD3D11Device();
    RETURN_NULL_IF_NULL(device, "[Buffer] D3D11Deviceがnullです");

    BufferDesc desc = BufferDesc::Structured(elementSize, elementCount, withUav);

    D3D11_BUFFER_DESC d3dDesc = {};
    d3dDesc.ByteWidth = desc.size;
    d3dDesc.Usage = desc.usage;
    d3dDesc.BindFlags = desc.bindFlags;
    d3dDesc.CPUAccessFlags = desc.cpuAccess;
    d3dDesc.MiscFlags = desc.miscFlags;
    d3dDesc.StructureByteStride = desc.stride;

    D3D11_SUBRESOURCE_DATA initData = {};
    D3D11_SUBRESOURCE_DATA* pInitData = nullptr;
    if (initialData) {
        initData.pSysMem = initialData;
        pInitData = &initData;
    }

    ComPtr<ID3D11Buffer> buffer;
    HRESULT hr = device->CreateBuffer(&d3dDesc, pInitData, buffer.GetAddressOf());
    RETURN_NULL_IF_FAILED(hr, "[Buffer] 構造化バッファ作成失敗");

    // SRV作成
    std::unique_ptr<ShaderResourceView> srv;
    if (desc.bindFlags & D3D11_BIND_SHADER_RESOURCE) {
        D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Format = DXGI_FORMAT_UNKNOWN;
        srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        srvDesc.Buffer.FirstElement = 0;
        srvDesc.Buffer.NumElements = elementCount;

        srv = ShaderResourceView::CreateFromBuffer(buffer.Get(), &srvDesc);
    }

    // UAV作成
    std::unique_ptr<UnorderedAccessView> uav;
    if (desc.bindFlags & D3D11_BIND_UNORDERED_ACCESS) {
        D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
        uavDesc.Format = DXGI_FORMAT_UNKNOWN;
        uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
        uavDesc.Buffer.FirstElement = 0;
        uavDesc.Buffer.NumElements = elementCount;

        uav = UnorderedAccessView::CreateFromBuffer(buffer.Get(), &uavDesc);
    }

    return std::make_shared<Buffer>(
        std::move(buffer), std::move(srv), std::move(uav), desc);
}

//! 頂点バッファを作成
std::shared_ptr<Buffer> Buffer::CreateVertex(
    uint32_t byteSize,
    uint32_t stride,
    bool dynamic,
    const void* initialData)
{
    auto* device = GetD3D11Device();
    RETURN_NULL_IF_NULL(device, "[Buffer] D3D11Deviceがnullです");

    BufferDesc desc = BufferDesc::Vertex(byteSize, dynamic);
    desc.stride = stride;

    D3D11_BUFFER_DESC d3dDesc = {};
    d3dDesc.ByteWidth = desc.size;
    d3dDesc.Usage = desc.usage;
    d3dDesc.BindFlags = desc.bindFlags;
    d3dDesc.CPUAccessFlags = desc.cpuAccess;

    D3D11_SUBRESOURCE_DATA initData = {};
    D3D11_SUBRESOURCE_DATA* pInitData = nullptr;
    if (initialData) {
        initData.pSysMem = initialData;
        pInitData = &initData;
    }

    ComPtr<ID3D11Buffer> buffer;
    HRESULT hr = device->CreateBuffer(&d3dDesc, pInitData, buffer.GetAddressOf());
    RETURN_NULL_IF_FAILED(hr, "[Buffer] 頂点バッファ作成失敗");

    return std::make_shared<Buffer>(std::move(buffer), nullptr, nullptr, desc);
}

//! インデックスバッファを作成
std::shared_ptr<Buffer> Buffer::CreateIndex(
    uint32_t byteSize,
    bool dynamic,
    const void* initialData)
{
    auto* device = GetD3D11Device();
    RETURN_NULL_IF_NULL(device, "[Buffer] D3D11Deviceがnullです");

    BufferDesc desc = BufferDesc::Index(byteSize, dynamic);

    D3D11_BUFFER_DESC d3dDesc = {};
    d3dDesc.ByteWidth = desc.size;
    d3dDesc.Usage = desc.usage;
    d3dDesc.BindFlags = desc.bindFlags;
    d3dDesc.CPUAccessFlags = desc.cpuAccess;

    D3D11_SUBRESOURCE_DATA initData = {};
    D3D11_SUBRESOURCE_DATA* pInitData = nullptr;
    if (initialData) {
        initData.pSysMem = initialData;
        pInitData = &initData;
    }

    ComPtr<ID3D11Buffer> buffer;
    HRESULT hr = device->CreateBuffer(&d3dDesc, pInitData, buffer.GetAddressOf());
    RETURN_NULL_IF_FAILED(hr, "[Buffer] インデックスバッファ作成失敗");

    return std::make_shared<Buffer>(std::move(buffer), nullptr, nullptr, desc);
}

//! 定数バッファを作成
std::shared_ptr<Buffer> Buffer::CreateConstant(uint32_t byteSize)
{
    auto* device = GetD3D11Device();
    RETURN_NULL_IF_NULL(device, "[Buffer] D3D11Deviceがnullです");

    BufferDesc desc = BufferDesc::Constant(byteSize);

    D3D11_BUFFER_DESC d3dDesc = {};
    d3dDesc.ByteWidth = desc.size;
    d3dDesc.Usage = desc.usage;
    d3dDesc.BindFlags = desc.bindFlags;
    d3dDesc.CPUAccessFlags = desc.cpuAccess;

    ComPtr<ID3D11Buffer> buffer;
    HRESULT hr = device->CreateBuffer(&d3dDesc, nullptr, buffer.GetAddressOf());
    RETURN_NULL_IF_FAILED(hr, "[Buffer] 定数バッファ作成失敗");

    return std::make_shared<Buffer>(std::move(buffer), nullptr, nullptr, desc);
}
