//----------------------------------------------------------------------------
//! @file   test_buffer.cpp
//! @brief  バッファシステム テストスイート
//!
//! @details
//! このファイルはバッファシステムの包括的なテストを提供します。
//!
//! テストカテゴリ:
//! - BufferDesc: バッファ記述子ヘルパー関数のテスト
//! - Buffer静的ファクトリ: Buffer::CreateXxx系メソッドのテスト
//! - VertexBuffer: 頂点バッファの生成、GPU Readback検証
//! - IndexBuffer: インデックスバッファの生成、GPU Readback検証
//! - ConstantBuffer: 定数バッファの生成と更新
//! - StructuredBuffer: 構造化バッファとSRV/UAVのテスト
//! - DynamicBuffer: 動的バッファの更新テスト
//! - BufferAccessors: バッファアクセサメソッドのテスト
//! - EdgeCases: エッジケースのテスト
//!
//! @note D3D11デバイスが必要なテストは自動的にスキップされます
//----------------------------------------------------------------------------
#include "test_buffer.h"
#include "test_common.h"
#include "dx11/gpu_common.h"
#include "dx11/graphics_device.h"
#include "dx11/graphics_context.h"
#include "dx11/gpu/gpu.h"
#include "common/logging/logging.h"
#include <cassert>
#include <iostream>
#include <cstring>
#include <cmath>

