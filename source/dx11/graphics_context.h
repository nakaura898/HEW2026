//----------------------------------------------------------------------------
//! @file   graphics_context.h
//! @brief  グラフィックスコンテキスト
//----------------------------------------------------------------------------
#pragma once

#include "dx11/gpu_common.h"
#include "dx11/gpu/gpu.h"

// 既存クラス（state/）
class BlendState;
class DepthStencilState;
class RasterizerState;
class SamplerState;

//===========================================================================
//! グラフィックスコンテキスト
//! @brief Immediate Contextのラッパー
//===========================================================================
class GraphicsContext final : private NonCopyableNonMovable
{
public:
    //! インスタンスを取得
    static GraphicsContext& Get() noexcept;

    //! 初期化（GraphicsDeviceから即時コンテキストを取得）
    [[nodiscard]] bool Initialize() noexcept;

    //! 終了処理
    void Shutdown() noexcept;

    //----------------------------------------------------------
    //! @name   描画コマンド
    //----------------------------------------------------------
    //! @{

    void Draw(uint32_t vertexCount, uint32_t startVertexLocation = 0);
    void DrawIndexed(uint32_t indexCount, uint32_t startIndexLocation = 0, int32_t baseVertexLocation = 0);
    void DrawInstanced(uint32_t vertexCountPerInstance, uint32_t instanceCount,
                       uint32_t startVertexLocation = 0, uint32_t startInstanceLocation = 0);
    void DrawIndexedInstanced(uint32_t indexCountPerInstance, uint32_t instanceCount,
                              uint32_t startIndexLocation = 0, int32_t baseVertexLocation = 0,
                              uint32_t startInstanceLocation = 0);
    void DrawInstancedIndirect(Buffer* argsBuffer, uint32_t alignedByteOffsetForArgs = 0);
    void DrawIndexedInstancedIndirect(Buffer* argsBuffer, uint32_t alignedByteOffsetForArgs = 0);

    //!@}
    //----------------------------------------------------------
    //! @name   コンピュートシェーダー実行
    //----------------------------------------------------------
    //!@{

    void Dispatch(uint32_t threadGroupCountX, uint32_t threadGroupCountY, uint32_t threadGroupCountZ);
    void DispatchIndirect(Buffer* argsBuffer, uint32_t alignedByteOffsetForArgs = 0);

    //!@}
    //----------------------------------------------------------
    //! @name   入力アセンブラ
    //----------------------------------------------------------
    //!@{