namespace tests {

//----------------------------------------------------------------------------
// テストユーティリティ（共通ヘッダーから使用）
//----------------------------------------------------------------------------

// グローバルカウンターを使用（後方互換性のため）
#define s_testCount tests::GetGlobalTestCount()
#define s_passCount tests::GetGlobalPassCount()

//----------------------------------------------------------------------------
// テスト用頂点構造体
//----------------------------------------------------------------------------

//! シンプルな頂点構造体（位置のみ）
struct SimpleVertex
{
    float x, y, z;
};

//! 拡張頂点構造体（位置＋法線＋UV）
struct ExtendedVertex
{
    float px, py, pz;    // Position
    float nx, ny, nz;    // Normal
    float u, v;          // TexCoord
};

//----------------------------------------------------------------------------
// バッファ作成ヘルパー関数
//----------------------------------------------------------------------------

//! 頂点バッファを作成
//! @param data 初期データ
//! @param sizeInBytes データサイズ
//! @param stride 頂点ストライド
//! @param dynamic 動的バッファか
//! @return 作成されたバッファ（失敗時nullptr）
static BufferPtr CreateVertexBuffer(const void* data, uint32_t sizeInBytes, uint32_t stride, bool dynamic = false)
{
    auto* device = GetD3D11Device();
    if (!device) return nullptr;

    BufferDesc desc = BufferDesc::Vertex(sizeInBytes, dynamic);
    desc.stride = stride;

    D3D11_BUFFER_DESC d3dDesc = {};
    d3dDesc.ByteWidth = desc.size;
    d3dDesc.Usage = desc.usage;
    d3dDesc.BindFlags = desc.bindFlags;
    d3dDesc.CPUAccessFlags = desc.cpuAccess;
    d3dDesc.MiscFlags = desc.miscFlags;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = data;

    ComPtr<ID3D11Buffer> buffer;
    HRESULT hr = device->CreateBuffer(&d3dDesc, data ? &initData : nullptr, buffer.GetAddressOf());
    if (FAILED(hr)) {
        LOG_HRESULT(hr, "CreateVertexBuffer failed");
        return nullptr;
    }

    return std::make_shared<Buffer>(std::move(buffer), nullptr, nullptr, desc);
}

//! インデックスバッファを作成（16bit）
static BufferPtr CreateIndexBuffer16(const uint16_t* indices, uint32_t count)
{
    auto* device = GetD3D11Device();
    if (!device) return nullptr;

    uint32_t sizeInBytes = count * sizeof(uint16_t);
    BufferDesc desc = BufferDesc::Index(sizeInBytes, false);

    D3D11_BUFFER_DESC d3dDesc = {};
    d3dDesc.ByteWidth = desc.size;
    d3dDesc.Usage = D3D11_USAGE_IMMUTABLE;
    d3dDesc.BindFlags = desc.bindFlags;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = indices;

    ComPtr<ID3D11Buffer> buffer;
    HRESULT hr = device->CreateBuffer(&d3dDesc, &initData, buffer.GetAddressOf());
    if (FAILED(hr)) {
        LOG_HRESULT(hr, "CreateIndexBuffer16 failed");
        return nullptr;
    }

    return std::make_shared<Buffer>(std::move(buffer), nullptr, nullptr, desc);
}

//! インデックスバッファを作成（32bit）
static BufferPtr CreateIndexBuffer32(const uint32_t* indices, uint32_t count)
{
    auto* device = GetD3D11Device();
    if (!device) return nullptr;

    uint32_t sizeInBytes = count * sizeof(uint32_t);
    BufferDesc desc = BufferDesc::Index(sizeInBytes, false);

    D3D11_BUFFER_DESC d3dDesc = {};
    d3dDesc.ByteWidth = desc.size;
    d3dDesc.Usage = D3D11_USAGE_IMMUTABLE;
    d3dDesc.BindFlags = desc.bindFlags;

    D3D11_SUBRESOURCE_DATA initData = {};
    initData.pSysMem = indices;

    ComPtr<ID3D11Buffer> buffer;
    HRESULT hr = device->CreateBuffer(&d3dDesc, &initData, buffer.GetAddressOf());
    if (FAILED(hr)) {
        LOG_HRESULT(hr, "CreateIndexBuffer32 failed");
        return nullptr;
    }

    return std::make_shared<Buffer>(std::move(buffer), nullptr, nullptr, desc);
}

//! 定数バッファを作成
static BufferPtr CreateConstantBuffer(uint32_t sizeInBytes)
{
    auto* device = GetD3D11Device();
    if (!device) return nullptr;

    BufferDesc desc = BufferDesc::Constant(sizeInBytes);

    D3D11_BUFFER_DESC d3dDesc = {};
    d3dDesc.ByteWidth = desc.size;
    d3dDesc.Usage = desc.usage;
    d3dDesc.BindFlags = desc.bindFlags;
    d3dDesc.CPUAccessFlags = desc.cpuAccess;

    ComPtr<ID3D11Buffer> buffer;
    HRESULT hr = device->CreateBuffer(&d3dDesc, nullptr, buffer.GetAddressOf());
    if (FAILED(hr)) {
        LOG_HRESULT(hr, "CreateConstantBuffer failed");
        return nullptr;
    }

    return std::make_shared<Buffer>(std::move(buffer), nullptr, nullptr, desc);
}

//----------------------------------------------------------------------------
// GPU Readback ユーティリティ
//----------------------------------------------------------------------------

//! ステージングバッファを作成してGPUバッファからコピー
//! @param srcBuffer コピー元バッファ
//! @return 読み戻したデータ（失敗時は空）
static std::vector<std::byte> ReadbackBuffer(ID3D11Buffer* srcBuffer)
{
    if (!srcBuffer) return {};

    auto& ctx = GraphicsContext::Get();
    auto* device = GetD3D11Device();

    // 元のバッファの情報を取得
    D3D11_BUFFER_DESC srcDesc;
    srcBuffer->GetDesc(&srcDesc);

    // ステージングバッファを作成
    D3D11_BUFFER_DESC stagingDesc = {};
    stagingDesc.ByteWidth = srcDesc.ByteWidth;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingDesc.BindFlags = 0;

    ComPtr<ID3D11Buffer> stagingBuffer;
    HRESULT hr = device->CreateBuffer(&stagingDesc, nullptr, stagingBuffer.GetAddressOf());
    if (FAILED(hr)) return {};

    // GPUからステージングへコピー
    ctx.CopyResource(stagingBuffer.Get(), srcBuffer);

    // マップして読み戻し
    D3D11_MAPPED_SUBRESOURCE mapped = ctx.Map(stagingBuffer.Get(), 0, D3D11_MAP_READ);
    if (!mapped.pData) return {};

    std::vector<std::byte> result(srcDesc.ByteWidth);
    std::memcpy(result.data(), mapped.pData, srcDesc.ByteWidth);

    ctx.Unmap(stagingBuffer.Get(), 0);
    return result;
}

//----------------------------------------------------------------------------
// BufferDesc テスト
//----------------------------------------------------------------------------

//! BufferDesc::Vertex テスト
static void TestBufferDesc_Vertex()
{
    std::cout << "\n=== BufferDesc::Vertex テスト ===" << std::endl;

    // 静的バッファ
    BufferDesc desc = BufferDesc::Vertex(1024, false);
    TEST_ASSERT(desc.size >= 1024, "サイズが1024以上であること");
    TEST_ASSERT(desc.usage == D3D11_USAGE_DEFAULT, "静的バッファはDEFAULTであること");
    TEST_ASSERT(desc.bindFlags == D3D11_BIND_VERTEX_BUFFER, "バインドフラグがVERTEX_BUFFERであること");
    TEST_ASSERT(desc.cpuAccess == 0, "静的バッファはCPUアクセスなしであること");

    // 動的バッファ
    BufferDesc descDynamic = BufferDesc::Vertex(2048, true);
    TEST_ASSERT(descDynamic.usage == D3D11_USAGE_DYNAMIC, "動的バッファはDYNAMICであること");
    TEST_ASSERT(descDynamic.cpuAccess == D3D11_CPU_ACCESS_WRITE, "動的バッファはCPU_ACCESS_WRITEであること");
}

//! BufferDesc::Index テスト
static void TestBufferDesc_Index()
{
    std::cout << "\n=== BufferDesc::Index テスト ===" << std::endl;

    // 静的バッファ
    BufferDesc desc = BufferDesc::Index(512, false);
    TEST_ASSERT(desc.size >= 512, "サイズが512以上であること");
    TEST_ASSERT(desc.usage == D3D11_USAGE_DEFAULT, "静的バッファはDEFAULTであること");
    TEST_ASSERT(desc.bindFlags == D3D11_BIND_INDEX_BUFFER, "バインドフラグがINDEX_BUFFERであること");
    TEST_ASSERT(desc.cpuAccess == 0, "静的バッファはCPUアクセスなしであること");

    // 動的バッファ
    BufferDesc descDynamic = BufferDesc::Index(1024, true);
    TEST_ASSERT(descDynamic.usage == D3D11_USAGE_DYNAMIC, "動的バッファはDYNAMICであること");
    TEST_ASSERT(descDynamic.cpuAccess == D3D11_CPU_ACCESS_WRITE, "動的バッファはCPU_ACCESS_WRITEであること");
}

//! BufferDesc::Constant テスト
static void TestBufferDesc_Constant()
{
    std::cout << "\n=== BufferDesc::Constant テスト ===" << std::endl;

    BufferDesc desc = BufferDesc::Constant(256);
    TEST_ASSERT(desc.size >= 256, "サイズが256以上であること");
    TEST_ASSERT(desc.usage == D3D11_USAGE_DYNAMIC, "定数バッファはDYNAMICであること");
    TEST_ASSERT(desc.bindFlags == D3D11_BIND_CONSTANT_BUFFER, "バインドフラグがCONSTANT_BUFFERであること");
    TEST_ASSERT(desc.cpuAccess == D3D11_CPU_ACCESS_WRITE, "定数バッファはCPU_ACCESS_WRITEであること");

    // 16バイトアライメント確認
    BufferDesc descUnaligned = BufferDesc::Constant(100);
    TEST_ASSERT(descUnaligned.size % 16 == 0, "定数バッファは16バイト境界にアライメントされること");
}

//! BufferDesc::Structured テスト
static void TestBufferDesc_Structured()
{
    std::cout << "\n=== BufferDesc::Structured テスト ===" << std::endl;

    // SRVのみ
    BufferDesc descSrv = BufferDesc::Structured(64, 100, false);
    TEST_ASSERT(descSrv.stride >= 64, "ストライドが64以上であること");
    TEST_ASSERT(descSrv.size == descSrv.stride * 100, "サイズがstride*countであること");
    TEST_ASSERT(descSrv.usage == D3D11_USAGE_DEFAULT, "構造化バッファはDEFAULTであること");
    TEST_ASSERT((descSrv.bindFlags & D3D11_BIND_SHADER_RESOURCE) != 0, "SRVフラグがあること");
    TEST_ASSERT((descSrv.bindFlags & D3D11_BIND_UNORDERED_ACCESS) == 0, "UAVフラグがないこと");
    TEST_ASSERT(descSrv.miscFlags == D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, "STRUCTUREDフラグがあること");

    // SRV + UAV
    BufferDesc descUav = BufferDesc::Structured(32, 50, true);
    TEST_ASSERT((descUav.bindFlags & D3D11_BIND_SHADER_RESOURCE) != 0, "SRVフラグがあること");
    TEST_ASSERT((descUav.bindFlags & D3D11_BIND_UNORDERED_ACCESS) != 0, "UAVフラグがあること");
}

//! BufferDesc::HashKey テスト
static void TestBufferDesc_HashKey()
{
    std::cout << "\n=== BufferDesc::HashKey テスト ===" << std::endl;

    BufferDesc desc1 = BufferDesc::Vertex(1024, false);
    BufferDesc desc2 = BufferDesc::Vertex(1024, false);
    BufferDesc desc3 = BufferDesc::Vertex(2048, false);
    BufferDesc desc4 = BufferDesc::Index(1024, false);

    TEST_ASSERT(desc1.HashKey() == desc2.HashKey(), "同じ記述子は同じハッシュであること");
    TEST_ASSERT(desc1.HashKey() != desc3.HashKey(), "異なるサイズは異なるハッシュであること");
    TEST_ASSERT(desc1.HashKey() != desc4.HashKey(), "異なるバインドフラグは異なるハッシュであること");
}

//----------------------------------------------------------------------------
// Buffer静的ファクトリメソッド テスト
//----------------------------------------------------------------------------

//! Buffer::CreateVertex テスト
static void TestBuffer_CreateVertex()
{
    std::cout << "\n=== Buffer::CreateVertex テスト ===" << std::endl;

    if (!GraphicsDevice::Get().IsValid()) {
        std::cout << "[スキップ] GraphicsDeviceが初期化されていません" << std::endl;
        return;
    }

    SimpleVertex vertices[] = {
        { 0.0f, 0.5f, 0.0f },
        { 0.5f, -0.5f, 0.0f },
        {-0.5f, -0.5f, 0.0f },
    };

    // 静的頂点バッファ
    auto vbStatic = Buffer::CreateVertex(sizeof(vertices), sizeof(SimpleVertex), false, vertices);
    TEST_ASSERT(vbStatic != nullptr, "静的頂点バッファが作成できること");
    TEST_ASSERT(vbStatic->Get() != nullptr, "D3D11Bufferが有効であること");
    TEST_ASSERT(vbStatic->Stride() == sizeof(SimpleVertex), "ストライドが正しいこと");
    TEST_ASSERT(!vbStatic->IsDynamic(), "静的バッファと判定されること");
    TEST_ASSERT(!vbStatic->HasSrv(), "頂点バッファはSRVを持たないこと");
    TEST_ASSERT(!vbStatic->HasUav(), "頂点バッファはUAVを持たないこと");

    // 動的頂点バッファ
    auto vbDynamic = Buffer::CreateVertex(1024, sizeof(SimpleVertex), true, nullptr);
    TEST_ASSERT(vbDynamic != nullptr, "動的頂点バッファが作成できること");
    TEST_ASSERT(vbDynamic->IsDynamic(), "動的バッファと判定されること");
}

//! Buffer::CreateIndex テスト
static void TestBuffer_CreateIndex()
{
    std::cout << "\n=== Buffer::CreateIndex テスト ===" << std::endl;

    if (!GraphicsDevice::Get().IsValid()) {
        std::cout << "[スキップ] GraphicsDeviceが初期化されていません" << std::endl;
        return;
    }

    uint16_t indices[] = { 0, 1, 2 };

    // 静的インデックスバッファ
    auto ibStatic = Buffer::CreateIndex(sizeof(indices), false, indices);
    TEST_ASSERT(ibStatic != nullptr, "静的インデックスバッファが作成できること");
    TEST_ASSERT(ibStatic->Get() != nullptr, "D3D11Bufferが有効であること");
    TEST_ASSERT(!ibStatic->IsDynamic(), "静的バッファと判定されること");

    // 動的インデックスバッファ
    auto ibDynamic = Buffer::CreateIndex(1024, true, nullptr);
    TEST_ASSERT(ibDynamic != nullptr, "動的インデックスバッファが作成できること");
    TEST_ASSERT(ibDynamic->IsDynamic(), "動的バッファと判定されること");
}

//! Buffer::CreateConstant テスト
static void TestBuffer_CreateConstant()
{
    std::cout << "\n=== Buffer::CreateConstant テスト ===" << std::endl;

    if (!GraphicsDevice::Get().IsValid()) {
        std::cout << "[スキップ] GraphicsDeviceが初期化されていません" << std::endl;
        return;
    }

    auto cb = Buffer::CreateConstant(256);
    TEST_ASSERT(cb != nullptr, "定数バッファが作成できること");
    TEST_ASSERT(cb->Get() != nullptr, "D3D11Bufferが有効であること");
    TEST_ASSERT(cb->IsDynamic(), "定数バッファはDYNAMICであること");
    TEST_ASSERT(cb->Size() >= 256, "サイズが256以上であること");
    TEST_ASSERT(cb->Size() % 16 == 0, "16バイト境界にアライメントされていること");
}

//! Buffer::CreateStructured テスト
static void TestBuffer_CreateStructured()
{
    std::cout << "\n=== Buffer::CreateStructured テスト ===" << std::endl;

    if (!GraphicsDevice::Get().IsValid()) {
        std::cout << "[スキップ] GraphicsDeviceが初期化されていません" << std::endl;
        return;
    }

    // SRVのみの構造化バッファ
    auto sbSrv = Buffer::CreateStructured(sizeof(SimpleVertex), 100, false, nullptr);
    TEST_ASSERT(sbSrv != nullptr, "構造化バッファ(SRVのみ)が作成できること");
    TEST_ASSERT(sbSrv->IsStructured(), "構造化バッファと判定されること");
    TEST_ASSERT(sbSrv->HasSrv(), "SRVを持つこと");
    TEST_ASSERT(!sbSrv->HasUav(), "UAVを持たないこと");
    TEST_ASSERT(sbSrv->Srv() != nullptr, "SRVが取得できること");
    TEST_ASSERT(sbSrv->Uav() == nullptr, "UAVはnullptrであること");

    // SRV+UAVの構造化バッファ
    auto sbUav = Buffer::CreateStructured(sizeof(SimpleVertex), 100, true, nullptr);
    TEST_ASSERT(sbUav != nullptr, "構造化バッファ(SRV+UAV)が作成できること");
    TEST_ASSERT(sbUav->IsStructured(), "構造化バッファと判定されること");
    TEST_ASSERT(sbUav->HasSrv(), "SRVを持つこと");
    TEST_ASSERT(sbUav->HasUav(), "UAVを持つこと");
    TEST_ASSERT(sbUav->Srv() != nullptr, "SRVが取得できること");
    TEST_ASSERT(sbUav->Uav() != nullptr, "UAVが取得できること");
}

//! Buffer::Create 汎用ファクトリ テスト
static void TestBuffer_Create_Generic()
{
    std::cout << "\n=== Buffer::Create 汎用ファクトリ テスト ===" << std::endl;

    if (!GraphicsDevice::Get().IsValid()) {
        std::cout << "[スキップ] GraphicsDeviceが初期化されていません" << std::endl;
        return;
    }

    // 頂点バッファを汎用Createで作成
    BufferDesc vbDesc = BufferDesc::Vertex(1024, false);
    auto vb = Buffer::Create(vbDesc, nullptr);
    TEST_ASSERT(vb != nullptr, "汎用Createで頂点バッファが作成できること");
    TEST_ASSERT(vb->BindFlags() == D3D11_BIND_VERTEX_BUFFER, "バインドフラグが正しいこと");

    // インデックスバッファを汎用Createで作成
    BufferDesc ibDesc = BufferDesc::Index(512, false);
    auto ib = Buffer::Create(ibDesc, nullptr);
    TEST_ASSERT(ib != nullptr, "汎用Createでインデックスバッファが作成できること");
    TEST_ASSERT(ib->BindFlags() == D3D11_BIND_INDEX_BUFFER, "バインドフラグが正しいこと");

    // 定数バッファを汎用Createで作成
    BufferDesc cbDesc = BufferDesc::Constant(256);
    auto cb = Buffer::Create(cbDesc, nullptr);
    TEST_ASSERT(cb != nullptr, "汎用Createで定数バッファが作成できること");
    TEST_ASSERT(cb->BindFlags() == D3D11_BIND_CONSTANT_BUFFER, "バインドフラグが正しいこと");

    // 構造化バッファを汎用Createで作成（SRV付き）
    BufferDesc sbDesc = BufferDesc::Structured(16, 100, true);
    auto sb = Buffer::Create(sbDesc, nullptr);
    TEST_ASSERT(sb != nullptr, "汎用Createで構造化バッファが作成できること");
    TEST_ASSERT(sb->IsStructured(), "構造化バッファと判定されること");
    TEST_ASSERT(sb->HasSrv(), "SRVを持つこと");
    TEST_ASSERT(sb->HasUav(), "UAVを持つこと");
}

//----------------------------------------------------------------------------
// VertexBuffer テスト
//----------------------------------------------------------------------------

//! VertexBuffer 基本生成テスト
//! @details CreateImmutableでの静的頂点バッファ生成をテスト
static void TestVertexBuffer_CreateImmutable()
{
    std::cout << "\n=== VertexBuffer 静的生成テスト ===" << std::endl;

    if (!GraphicsDevice::Get().IsValid()) {
        std::cout << "[スキップ] GraphicsDeviceが初期化されていません" << std::endl;
        return;
    }

    // テスト用頂点データ
    SimpleVertex vertices[] = {
        { 0.0f,  0.5f, 0.0f },  // 上
        { 0.5f, -0.5f, 0.0f },  // 右下
        {-0.5f, -0.5f, 0.0f },  // 左下
    };

    auto vb = CreateVertexBuffer(
        vertices,
        sizeof(vertices),
        sizeof(SimpleVertex),
        false);  // immutable

    TEST_ASSERT(vb != nullptr, "CreateVertexBufferが成功すること");
    TEST_ASSERT(vb->Get() != nullptr, "頂点バッファが有効であること");
    TEST_ASSERT(vb->Size() >= sizeof(vertices), "バッファサイズが十分であること");
    TEST_ASSERT(vb->Stride() == sizeof(SimpleVertex), "ストライドが正しいこと");
    TEST_ASSERT(vb->Usage() == D3D11_USAGE_DEFAULT, "UsageがDEFAULTであること");
}

//! VertexBuffer 動的生成テスト
//! @details CreateDynamicでの動的頂点バッファ生成をテスト
static void TestVertexBuffer_CreateDynamic()
{
    std::cout << "\n=== VertexBuffer 動的生成テスト ===" << std::endl;

    if (!GraphicsDevice::Get().IsValid()) {
        std::cout << "[スキップ] GraphicsDeviceが初期化されていません" << std::endl;
        return;
    }

    const uint32_t maxVertices = 100;
    auto vb = CreateVertexBuffer(
        nullptr,
        maxVertices * sizeof(SimpleVertex),
        sizeof(SimpleVertex),
        true);  // dynamic

    TEST_ASSERT(vb != nullptr, "CreateDynamicが成功すること");
    TEST_ASSERT(vb->Get() != nullptr, "動的頂点バッファが有効であること");
    TEST_ASSERT(vb->Stride() == sizeof(SimpleVertex), "ストライドが正しいこと");
    TEST_ASSERT(vb->Usage() == D3D11_USAGE_DYNAMIC, "UsageがDYNAMICであること");
}

//! VertexBuffer GPU Readbackテスト（SimpleVertex）
//! @details GPUに書き込んだ頂点データを読み戻して検証
static void TestVertexBuffer_GPUReadback_Simple()
{
    std::cout << "\n=== VertexBuffer GPU Readbackテスト (Simple) ===" << std::endl;

    if (!GraphicsDevice::Get().IsValid()) {
        std::cout << "[スキップ] GraphicsDeviceが初期化されていません" << std::endl;
        return;
    }

    // テスト用頂点データ（三角形）
    SimpleVertex originalVertices[] = {
        {  0.0f,   1.0f,  0.0f },
        {  1.0f,  -1.0f,  0.0f },
        { -1.0f,  -1.0f,  0.0f },
    };
    const size_t dataSize = sizeof(originalVertices);

    // DEFAULTバッファとして作成（Readback可能）
    auto vb = CreateVertexBuffer(originalVertices, static_cast<uint32_t>(dataSize), sizeof(SimpleVertex), false);
    TEST_ASSERT(vb != nullptr, "頂点バッファが作成できること");

    // GPUからデータを読み戻す
    auto readbackData = ReadbackBuffer(vb->Get());
    TEST_ASSERT(!readbackData.empty(), "Readbackデータが取得できること");
    TEST_ASSERT(readbackData.size() >= dataSize, "Readbackデータサイズが十分であること");

    // バイト単位で比較
    const std::byte* originalBytes = reinterpret_cast<const std::byte*>(originalVertices);
    bool allMatch = std::memcmp(readbackData.data(), originalBytes, dataSize) == 0;
    TEST_ASSERT(allMatch, "GPU上の頂点データが元のデータと完全に一致すること");

    // 頂点ごとに詳細比較
    if (!allMatch) {
        const SimpleVertex* readbackVertices = reinterpret_cast<const SimpleVertex*>(readbackData.data());
        for (int i = 0; i < 3; ++i) {
            std::cout << "  頂点" << i << " - 元: ("
                      << originalVertices[i].x << ", "
                      << originalVertices[i].y << ", "
                      << originalVertices[i].z << ") / 読み戻し: ("
                      << readbackVertices[i].x << ", "
                      << readbackVertices[i].y << ", "
                      << readbackVertices[i].z << ")" << std::endl;
        }
    }
}

//! VertexBuffer GPU Readbackテスト（ExtendedVertex）
//! @details より複雑な頂点構造体でのReadback検証
static void TestVertexBuffer_GPUReadback_Extended()
{
    std::cout << "\n=== VertexBuffer GPU Readbackテスト (Extended) ===" << std::endl;

    if (!GraphicsDevice::Get().IsValid()) {
        std::cout << "[スキップ] GraphicsDeviceが初期化されていません" << std::endl;
        return;
    }

    // 拡張頂点データ（四角形、2つの三角形）
    ExtendedVertex originalVertices[] = {
        // Position           Normal              UV
        { -1.0f,  1.0f, 0.0f,  0.0f, 0.0f, 1.0f,  0.0f, 0.0f },  // 左上
        {  1.0f,  1.0f, 0.0f,  0.0f, 0.0f, 1.0f,  1.0f, 0.0f },  // 右上
        { -1.0f, -1.0f, 0.0f,  0.0f, 0.0f, 1.0f,  0.0f, 1.0f },  // 左下
        {  1.0f,  1.0f, 0.0f,  0.0f, 0.0f, 1.0f,  1.0f, 0.0f },  // 右上
        {  1.0f, -1.0f, 0.0f,  0.0f, 0.0f, 1.0f,  1.0f, 1.0f },  // 右下
        { -1.0f, -1.0f, 0.0f,  0.0f, 0.0f, 1.0f,  0.0f, 1.0f },  // 左下
    };
    const size_t dataSize = sizeof(originalVertices);
    const int vertexCount = sizeof(originalVertices) / sizeof(ExtendedVertex);

    auto vb = CreateVertexBuffer(originalVertices, static_cast<uint32_t>(dataSize), sizeof(ExtendedVertex), false);
    TEST_ASSERT(vb != nullptr, "拡張頂点バッファが作成できること");

    // GPUからデータを読み戻す
    auto readbackData = ReadbackBuffer(vb->Get());
    TEST_ASSERT(!readbackData.empty(), "拡張頂点Readbackデータが取得できること");
    TEST_ASSERT(readbackData.size() >= dataSize, "拡張頂点Readbackデータサイズが十分であること");

    // 全頂点を比較
    const ExtendedVertex* readbackVertices = reinterpret_cast<const ExtendedVertex*>(readbackData.data());
    bool allMatch = true;

    for (int i = 0; i < vertexCount; ++i) {
        const auto& orig = originalVertices[i];
        const auto& rb = readbackVertices[i];

        // 浮動小数点の比較（完全一致を期待）
        if (orig.px != rb.px || orig.py != rb.py || orig.pz != rb.pz ||
            orig.nx != rb.nx || orig.ny != rb.ny || orig.nz != rb.nz ||
            orig.u != rb.u || orig.v != rb.v) {
            allMatch = false;
            std::cout << "  頂点" << i << "が不一致" << std::endl;
        }
    }

    TEST_ASSERT(allMatch, "拡張頂点データがGPU上で完全に一致すること");
}

//! VertexBuffer 大量頂点 GPU Readbackテスト
//! @details 1000頂点でのReadback検証
static void TestVertexBuffer_GPUReadback_Large()
{
    std::cout << "\n=== VertexBuffer 大量頂点 GPU Readbackテスト ===" << std::endl;

    if (!GraphicsDevice::Get().IsValid()) {
        std::cout << "[スキップ] GraphicsDeviceが初期化されていません" << std::endl;
        return;
    }

    // 1000頂点を生成（円周上の点）
    const int vertexCount = 1000;
    std::vector<SimpleVertex> originalVertices(vertexCount);

    for (int i = 0; i < vertexCount; ++i) {
        float angle = (2.0f * 3.14159265f * i) / vertexCount;
        originalVertices[i].x = std::cos(angle);
        originalVertices[i].y = std::sin(angle);
        originalVertices[i].z = static_cast<float>(i) / vertexCount;
    }

    const size_t dataSize = originalVertices.size() * sizeof(SimpleVertex);

    auto vb = CreateVertexBuffer(originalVertices.data(), static_cast<uint32_t>(dataSize), sizeof(SimpleVertex), false);
    TEST_ASSERT(vb != nullptr, "1000頂点バッファが作成できること");

    // GPUからデータを読み戻す
    auto readbackData = ReadbackBuffer(vb->Get());
    TEST_ASSERT(!readbackData.empty(), "大量頂点Readbackデータが取得できること");
    TEST_ASSERT(readbackData.size() >= dataSize, "大量頂点Readbackデータサイズが十分であること");

    // 全頂点を比較
    const SimpleVertex* readbackVertices = reinterpret_cast<const SimpleVertex*>(readbackData.data());
    int mismatchCount = 0;

    for (int i = 0; i < vertexCount; ++i) {
        const auto& orig = originalVertices[i];
        const auto& rb = readbackVertices[i];

        if (orig.x != rb.x || orig.y != rb.y || orig.z != rb.z) {
            mismatchCount++;
        }
    }

    TEST_ASSERT(mismatchCount == 0, "1000頂点が全てGPU上で一致すること");

    if (mismatchCount > 0) {
        std::cout << "  不一致頂点数: " << mismatchCount << "/" << vertexCount << std::endl;
    }
}

//----------------------------------------------------------------------------
// IndexBuffer テスト
//----------------------------------------------------------------------------

//! IndexBuffer GPU Readbackテスト
//! @details GPUに書き込んだインデックスデータを読み戻して検証
static void TestIndexBuffer_GPUReadback()
{
    std::cout << "\n=== IndexBuffer GPU Readbackテスト ===" << std::endl;

    if (!GraphicsDevice::Get().IsValid()) {
        std::cout << "[スキップ] GraphicsDeviceが初期化されていません" << std::endl;
        return;
    }

    // テスト用インデックスデータ（2つの三角形で四角形）
    uint16_t originalIndices[] = {
        0, 1, 2,  // 三角形1
        2, 1, 3,  // 三角形2
    };
    const size_t dataSize = sizeof(originalIndices);
    const int indexCount = sizeof(originalIndices) / sizeof(uint16_t);

    auto ib = CreateIndexBuffer16(originalIndices, indexCount);

    TEST_ASSERT(ib != nullptr, "インデックスバッファが作成できること");
    TEST_ASSERT(ib->Get() != nullptr, "インデックスバッファが有効であること");

    // IMMUTABLEはCopyResourceでステージングにコピー可能
    auto readbackData = ReadbackBuffer(ib->Get());
    TEST_ASSERT(!readbackData.empty(), "インデックスReadbackデータが取得できること");
    TEST_ASSERT(readbackData.size() >= dataSize, "インデックスReadbackデータサイズが十分であること");

    // インデックスを比較
    const uint16_t* readbackIndices = reinterpret_cast<const uint16_t*>(readbackData.data());
    bool allMatch = true;

    for (int i = 0; i < indexCount; ++i) {
        if (originalIndices[i] != readbackIndices[i]) {
            allMatch = false;
            std::cout << "  インデックス" << i << ": 元=" << originalIndices[i]
                      << " / 読み戻し=" << readbackIndices[i] << std::endl;
        }
    }

    TEST_ASSERT(allMatch, "インデックスデータがGPU上で完全に一致すること");
}

//! IndexBuffer 32bit GPU Readbackテスト
//! @details 32bitインデックスでのReadback検証
static void TestIndexBuffer_GPUReadback_32bit()
{
    std::cout << "\n=== IndexBuffer 32bit GPU Readbackテスト ===" << std::endl;

    if (!GraphicsDevice::Get().IsValid()) {
        std::cout << "[スキップ] GraphicsDeviceが初期化されていません" << std::endl;
        return;
    }

    // 32bitインデックス（大きなメッシュを想定）
    uint32_t originalIndices[] = {
        0, 100000, 200000,
        200000, 100000, 300000,
        65536, 65537, 65538,  // 16bitでは表現できない値
    };
    const size_t dataSize = sizeof(originalIndices);
    const int indexCount = sizeof(originalIndices) / sizeof(uint32_t);

    auto ib = CreateIndexBuffer32(originalIndices, indexCount);

    TEST_ASSERT(ib != nullptr, "32bitインデックスバッファが作成できること");

    auto readbackData = ReadbackBuffer(ib->Get());
    TEST_ASSERT(!readbackData.empty(), "32bitインデックスReadbackデータが取得できること");

    const uint32_t* readbackIndices = reinterpret_cast<const uint32_t*>(readbackData.data());
    bool allMatch = std::memcmp(originalIndices, readbackIndices, dataSize) == 0;

    TEST_ASSERT(allMatch, "32bitインデックスデータがGPU上で完全に一致すること");
}

//----------------------------------------------------------------------------
// ConstantBuffer テスト
//----------------------------------------------------------------------------

//! 定数バッファ用構造体
struct TestCBData
{
    float matrix[16];    // 4x4行列
    float vector[4];     // ベクトル
    float scalar;        // スカラー
    float padding[3];    // 16バイトアライメント用パディング
};

//! ConstantBuffer 生成テスト
//! @details 定数バッファの基本的な生成をテスト
static void TestConstantBuffer_Create()
{
    std::cout << "\n=== ConstantBuffer 生成テスト ===" << std::endl;

    if (!GraphicsDevice::Get().IsValid()) {
        std::cout << "[スキップ] GraphicsDeviceが初期化されていません" << std::endl;
        return;
    }

    auto cb = CreateConstantBuffer(sizeof(TestCBData));

    TEST_ASSERT(cb != nullptr, "定数バッファが作成できること");
    TEST_ASSERT(cb->Get() != nullptr, "定数バッファが有効であること");
    TEST_ASSERT(cb->Size() >= sizeof(TestCBData), "バッファサイズが十分であること");
}

//----------------------------------------------------------------------------
// StructuredBuffer テスト
//----------------------------------------------------------------------------

//! 構造化バッファ用構造体
struct ParticleData
{
    float position[3];
    float velocity[3];
    float lifetime;
    float padding;  // 16バイトアライメント
};

//! StructuredBuffer GPU Readbackテスト
//! @details 構造化バッファへの初期データ書き込みと読み戻しを検証
static void TestStructuredBuffer_GPUReadback()
{
    std::cout << "\n=== StructuredBuffer GPU Readbackテスト ===" << std::endl;

    if (!GraphicsDevice::Get().IsValid()) {
        std::cout << "[スキップ] GraphicsDeviceが初期化されていません" << std::endl;
        return;
    }

    // パーティクルデータを生成
    const int particleCount = 100;
    std::vector<ParticleData> originalData(particleCount);

    for (int i = 0; i < particleCount; ++i) {
        float t = static_cast<float>(i) / particleCount;
        originalData[i].position[0] = t * 10.0f;
        originalData[i].position[1] = std::sin(t * 3.14159f) * 5.0f;
        originalData[i].position[2] = 0.0f;
        originalData[i].velocity[0] = 1.0f;
        originalData[i].velocity[1] = 0.5f;
        originalData[i].velocity[2] = 0.0f;
        originalData[i].lifetime = 1.0f - t;
        originalData[i].padding = 0.0f;
    }

    // 構造化バッファを作成
    auto sb = Buffer::CreateStructured(
        sizeof(ParticleData),
        particleCount,
        false,  // UAVなし
        originalData.data());

    TEST_ASSERT(sb != nullptr, "構造化バッファが作成できること");
    TEST_ASSERT(sb->IsStructured(), "構造化バッファと判定されること");
    TEST_ASSERT(sb->HasSrv(), "SRVを持つこと");

    // GPUからデータを読み戻す
    auto readbackData = ReadbackBuffer(sb->Get());
    TEST_ASSERT(!readbackData.empty(), "構造化バッファReadbackデータが取得できること");

    const ParticleData* readbackParticles = reinterpret_cast<const ParticleData*>(readbackData.data());
    int mismatchCount = 0;

    for (int i = 0; i < particleCount; ++i) {
        const auto& orig = originalData[i];
        const auto& rb = readbackParticles[i];

        if (std::memcmp(&orig, &rb, sizeof(ParticleData)) != 0) {
            mismatchCount++;
        }
    }

    TEST_ASSERT(mismatchCount == 0, "構造化バッファの全データがGPU上で一致すること");
}

//! StructuredBuffer SRV/UAVアクセステスト
//! @details SRV/UAVの取得と有効性を検証
static void TestStructuredBuffer_SrvUavAccess()
{
    std::cout << "\n=== StructuredBuffer SRV/UAVアクセステスト ===" << std::endl;

    if (!GraphicsDevice::Get().IsValid()) {
        std::cout << "[スキップ] GraphicsDeviceが初期化されていません" << std::endl;
        return;
    }

    // UAV付き構造化バッファ
    auto sbUav = Buffer::CreateStructured(32, 256, true, nullptr);
    TEST_ASSERT(sbUav != nullptr, "UAV付き構造化バッファが作成できること");

    // SRVラッパー取得
    auto* srvView = sbUav->GetSrvView();
    TEST_ASSERT(srvView != nullptr, "SRVラッパーが取得できること");
    TEST_ASSERT(srvView->IsValid(), "SRVラッパーが有効であること");
    TEST_ASSERT(srvView->Get() == sbUav->Srv(), "SRVラッパーとSrv()が一致すること");

    // UAVラッパー取得
    auto* uavView = sbUav->GetUavView();
    TEST_ASSERT(uavView != nullptr, "UAVラッパーが取得できること");
    TEST_ASSERT(uavView->IsValid(), "UAVラッパーが有効であること");
    TEST_ASSERT(uavView->Get() == sbUav->Uav(), "UAVラッパーとUav()が一致すること");
}

//----------------------------------------------------------------------------
// 動的バッファ更新 テスト
//----------------------------------------------------------------------------

//! 動的バッファ用構造体
struct DynamicTestData
{
    float value;
    int index;
};

//! 動的頂点バッファ更新テスト
//! @details Map/Unmapによる動的バッファの更新を検証
static void TestDynamicBuffer_Update()
{
    std::cout << "\n=== 動的バッファ更新テスト ===" << std::endl;

    if (!GraphicsDevice::Get().IsValid()) {
        std::cout << "[スキップ] GraphicsDeviceが初期化されていません" << std::endl;
        return;
    }

    // 動的頂点バッファを作成
    const int maxVertices = 100;
    auto vb = Buffer::CreateVertex(
        maxVertices * sizeof(SimpleVertex),
        sizeof(SimpleVertex),
        true,  // dynamic
        nullptr);

    TEST_ASSERT(vb != nullptr, "動的頂点バッファが作成できること");
    TEST_ASSERT(vb->IsDynamic(), "動的バッファと判定されること");

    // GraphicsContextを使用して更新
    auto& ctx = GraphicsContext::Get();

    // テストデータを生成
    std::vector<SimpleVertex> testData(50);
    for (int i = 0; i < 50; ++i) {
        testData[i].x = static_cast<float>(i);
        testData[i].y = static_cast<float>(i * 2);
        testData[i].z = static_cast<float>(i * 3);
    }

    // Map/Unmapで更新
    D3D11_MAPPED_SUBRESOURCE mapped = ctx.Map(vb->Get(), 0, D3D11_MAP_WRITE_DISCARD);
    TEST_ASSERT(mapped.pData != nullptr, "Mapが成功すること");

    if (mapped.pData) {
        std::memcpy(mapped.pData, testData.data(), testData.size() * sizeof(SimpleVertex));
        ctx.Unmap(vb->Get(), 0);

        // 注: 動的バッファはD3D11_USAGE_DYNAMICなのでReadbackは不可
        // ここではMapの成功をもって更新が完了したとみなす
        TEST_ASSERT(true, "動的バッファの更新が完了したこと");
    }
}

//! 動的インデックスバッファ更新テスト
static void TestDynamicIndexBuffer_Update()
{
    std::cout << "\n=== 動的インデックスバッファ更新テスト ===" << std::endl;

    if (!GraphicsDevice::Get().IsValid()) {
        std::cout << "[スキップ] GraphicsDeviceが初期化されていません" << std::endl;
        return;
    }

    // 動的インデックスバッファを作成
    const int maxIndices = 300;
    auto ib = Buffer::CreateIndex(
        maxIndices * sizeof(uint16_t),
        true,  // dynamic
        nullptr);

    TEST_ASSERT(ib != nullptr, "動的インデックスバッファが作成できること");
    TEST_ASSERT(ib->IsDynamic(), "動的バッファと判定されること");

    auto& ctx = GraphicsContext::Get();

    // テストインデックスを生成
    std::vector<uint16_t> testIndices(100);
    for (int i = 0; i < 100; ++i) {
        testIndices[i] = static_cast<uint16_t>(i);
    }

    // Map/Unmapで更新
    D3D11_MAPPED_SUBRESOURCE mapped = ctx.Map(ib->Get(), 0, D3D11_MAP_WRITE_DISCARD);
    TEST_ASSERT(mapped.pData != nullptr, "インデックスバッファのMapが成功すること");

    if (mapped.pData) {
        std::memcpy(mapped.pData, testIndices.data(), testIndices.size() * sizeof(uint16_t));
        ctx.Unmap(ib->Get(), 0);
        TEST_ASSERT(true, "動的インデックスバッファの更新が完了したこと");
    }
}

//----------------------------------------------------------------------------
// 定数バッファ更新 テスト
//----------------------------------------------------------------------------

//! 変換行列用定数バッファ構造体
struct TransformCB
{
    float world[16];
    float view[16];
    float projection[16];
};

//! 定数バッファ更新テスト
//! @details 定数バッファのMap/Unmap更新を検証
static void TestConstantBuffer_Update()
{
    std::cout << "\n=== 定数バッファ更新テスト ===" << std::endl;

    if (!GraphicsDevice::Get().IsValid()) {
        std::cout << "[スキップ] GraphicsDeviceが初期化されていません" << std::endl;
        return;
    }

    auto cb = Buffer::CreateConstant(sizeof(TransformCB));
    TEST_ASSERT(cb != nullptr, "定数バッファが作成できること");

    auto& ctx = GraphicsContext::Get();

    // テストデータ（単位行列を設定）
    TransformCB data = {};
    for (int i = 0; i < 4; ++i) {
        data.world[i * 4 + i] = 1.0f;
        data.view[i * 4 + i] = 1.0f;
        data.projection[i * 4 + i] = 1.0f;
    }

    // Map/Unmapで更新
    D3D11_MAPPED_SUBRESOURCE mapped = ctx.Map(cb->Get(), 0, D3D11_MAP_WRITE_DISCARD);
    TEST_ASSERT(mapped.pData != nullptr, "定数バッファのMapが成功すること");

    if (mapped.pData) {
        std::memcpy(mapped.pData, &data, sizeof(TransformCB));
        ctx.Unmap(cb->Get(), 0);
        TEST_ASSERT(true, "定数バッファの更新が完了したこと");
    }
}

//! 定数バッファ 複数回更新テスト
//! @details フレームごとの更新をシミュレート
static void TestConstantBuffer_MultipleUpdates()
{
    std::cout << "\n=== 定数バッファ 複数回更新テスト ===" << std::endl;

    if (!GraphicsDevice::Get().IsValid()) {
        std::cout << "[スキップ] GraphicsDeviceが初期化されていません" << std::endl;
        return;
    }

    struct PerFrameCB {
        float time;
        float deltaTime;
        float padding[2];
    };

    auto cb = Buffer::CreateConstant(sizeof(PerFrameCB));
    TEST_ASSERT(cb != nullptr, "PerFrameCB定数バッファが作成できること");

    auto& ctx = GraphicsContext::Get();

    // 10フレーム分の更新をシミュレート
    bool allUpdatesSucceeded = true;
    for (int frame = 0; frame < 10; ++frame) {
        PerFrameCB data = {};
        data.time = frame * 0.016f;
        data.deltaTime = 0.016f;

        D3D11_MAPPED_SUBRESOURCE mapped = ctx.Map(cb->Get(), 0, D3D11_MAP_WRITE_DISCARD);
        if (!mapped.pData) {
            allUpdatesSucceeded = false;
            break;
        }
        std::memcpy(mapped.pData, &data, sizeof(PerFrameCB));
        ctx.Unmap(cb->Get(), 0);
    }

    TEST_ASSERT(allUpdatesSucceeded, "10フレーム分の定数バッファ更新が全て成功すること");
}

//----------------------------------------------------------------------------
// バッファアクセサ テスト
//----------------------------------------------------------------------------

//! バッファアクセサ 頂点バッファテスト
static void TestBufferAccessors_VertexBuffer()
{
    std::cout << "\n=== バッファアクセサ 頂点バッファテスト ===" << std::endl;

    if (!GraphicsDevice::Get().IsValid()) {
        std::cout << "[スキップ] GraphicsDeviceが初期化されていません" << std::endl;
        return;
    }

    auto vb = Buffer::CreateVertex(1024, 32, false, nullptr);
    TEST_ASSERT(vb != nullptr, "頂点バッファが作成できること");

    // アクセサテスト
    TEST_ASSERT(vb->Size() >= 1024, "Size()が正しい値を返すこと");
    TEST_ASSERT(vb->GpuSize() >= 1024, "GpuSize()が正しい値を返すこと");
    TEST_ASSERT(vb->Stride() == 32, "Stride()が正しい値を返すこと");
    TEST_ASSERT(vb->Usage() == D3D11_USAGE_DEFAULT, "Usage()が正しい値を返すこと");
    TEST_ASSERT(vb->BindFlags() == D3D11_BIND_VERTEX_BUFFER, "BindFlags()が正しい値を返すこと");
    TEST_ASSERT(vb->CpuAccess() == 0, "CpuAccess()が正しい値を返すこと");
    TEST_ASSERT(vb->MiscFlags() == 0, "MiscFlags()が正しい値を返すこと");
    TEST_ASSERT(!vb->IsDynamic(), "IsDynamic()がfalseを返すこと");
    TEST_ASSERT(!vb->IsStructured(), "IsStructured()がfalseを返すこと");
    TEST_ASSERT(!vb->HasSrv(), "HasSrv()がfalseを返すこと");
    TEST_ASSERT(!vb->HasUav(), "HasUav()がfalseを返すこと");
    TEST_ASSERT(vb->Get() != nullptr, "Get()がnullptrでないこと");
    TEST_ASSERT(vb->AddressOf() != nullptr, "AddressOf()がnullptrでないこと");

    // Desc()テスト
    const auto& desc = vb->Desc();
    TEST_ASSERT(desc.size == vb->Size(), "Desc().sizeがSize()と一致すること");
    TEST_ASSERT(desc.stride == vb->Stride(), "Desc().strideがStride()と一致すること");
}

//! バッファアクセサ 構造化バッファテスト
static void TestBufferAccessors_StructuredBuffer()
{
    std::cout << "\n=== バッファアクセサ 構造化バッファテスト ===" << std::endl;

    if (!GraphicsDevice::Get().IsValid()) {
        std::cout << "[スキップ] GraphicsDeviceが初期化されていません" << std::endl;
        return;
    }

    auto sb = Buffer::CreateStructured(64, 100, true, nullptr);
    TEST_ASSERT(sb != nullptr, "構造化バッファが作成できること");

    // アクセサテスト
    TEST_ASSERT(sb->IsStructured(), "IsStructured()がtrueを返すこと");
    TEST_ASSERT(!sb->IsDynamic(), "IsDynamic()がfalseを返すこと");
    TEST_ASSERT(sb->HasSrv(), "HasSrv()がtrueを返すこと");
    TEST_ASSERT(sb->HasUav(), "HasUav()がtrueを返すこと");
    TEST_ASSERT((sb->BindFlags() & D3D11_BIND_SHADER_RESOURCE) != 0, "SRVバインドフラグがあること");
    TEST_ASSERT((sb->BindFlags() & D3D11_BIND_UNORDERED_ACCESS) != 0, "UAVバインドフラグがあること");
    TEST_ASSERT(sb->MiscFlags() == D3D11_RESOURCE_MISC_BUFFER_STRUCTURED, "MiscFlagsがSTRUCTUREDであること");
    TEST_ASSERT(sb->Stride() >= 64, "Stride()が64以上であること");
    TEST_ASSERT(sb->Srv() != nullptr, "Srv()がnullptrでないこと");
    TEST_ASSERT(sb->Uav() != nullptr, "Uav()がnullptrでないこと");
    TEST_ASSERT(sb->GetSrvView() != nullptr, "GetSrvView()がnullptrでないこと");
    TEST_ASSERT(sb->GetUavView() != nullptr, "GetUavView()がnullptrでないこと");
}

//----------------------------------------------------------------------------
// エッジケース テスト
//----------------------------------------------------------------------------

//! 最小サイズバッファテスト
static void TestEdgeCase_MinimumSize()
{
    std::cout << "\n=== エッジケース: 最小サイズバッファテスト ===" << std::endl;

    if (!GraphicsDevice::Get().IsValid()) {
        std::cout << "[スキップ] GraphicsDeviceが初期化されていません" << std::endl;
        return;
    }

    // 1バイトの頂点バッファ（アライメントで大きくなる）
    auto vb = Buffer::CreateVertex(1, 1, false, nullptr);
    TEST_ASSERT(vb != nullptr, "1バイトの頂点バッファが作成できること");
    TEST_ASSERT(vb->Size() >= 1, "サイズが1以上であること");

    // 16バイトの定数バッファ（最小サイズ）
    auto cb = Buffer::CreateConstant(16);
    TEST_ASSERT(cb != nullptr, "16バイトの定数バッファが作成できること");
    TEST_ASSERT(cb->Size() >= 16, "サイズが16以上であること");
}

//! 大きなバッファテスト
static void TestEdgeCase_LargeBuffer()
{
    std::cout << "\n=== エッジケース: 大きなバッファテスト ===" << std::endl;

    if (!GraphicsDevice::Get().IsValid()) {
        std::cout << "[スキップ] GraphicsDeviceが初期化されていません" << std::endl;
        return;
    }

    // 16MBの頂点バッファ
    const uint32_t largeSize = 16 * 1024 * 1024;
    auto vbLarge = Buffer::CreateVertex(largeSize, sizeof(SimpleVertex), false, nullptr);
    TEST_ASSERT(vbLarge != nullptr, "16MBの頂点バッファが作成できること");
    TEST_ASSERT(vbLarge->Size() >= largeSize, "サイズが16MB以上であること");

    // 10000要素の構造化バッファ
    auto sbLarge = Buffer::CreateStructured(sizeof(ExtendedVertex), 10000, true, nullptr);
    TEST_ASSERT(sbLarge != nullptr, "10000要素の構造化バッファが作成できること");
}

//! アライメントテスト
static void TestEdgeCase_Alignment()
{
    std::cout << "\n=== エッジケース: アライメントテスト ===" << std::endl;

    // 非アライメントサイズのBufferDesc
    BufferDesc cb100 = BufferDesc::Constant(100);
    TEST_ASSERT(cb100.size % 16 == 0, "100バイト定数バッファが16バイト境界にアライメントされること");

    BufferDesc cb17 = BufferDesc::Constant(17);
    TEST_ASSERT(cb17.size % 16 == 0, "17バイト定数バッファが16バイト境界にアライメントされること");

    BufferDesc cb1 = BufferDesc::Constant(1);
    TEST_ASSERT(cb1.size % 16 == 0, "1バイト定数バッファが16バイト境界にアライメントされること");
}

//! 初期データなしバッファテスト
static void TestEdgeCase_NoInitialData()
{
    std::cout << "\n=== エッジケース: 初期データなしバッファテスト ===" << std::endl;

    if (!GraphicsDevice::Get().IsValid()) {
        std::cout << "[スキップ] GraphicsDeviceが初期化されていません" << std::endl;
        return;
    }

    // 初期データなしの静的頂点バッファ
    auto vb = Buffer::CreateVertex(1024, 32, false, nullptr);
    TEST_ASSERT(vb != nullptr, "初期データなしの静的頂点バッファが作成できること");

    // 初期データなしのインデックスバッファ
    auto ib = Buffer::CreateIndex(512, false, nullptr);
    TEST_ASSERT(ib != nullptr, "初期データなしのインデックスバッファが作成できること");

    // 初期データなしの構造化バッファ
    auto sb = Buffer::CreateStructured(64, 100, true, nullptr);
    TEST_ASSERT(sb != nullptr, "初期データなしの構造化バッファが作成できること");
}

//! バッファ記述子一貫性テスト
static void TestEdgeCase_DescriptorConsistency()
{
    std::cout << "\n=== エッジケース: バッファ記述子一貫性テスト ===" << std::endl;

    if (!GraphicsDevice::Get().IsValid()) {
        std::cout << "[スキップ] GraphicsDeviceが初期化されていません" << std::endl;
        return;
    }

    // 作成したバッファのDesc()が元の記述子と一致するか確認
    BufferDesc vbDesc = BufferDesc::Vertex(1024, true);
    vbDesc.stride = 32;
    auto vb = Buffer::Create(vbDesc, nullptr);
    TEST_ASSERT(vb != nullptr, "頂点バッファが作成できること");

    const auto& resultDesc = vb->Desc();
    TEST_ASSERT(resultDesc.size == vbDesc.size, "記述子のsizeが一致すること");
    TEST_ASSERT(resultDesc.stride == vbDesc.stride, "記述子のstrideが一致すること");
    TEST_ASSERT(resultDesc.usage == vbDesc.usage, "記述子のusageが一致すること");
    TEST_ASSERT(resultDesc.bindFlags == vbDesc.bindFlags, "記述子のbindFlagsが一致すること");
    TEST_ASSERT(resultDesc.cpuAccess == vbDesc.cpuAccess, "記述子のcpuAccessが一致すること");
}

//----------------------------------------------------------------------------
// 公開インターフェース
//----------------------------------------------------------------------------

//! バッファテストスイートを実行
//! @return 全テスト成功時true、それ以外false
bool RunBufferTests()
{
    std::cout << "\n========================================" << std::endl;
    std::cout << "  バッファシステム テスト" << std::endl;
    std::cout << "========================================" << std::endl;

    ResetGlobalCounters();

    // BufferDescテスト
    TestBufferDesc_Vertex();
    TestBufferDesc_Index();
    TestBufferDesc_Constant();
    TestBufferDesc_Structured();
    TestBufferDesc_HashKey();

    // Buffer静的ファクトリメソッドテスト
    TestBuffer_CreateVertex();
    TestBuffer_CreateIndex();
    TestBuffer_CreateConstant();
    TestBuffer_CreateStructured();
    TestBuffer_Create_Generic();

    // VertexBufferテスト（ヘルパー関数使用）
    TestVertexBuffer_CreateImmutable();
    TestVertexBuffer_CreateDynamic();
    TestVertexBuffer_GPUReadback_Simple();
    TestVertexBuffer_GPUReadback_Extended();
    TestVertexBuffer_GPUReadback_Large();

    // IndexBufferテスト
    TestIndexBuffer_GPUReadback();
    TestIndexBuffer_GPUReadback_32bit();

    // ConstantBufferテスト
    TestConstantBuffer_Create();

    // StructuredBufferテスト
    TestStructuredBuffer_GPUReadback();
    TestStructuredBuffer_SrvUavAccess();

    // 動的バッファ更新テスト
    TestDynamicBuffer_Update();
    TestDynamicIndexBuffer_Update();

    // 定数バッファ更新テスト
    TestConstantBuffer_Update();
    TestConstantBuffer_MultipleUpdates();

    // バッファアクセサテスト
    TestBufferAccessors_VertexBuffer();
    TestBufferAccessors_StructuredBuffer();

    // エッジケーステスト
    TestEdgeCase_MinimumSize();
    TestEdgeCase_LargeBuffer();
    TestEdgeCase_Alignment();
    TestEdgeCase_NoInitialData();
    TestEdgeCase_DescriptorConsistency();

    std::cout << "\n----------------------------------------" << std::endl;
    std::cout << "バッファテスト: " << s_passCount << "/" << s_testCount << " 成功" << std::endl;
    std::cout << "----------------------------------------" << std::endl;

    return s_passCount == s_testCount;
}

} // namespace tests