    void SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY topology);
    void SetInputLayout(ID3D11InputLayout* inputLayout);

    //!@}
    //----------------------------------------------------------
    //! @name   クリア系
    //----------------------------------------------------------
    //!@{

    void ClearRenderTarget(Texture* target, const float color[4]);
    void ClearDepthStencil(Texture* depthStencil, float depth = 1.0f, uint8_t stencil = 0);
    void ClearUnorderedAccessViewUint(ID3D11UnorderedAccessView* uav, const uint32_t values[4]);
    void ClearUnorderedAccessViewFloat(ID3D11UnorderedAccessView* uav, const float values[4]);

    //!@}
    //----------------------------------------------------------
    //! @name   レンダーターゲット設定
    //----------------------------------------------------------
    //!@{

    void SetRenderTarget(Texture* renderTarget, Texture* depthStencil = nullptr);
    void SetRenderTargets(uint32_t count, Texture* const* renderTargets, Texture* depthStencil = nullptr);
    void SetRenderTargetsAndUnorderedAccessViews(
        uint32_t numRTVs, Texture* const* renderTargets, Texture* depthStencil,
        uint32_t uavStartSlot, uint32_t numUAVs,
        ID3D11UnorderedAccessView* const* uavs, const uint32_t* uavInitialCounts = nullptr);

    //!@}
    //----------------------------------------------------------
    //! @name   ビューポート・シザー
    //----------------------------------------------------------
    //!@{

    void SetViewport(float x, float y, float width, float height, float minDepth = 0.0f, float maxDepth = 1.0f);
    void SetViewports(uint32_t count, const D3D11_VIEWPORT* viewports);
    void SetScissorRect(int32_t left, int32_t top, int32_t right, int32_t bottom);
    void SetScissorRects(uint32_t count, const D3D11_RECT* rects);

    //!@}
    //----------------------------------------------------------
    //! @name   頂点・インデックスバッファ（新gpu/）
    //----------------------------------------------------------
    //!@{

    //! 頂点バッファを設定
    void SetVertexBuffer(uint32_t slot, Buffer* buffer, uint32_t stride, uint32_t offset = 0);
    void SetVertexBuffers(uint32_t startSlot, uint32_t count,
                          ID3D11Buffer* const* buffers, const uint32_t* strides, const uint32_t* offsets);

    //! インデックスバッファを設定
    void SetIndexBuffer(Buffer* buffer, DXGI_FORMAT format, uint32_t offset = 0);

    //!@}
    //----------------------------------------------------------
    //! @name   ストリーム出力
    //----------------------------------------------------------
    //!@{

    void SetStreamOutputTargets(uint32_t count, Buffer* const* buffers, const uint32_t* offsets);

    //!@}
    //----------------------------------------------------------
    //! @name   バッファ更新
    //----------------------------------------------------------
    //!@{

    //! バッファデータを更新（全体）
    void UpdateBuffer(Buffer* buffer, const void* data);

    //! バッファデータを更新（サイズ指定）
    void UpdateBuffer(Buffer* buffer, const void* data, uint32_t sizeInBytes);

    //! バッファデータを更新（部分）
    void UpdateBuffer(Buffer* buffer, const void* data, uint32_t sizeInBytes, uint32_t offsetInBytes);

    //! バッファをマップ
    [[nodiscard]] void* MapBuffer(Buffer* buffer, D3D11_MAP mapType = D3D11_MAP_WRITE_DISCARD);

    //! バッファをアンマップ
    void UnmapBuffer(Buffer* buffer);

    //! 定数バッファを更新
    void UpdateConstantBuffer(Buffer* buffer, const void* data, uint32_t sizeInBytes);

    //! 定数バッファを更新（テンプレート）
    template<typename T>
    void UpdateConstantBuffer(Buffer* buffer, const T& data)
    {
        UpdateConstantBuffer(buffer, &data, sizeof(T));
    }

    //!@}
    //----------------------------------------------------------
    //! @name   定数バッファ（新gpu/）
    //----------------------------------------------------------
    //!@{

    void SetVSConstantBuffer(uint32_t slot, Buffer* buffer);
    void SetPSConstantBuffer(uint32_t slot, Buffer* buffer);
    void SetGSConstantBuffer(uint32_t slot, Buffer* buffer);
    void SetHSConstantBuffer(uint32_t slot, Buffer* buffer);
    void SetDSConstantBuffer(uint32_t slot, Buffer* buffer);
    void SetCSConstantBuffer(uint32_t slot, Buffer* buffer);

    //!@}
    //----------------------------------------------------------
    //! @name   シェーダーリソース（SRV直接指定）
    //----------------------------------------------------------
    //!@{

    void SetVSShaderResourceView(uint32_t slot, ID3D11ShaderResourceView* srv);
    void SetPSShaderResourceView(uint32_t slot, ID3D11ShaderResourceView* srv);
    void SetGSShaderResourceView(uint32_t slot, ID3D11ShaderResourceView* srv);
    void SetHSShaderResourceView(uint32_t slot, ID3D11ShaderResourceView* srv);
    void SetDSShaderResourceView(uint32_t slot, ID3D11ShaderResourceView* srv);
    void SetCSShaderResourceView(uint32_t slot, ID3D11ShaderResourceView* srv);

    //!@}
    //----------------------------------------------------------
    //! @name   シェーダーリソース（Texture）
    //----------------------------------------------------------
    //!@{

    void SetVSShaderResource(uint32_t slot, Texture* texture);
    void SetPSShaderResource(uint32_t slot, Texture* texture);
    void SetGSShaderResource(uint32_t slot, Texture* texture);
    void SetHSShaderResource(uint32_t slot, Texture* texture);
    void SetDSShaderResource(uint32_t slot, Texture* texture);
    void SetCSShaderResource(uint32_t slot, Texture* texture);

    //!@}
    //----------------------------------------------------------
    //! @name   シェーダーリソース（Buffer）
    //----------------------------------------------------------
    //!@{

    //! 構造化バッファ/RawバッファのSRVを設定
    void SetVSShaderResource(uint32_t slot, Buffer* buffer);
    void SetPSShaderResource(uint32_t slot, Buffer* buffer);
    void SetGSShaderResource(uint32_t slot, Buffer* buffer);
    void SetHSShaderResource(uint32_t slot, Buffer* buffer);
    void SetDSShaderResource(uint32_t slot, Buffer* buffer);
    void SetCSShaderResource(uint32_t slot, Buffer* buffer);

    //!@}
    //----------------------------------------------------------
    //! @name   サンプラー
    //----------------------------------------------------------
    //!@{

    void SetVSSampler(uint32_t slot, SamplerState* sampler);
    void SetPSSampler(uint32_t slot, SamplerState* sampler);
    void SetGSSampler(uint32_t slot, SamplerState* sampler);
    void SetHSSampler(uint32_t slot, SamplerState* sampler);
    void SetDSSampler(uint32_t slot, SamplerState* sampler);
    void SetCSSampler(uint32_t slot, SamplerState* sampler);

    //!@}
    //----------------------------------------------------------
    //! @name   パイプラインステート
    //----------------------------------------------------------
    //!@{

    void SetBlendState(BlendState* state, const float blendFactor[4] = nullptr, uint32_t sampleMask = 0xFFFFFFFF);
    void SetDepthStencilState(DepthStencilState* state, uint32_t stencilRef = 0);
    void SetRasterizerState(RasterizerState* state);

    //!@}
    //----------------------------------------------------------
    //! @name   シェーダー設定（新gpu/）
    //----------------------------------------------------------
    //!@{

    void SetVertexShader(Shader* shader);
    void SetPixelShader(Shader* shader);
    void SetGeometryShader(Shader* shader);
    void SetHullShader(Shader* shader);
    void SetDomainShader(Shader* shader);
    void SetComputeShader(Shader* shader);

    //!@}
    //----------------------------------------------------------
    //! @name   UAV設定
    //----------------------------------------------------------
    //!@{

    //! CS UAV設定（テクスチャ）
    void SetCSUnorderedAccessView(uint32_t slot, Texture* texture);

    //! CS UAV設定（バッファ）
    void SetCSUnorderedAccessView(uint32_t slot, Buffer* buffer, uint32_t initialCount = 0xFFFFFFFF);

    //! CS UAV設定（直接指定）
    void SetCSUnorderedAccessViewDirect(uint32_t slot, ID3D11UnorderedAccessView* uav, uint32_t initialCount = 0xFFFFFFFF);

    //!@}
    //----------------------------------------------------------
    //! @name   カウンター操作
    //----------------------------------------------------------
    //!@{

    void CopyStructureCount(Buffer* destBuffer, uint32_t destAlignedByteOffset, ID3D11UnorderedAccessView* srcUAV);

    //!@}
    //----------------------------------------------------------
    //! @name   リソースコピー
    //----------------------------------------------------------
    //!@{

    void CopyResource(ID3D11Resource* dest, ID3D11Resource* src);
    void UpdateSubresource(ID3D11Resource* dest, uint32_t destSubresource,
                           const D3D11_BOX* destBox, const void* srcData,
                           uint32_t srcRowPitch = 0, uint32_t srcDepthPitch = 0);

    //!@}
    //----------------------------------------------------------
    //! @name   低レベルMap/Unmap
    //----------------------------------------------------------
    //!@{

    [[nodiscard]] D3D11_MAPPED_SUBRESOURCE Map(ID3D11Resource* resource, uint32_t subresource, D3D11_MAP mapType);
    void Unmap(ID3D11Resource* resource, uint32_t subresource);

    //!@}
    //----------------------------------------------------------
    //! @name   コンテキスト取得
    //----------------------------------------------------------
    //!@{

    [[nodiscard]] ID3D11DeviceContext4* GetContext() const noexcept { return context_.Get(); }

    //!@}

private:
    GraphicsContext() = default;
    ~GraphicsContext() = default;

    ComPtr<ID3D11DeviceContext4> context_;
};
