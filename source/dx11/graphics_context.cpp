//----------------------------------------------------------------------------
//! @file   graphics_context.cpp
//! @brief  グラフィックスコンテキスト実装
//----------------------------------------------------------------------------
#include "dx11/graphics_context.h"
#include "dx11/graphics_device.h"
#include "dx11/state/blend_state.h"
#include "dx11/state/depth_stencil_state.h"
#include "dx11/state/rasterizer_state.h"
#include "dx11/state/sampler_state.h"
#include "common/logging/logging.h"
#include <cstring>

//===========================================================================
// インスタンス取得
//===========================================================================
GraphicsContext& GraphicsContext::Get() noexcept
{
    static GraphicsContext instance;
    return instance;
}

//===========================================================================
// 初期化
//===========================================================================
bool GraphicsContext::Initialize() noexcept
{
    auto* device = GraphicsDevice::Get().Device();
    if (!device) {
        return false;
    }

    ComPtr<ID3D11DeviceContext> context;
    device->GetImmediateContext(&context);
    if (!context) {
        return false;
    }

    HRESULT hr = context.As(&context_);
    if (FAILED(hr)) {
        return false;
    }

    return true;
}

//===========================================================================
// 終了処理
//===========================================================================
void GraphicsContext::Shutdown() noexcept
{
    if (context_) {
        context_->ClearState();  // パイプラインから全状態をアンバインド
        context_->Flush();       // 保留中のコマンドをフラッシュ
    }
    context_.Reset();
    ResetStateCache();
}

//===========================================================================
// ステートキャッシュリセット
//===========================================================================
void GraphicsContext::ResetStateCache() noexcept
{
    cachedBlendState_ = nullptr;
    cachedDepthStencilState_ = nullptr;
    cachedStencilRef_ = 0;
    cachedRasterizerState_ = nullptr;
    cachedPSSampler0_ = nullptr;
    cachedVS_ = nullptr;
    cachedPS_ = nullptr;
    cachedInputLayout_ = nullptr;
    cachedTopology_ = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
}

//===========================================================================
// 描画コマンド
//===========================================================================
void GraphicsContext::Draw(uint32_t vertexCount, uint32_t startVertexLocation)
{
    auto* ctx = context_.Get();
    if (!ctx) return;
    ctx->Draw(vertexCount, startVertexLocation);
}

void GraphicsContext::DrawIndexed(uint32_t indexCount, uint32_t startIndexLocation, int32_t baseVertexLocation)
{
    auto* ctx = context_.Get();
    if (!ctx) return;
    ctx->DrawIndexed(indexCount, startIndexLocation, baseVertexLocation);
}

void GraphicsContext::DrawInstanced(uint32_t vertexCountPerInstance, uint32_t instanceCount,
                                    uint32_t startVertexLocation, uint32_t startInstanceLocation)
{
    auto* ctx = context_.Get();
    if (!ctx) return;
    ctx->DrawInstanced(vertexCountPerInstance, instanceCount, startVertexLocation, startInstanceLocation);
}

void GraphicsContext::DrawIndexedInstanced(uint32_t indexCountPerInstance, uint32_t instanceCount,
                                           uint32_t startIndexLocation, int32_t baseVertexLocation,
                                           uint32_t startInstanceLocation)
{
    auto* ctx = context_.Get();
    if (!ctx) return;
    ctx->DrawIndexedInstanced(indexCountPerInstance, instanceCount, startIndexLocation,
                              baseVertexLocation, startInstanceLocation);
}

//===========================================================================
// 間接描画
//===========================================================================
void GraphicsContext::DrawInstancedIndirect(Buffer* argsBuffer, uint32_t alignedByteOffsetForArgs)
{
    auto* ctx = context_.Get();
    if (!ctx || !argsBuffer) return;
    ctx->DrawInstancedIndirect(argsBuffer->Get(), alignedByteOffsetForArgs);
}

void GraphicsContext::DrawIndexedInstancedIndirect(Buffer* argsBuffer, uint32_t alignedByteOffsetForArgs)
{
    auto* ctx = context_.Get();
    if (!ctx || !argsBuffer) return;
    ctx->DrawIndexedInstancedIndirect(argsBuffer->Get(), alignedByteOffsetForArgs);
}

//===========================================================================
// コンピュートシェーダー実行
//===========================================================================
void GraphicsContext::Dispatch(uint32_t threadGroupCountX, uint32_t threadGroupCountY, uint32_t threadGroupCountZ)
{
    auto* ctx = context_.Get();
    if (!ctx) return;
    ctx->Dispatch(threadGroupCountX, threadGroupCountY, threadGroupCountZ);
}

void GraphicsContext::DispatchIndirect(Buffer* argsBuffer, uint32_t alignedByteOffsetForArgs)
{
    auto* ctx = context_.Get();
    if (!ctx || !argsBuffer) return;
    ctx->DispatchIndirect(argsBuffer->Get(), alignedByteOffsetForArgs);
}

//===========================================================================
// 入力アセンブラ
//===========================================================================
void GraphicsContext::SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY topology)
{
    if (cachedTopology_ == topology) return;
    cachedTopology_ = topology;

    auto* ctx = context_.Get();
    if (!ctx) return;
    ctx->IASetPrimitiveTopology(topology);
}

void GraphicsContext::SetInputLayout(ID3D11InputLayout* inputLayout)
{
    if (cachedInputLayout_ == inputLayout) return;
    cachedInputLayout_ = inputLayout;

    auto* ctx = context_.Get();
    if (!ctx) return;
    ctx->IASetInputLayout(inputLayout);
}

//===========================================================================
// クリア系
//===========================================================================
void GraphicsContext::ClearRenderTarget(Texture* target, const float color[4])
{
    auto* ctx = context_.Get();
    if (!ctx || !target) return;

    auto* rtv = target->Rtv();
    if (!rtv) return;
    ctx->ClearRenderTargetView(rtv, color);
}

void GraphicsContext::ClearDepthStencil(Texture* depthStencil, float depth, uint8_t stencil)
{
    auto* ctx = context_.Get();
    if (!ctx || !depthStencil) return;

    auto* dsv = depthStencil->Dsv();
    if (!dsv) return;
    ctx->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, depth, stencil);
}

void GraphicsContext::ClearUnorderedAccessViewUint(ID3D11UnorderedAccessView* uav, const uint32_t values[4])
{
    auto* ctx = context_.Get();
    if (!ctx || !uav) return;
    ctx->ClearUnorderedAccessViewUint(uav, values);
}

void GraphicsContext::ClearUnorderedAccessViewFloat(ID3D11UnorderedAccessView* uav, const float values[4])
{
    auto* ctx = context_.Get();
    if (!ctx || !uav) return;
    ctx->ClearUnorderedAccessViewFloat(uav, values);
}

//===========================================================================
// レンダーターゲット設定
//===========================================================================
void GraphicsContext::SetRenderTarget(Texture* renderTarget, Texture* depthStencil)
{
    auto* ctx = context_.Get();
    if (!ctx) return;

    // RTV/DSVサイズ不一致を検出（D3D11エラーの原因）
    assert((!renderTarget || !depthStencil ||
            (renderTarget->Width() == depthStencil->Width() &&
             renderTarget->Height() == depthStencil->Height()))
           && "RTV/DSV size mismatch! RenderTarget and DepthStencil must have the same dimensions.");

    ID3D11RenderTargetView* rtv = renderTarget ? renderTarget->Rtv() : nullptr;
    ID3D11DepthStencilView* dsv = depthStencil ? depthStencil->Dsv() : nullptr;
    ctx->OMSetRenderTargets(renderTarget ? 1 : 0, &rtv, dsv);
}

void GraphicsContext::SetRenderTargets(uint32_t count, Texture* const* renderTargets, Texture* depthStencil)
{
    auto* ctx = context_.Get();
    if (!ctx) return;

    // RTV/DSVサイズ不一致を検出
    if (depthStencil && renderTargets) {
        for (uint32_t i = 0; i < count; ++i) {
            if (renderTargets[i]) {
                assert((renderTargets[i]->Width() == depthStencil->Width() &&
                        renderTargets[i]->Height() == depthStencil->Height())
                       && "RTV/DSV size mismatch!");
            }
        }
    }

    ID3D11RenderTargetView* rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
    for (uint32_t i = 0; i < count && i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i) {
        rtvs[i] = (renderTargets && renderTargets[i]) ? renderTargets[i]->Rtv() : nullptr;
    }
    ID3D11DepthStencilView* dsv = depthStencil ? depthStencil->Dsv() : nullptr;
    ctx->OMSetRenderTargets(count, rtvs, dsv);
}

void GraphicsContext::SetRenderTargetsAndUnorderedAccessViews(
    uint32_t numRTVs, Texture* const* renderTargets, Texture* depthStencil,
    uint32_t uavStartSlot, uint32_t numUAVs,
    ID3D11UnorderedAccessView* const* uavs, const uint32_t* uavInitialCounts)
{
    auto* ctx = context_.Get();
    if (!ctx) return;

    ID3D11RenderTargetView* rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT] = {};
    for (uint32_t i = 0; i < numRTVs && i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i) {
        rtvs[i] = (renderTargets && renderTargets[i]) ? renderTargets[i]->Rtv() : nullptr;
    }
    ID3D11DepthStencilView* dsv = depthStencil ? depthStencil->Dsv() : nullptr;
    ctx->OMSetRenderTargetsAndUnorderedAccessViews(numRTVs, rtvs, dsv, uavStartSlot, numUAVs, uavs, uavInitialCounts);
}

//===========================================================================
// ビューポート・シザー
//===========================================================================
void GraphicsContext::SetViewport(float x, float y, float width, float height, float minDepth, float maxDepth)
{
    auto* ctx = context_.Get();
    if (!ctx) return;

    D3D11_VIEWPORT viewport = {};
    viewport.TopLeftX = x;
    viewport.TopLeftY = y;
    viewport.Width = width;
    viewport.Height = height;
    viewport.MinDepth = minDepth;
    viewport.MaxDepth = maxDepth;
    ctx->RSSetViewports(1, &viewport);
}

void GraphicsContext::SetViewports(uint32_t count, const D3D11_VIEWPORT* viewports)
{
    auto* ctx = context_.Get();
    if (!ctx) return;
    ctx->RSSetViewports(count, viewports);
}

void GraphicsContext::SetScissorRect(int32_t left, int32_t top, int32_t right, int32_t bottom)
{
    auto* ctx = context_.Get();
    if (!ctx) return;

    D3D11_RECT rect = { left, top, right, bottom };
    ctx->RSSetScissorRects(1, &rect);
}

void GraphicsContext::SetScissorRects(uint32_t count, const D3D11_RECT* rects)
{
    auto* ctx = context_.Get();
    if (!ctx) return;
    ctx->RSSetScissorRects(count, rects);
}

//===========================================================================
// 頂点・インデックスバッファ（Buffer）
//===========================================================================
void GraphicsContext::SetVertexBuffer(uint32_t slot, Buffer* buffer, uint32_t stride, uint32_t offset)
{
    auto* ctx = context_.Get();
    if (!ctx) return;

    ID3D11Buffer* buffers[] = { buffer ? buffer->Get() : nullptr };
    UINT strides[] = { stride };
    UINT offsets[] = { offset };
    ctx->IASetVertexBuffers(slot, 1, buffers, strides, offsets);
}

void GraphicsContext::SetVertexBuffers(uint32_t startSlot, uint32_t count,
                                       ID3D11Buffer* const* buffers, const uint32_t* strides, const uint32_t* offsets)
{
    auto* ctx = context_.Get();
    if (!ctx) return;
    ctx->IASetVertexBuffers(startSlot, count, buffers, strides, offsets);
}

void GraphicsContext::SetIndexBuffer(Buffer* buffer, DXGI_FORMAT format, uint32_t offset)
{
    auto* ctx = context_.Get();
    if (!ctx) return;

    if (buffer) {
        ctx->IASetIndexBuffer(buffer->Get(), format, offset);
    } else {
        ctx->IASetIndexBuffer(nullptr, DXGI_FORMAT_R32_UINT, 0);
    }
}

//===========================================================================
// ストリーム出力
//===========================================================================
void GraphicsContext::SetStreamOutputTargets(uint32_t count, Buffer* const* buffers, const uint32_t* offsets)
{
    auto* ctx = context_.Get();
    if (!ctx) return;

    ID3D11Buffer* d3dBuffers[D3D11_SO_BUFFER_SLOT_COUNT] = {};
    for (uint32_t i = 0; i < count && i < D3D11_SO_BUFFER_SLOT_COUNT; ++i) {
        d3dBuffers[i] = (buffers && buffers[i]) ? buffers[i]->Get() : nullptr;
    }
    ctx->SOSetTargets(count, d3dBuffers, offsets);
}

//===========================================================================
// バッファ更新
//===========================================================================
void GraphicsContext::UpdateBuffer(Buffer* buffer, const void* data)
{
    if (!buffer || !data) return;
    UpdateBuffer(buffer, data, buffer->Size());
}

void GraphicsContext::UpdateBuffer(Buffer* buffer, const void* data, uint32_t sizeInBytes)
{
    auto* ctx = context_.Get();
    if (!ctx || !buffer || !data) return;

    if (buffer->IsDynamic()) {
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (SUCCEEDED(ctx->Map(buffer->Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
            memcpy(mapped.pData, data, sizeInBytes);
            ctx->Unmap(buffer->Get(), 0);
        }
    } else {
        ctx->UpdateSubresource(buffer->Get(), 0, nullptr, data, 0, 0);
    }
}

void* GraphicsContext::MapBuffer(Buffer* buffer, D3D11_MAP mapType)
{
    auto* ctx = context_.Get();
    if (!ctx || !buffer) return nullptr;

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(ctx->Map(buffer->Get(), 0, mapType, 0, &mapped))) {
        return nullptr;
    }
    return mapped.pData;
}

void GraphicsContext::UnmapBuffer(Buffer* buffer)
{
    auto* ctx = context_.Get();
    if (!ctx || !buffer) return;
    ctx->Unmap(buffer->Get(), 0);
}

void GraphicsContext::UpdateBuffer(Buffer* buffer, const void* data, uint32_t sizeInBytes, uint32_t offsetInBytes)
{
    auto* ctx = context_.Get();
    if (!ctx || !buffer || !data) return;

    if (buffer->IsDynamic()) {
        // オフセットがある場合はNO_OVERWRITE、なければDISCARD
        D3D11_MAP mapType = (offsetInBytes > 0) ? D3D11_MAP_WRITE_NO_OVERWRITE : D3D11_MAP_WRITE_DISCARD;
        D3D11_MAPPED_SUBRESOURCE mapped{};
        if (SUCCEEDED(ctx->Map(buffer->Get(), 0, mapType, 0, &mapped))) {
            memcpy(static_cast<uint8_t*>(mapped.pData) + offsetInBytes, data, sizeInBytes);
            ctx->Unmap(buffer->Get(), 0);
        }
    } else {
        D3D11_BOX box = {};
        box.left = offsetInBytes;
        box.right = offsetInBytes + sizeInBytes;
        box.top = 0;
        box.bottom = 1;
        box.front = 0;
        box.back = 1;
        ctx->UpdateSubresource(buffer->Get(), 0, &box, data, 0, 0);
    }
}

void GraphicsContext::UpdateConstantBuffer(Buffer* buffer, const void* data, uint32_t sizeInBytes)
{
    UpdateBuffer(buffer, data, sizeInBytes);
}

//===========================================================================
// 定数バッファ設定
//===========================================================================
void GraphicsContext::SetVSConstantBuffer(uint32_t slot, Buffer* buffer)
{
    auto* ctx = context_.Get();
    if (!ctx) return;
    ID3D11Buffer* buffers[] = { buffer ? buffer->Get() : nullptr };
    ctx->VSSetConstantBuffers(slot, 1, buffers);
}

void GraphicsContext::SetPSConstantBuffer(uint32_t slot, Buffer* buffer)
{
    auto* ctx = context_.Get();
    if (!ctx) return;
    ID3D11Buffer* buffers[] = { buffer ? buffer->Get() : nullptr };
    ctx->PSSetConstantBuffers(slot, 1, buffers);
}

void GraphicsContext::SetGSConstantBuffer(uint32_t slot, Buffer* buffer)
{
    auto* ctx = context_.Get();
    if (!ctx) return;
    ID3D11Buffer* buffers[] = { buffer ? buffer->Get() : nullptr };
    ctx->GSSetConstantBuffers(slot, 1, buffers);
}

void GraphicsContext::SetHSConstantBuffer(uint32_t slot, Buffer* buffer)
{
    auto* ctx = context_.Get();
    if (!ctx) return;
    ID3D11Buffer* buffers[] = { buffer ? buffer->Get() : nullptr };
    ctx->HSSetConstantBuffers(slot, 1, buffers);
}

void GraphicsContext::SetDSConstantBuffer(uint32_t slot, Buffer* buffer)
{
    auto* ctx = context_.Get();
    if (!ctx) return;
    ID3D11Buffer* buffers[] = { buffer ? buffer->Get() : nullptr };
    ctx->DSSetConstantBuffers(slot, 1, buffers);
}

void GraphicsContext::SetCSConstantBuffer(uint32_t slot, Buffer* buffer)
{
    auto* ctx = context_.Get();
    if (!ctx) return;
    ID3D11Buffer* buffers[] = { buffer ? buffer->Get() : nullptr };
    ctx->CSSetConstantBuffers(slot, 1, buffers);
}

//===========================================================================
// シェーダーリソース（SRV直接指定）
//===========================================================================
void GraphicsContext::SetVSShaderResourceView(uint32_t slot, ID3D11ShaderResourceView* srv)
{
    auto* ctx = context_.Get();
    if (!ctx) return;
    ctx->VSSetShaderResources(slot, 1, &srv);
}

void GraphicsContext::SetPSShaderResourceView(uint32_t slot, ID3D11ShaderResourceView* srv)
{
    auto* ctx = context_.Get();
    if (!ctx) return;
    ctx->PSSetShaderResources(slot, 1, &srv);
}

void GraphicsContext::SetGSShaderResourceView(uint32_t slot, ID3D11ShaderResourceView* srv)
{
    auto* ctx = context_.Get();
    if (!ctx) return;
    ctx->GSSetShaderResources(slot, 1, &srv);
}

void GraphicsContext::SetHSShaderResourceView(uint32_t slot, ID3D11ShaderResourceView* srv)
{
    auto* ctx = context_.Get();
    if (!ctx) return;
    ctx->HSSetShaderResources(slot, 1, &srv);
}

void GraphicsContext::SetDSShaderResourceView(uint32_t slot, ID3D11ShaderResourceView* srv)
{
    auto* ctx = context_.Get();
    if (!ctx) return;
    ctx->DSSetShaderResources(slot, 1, &srv);
}

void GraphicsContext::SetCSShaderResourceView(uint32_t slot, ID3D11ShaderResourceView* srv)
{
    auto* ctx = context_.Get();
    if (!ctx) return;
    ctx->CSSetShaderResources(slot, 1, &srv);
}

//===========================================================================
// シェーダーリソース（Texture）
//===========================================================================
void GraphicsContext::SetVSShaderResource(uint32_t slot, Texture* texture)
{
    auto* ctx = context_.Get();
    if (!ctx) return;
    ID3D11ShaderResourceView* srvs[] = { texture ? texture->Srv() : nullptr };
    ctx->VSSetShaderResources(slot, 1, srvs);
}

void GraphicsContext::SetPSShaderResource(uint32_t slot, Texture* texture)
{
    auto* ctx = context_.Get();
    if (!ctx) return;
    ID3D11ShaderResourceView* srvs[] = { texture ? texture->Srv() : nullptr };
    ctx->PSSetShaderResources(slot, 1, srvs);
}

void GraphicsContext::SetGSShaderResource(uint32_t slot, Texture* texture)
{
    auto* ctx = context_.Get();
    if (!ctx) return;
    ID3D11ShaderResourceView* srvs[] = { texture ? texture->Srv() : nullptr };
    ctx->GSSetShaderResources(slot, 1, srvs);
}

void GraphicsContext::SetHSShaderResource(uint32_t slot, Texture* texture)
{
    auto* ctx = context_.Get();
    if (!ctx) return;
    ID3D11ShaderResourceView* srvs[] = { texture ? texture->Srv() : nullptr };
    ctx->HSSetShaderResources(slot, 1, srvs);
}

void GraphicsContext::SetDSShaderResource(uint32_t slot, Texture* texture)
{
    auto* ctx = context_.Get();
    if (!ctx) return;
    ID3D11ShaderResourceView* srvs[] = { texture ? texture->Srv() : nullptr };
    ctx->DSSetShaderResources(slot, 1, srvs);
}

void GraphicsContext::SetCSShaderResource(uint32_t slot, Texture* texture)
{
    auto* ctx = context_.Get();
    if (!ctx) return;
    ID3D11ShaderResourceView* srvs[] = { texture ? texture->Srv() : nullptr };
    ctx->CSSetShaderResources(slot, 1, srvs);
}

//===========================================================================
// シェーダーリソース（Buffer）
//===========================================================================
void GraphicsContext::SetVSShaderResource(uint32_t slot, Buffer* buffer)
{
    auto* ctx = context_.Get();
    if (!ctx) return;
    ID3D11ShaderResourceView* srvs[] = { buffer ? buffer->Srv() : nullptr };
    ctx->VSSetShaderResources(slot, 1, srvs);
}

void GraphicsContext::SetPSShaderResource(uint32_t slot, Buffer* buffer)
{
    auto* ctx = context_.Get();
    if (!ctx) return;
    ID3D11ShaderResourceView* srvs[] = { buffer ? buffer->Srv() : nullptr };
    ctx->PSSetShaderResources(slot, 1, srvs);
}

void GraphicsContext::SetGSShaderResource(uint32_t slot, Buffer* buffer)
{
    auto* ctx = context_.Get();
    if (!ctx) return;
    ID3D11ShaderResourceView* srvs[] = { buffer ? buffer->Srv() : nullptr };
    ctx->GSSetShaderResources(slot, 1, srvs);
}

void GraphicsContext::SetHSShaderResource(uint32_t slot, Buffer* buffer)
{
    auto* ctx = context_.Get();
    if (!ctx) return;
    ID3D11ShaderResourceView* srvs[] = { buffer ? buffer->Srv() : nullptr };
    ctx->HSSetShaderResources(slot, 1, srvs);
}

void GraphicsContext::SetDSShaderResource(uint32_t slot, Buffer* buffer)
{
    auto* ctx = context_.Get();
    if (!ctx) return;
    ID3D11ShaderResourceView* srvs[] = { buffer ? buffer->Srv() : nullptr };
    ctx->DSSetShaderResources(slot, 1, srvs);
}

void GraphicsContext::SetCSShaderResource(uint32_t slot, Buffer* buffer)
{
    auto* ctx = context_.Get();
    if (!ctx) return;
    ID3D11ShaderResourceView* srvs[] = { buffer ? buffer->Srv() : nullptr };
    ctx->CSSetShaderResources(slot, 1, srvs);
}

//===========================================================================
// サンプラー
//===========================================================================
void GraphicsContext::SetVSSampler(uint32_t slot, SamplerState* sampler)
{
    auto* ctx = context_.Get();
    if (!ctx) return;
    ID3D11SamplerState* samplers[] = { sampler ? sampler->GetD3DSamplerState() : nullptr };
    ctx->VSSetSamplers(slot, 1, samplers);
}

void GraphicsContext::SetPSSampler(uint32_t slot, SamplerState* sampler)
{
    // slot 0のみキャッシュ（最も頻繁に使用）
    if (slot == 0) {
        if (cachedPSSampler0_ == sampler) return;
        cachedPSSampler0_ = sampler;
    }

    auto* ctx = context_.Get();
    if (!ctx) return;
    ID3D11SamplerState* samplers[] = { sampler ? sampler->GetD3DSamplerState() : nullptr };
    ctx->PSSetSamplers(slot, 1, samplers);
}

void GraphicsContext::SetGSSampler(uint32_t slot, SamplerState* sampler)
{
    auto* ctx = context_.Get();
    if (!ctx) return;
    ID3D11SamplerState* samplers[] = { sampler ? sampler->GetD3DSamplerState() : nullptr };
    ctx->GSSetSamplers(slot, 1, samplers);
}

void GraphicsContext::SetHSSampler(uint32_t slot, SamplerState* sampler)
{
    auto* ctx = context_.Get();
    if (!ctx) return;
    ID3D11SamplerState* samplers[] = { sampler ? sampler->GetD3DSamplerState() : nullptr };
    ctx->HSSetSamplers(slot, 1, samplers);
}

void GraphicsContext::SetDSSampler(uint32_t slot, SamplerState* sampler)
{
    auto* ctx = context_.Get();
    if (!ctx) return;
    ID3D11SamplerState* samplers[] = { sampler ? sampler->GetD3DSamplerState() : nullptr };
    ctx->DSSetSamplers(slot, 1, samplers);
}

void GraphicsContext::SetCSSampler(uint32_t slot, SamplerState* sampler)
{
    auto* ctx = context_.Get();
    if (!ctx) return;
    ID3D11SamplerState* samplers[] = { sampler ? sampler->GetD3DSamplerState() : nullptr };
    ctx->CSSetSamplers(slot, 1, samplers);
}

//===========================================================================
// パイプラインステート
//===========================================================================
void GraphicsContext::SetBlendState(BlendState* state, const float blendFactor[4], uint32_t sampleMask)
{
    if (cachedBlendState_ == state) return;
    cachedBlendState_ = state;

    auto* ctx = context_.Get();
    if (!ctx) return;

    static const float defaultBlendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    const float* factor = blendFactor ? blendFactor : defaultBlendFactor;
    ctx->OMSetBlendState(state ? state->GetD3DBlendState() : nullptr, factor, sampleMask);
}

void GraphicsContext::SetDepthStencilState(DepthStencilState* state, uint32_t stencilRef)
{
    if (cachedDepthStencilState_ == state && cachedStencilRef_ == stencilRef) return;
    cachedDepthStencilState_ = state;
    cachedStencilRef_ = stencilRef;

    auto* ctx = context_.Get();
    if (!ctx) return;
    ctx->OMSetDepthStencilState(state ? state->GetD3DDepthStencilState() : nullptr, stencilRef);
}

void GraphicsContext::SetRasterizerState(RasterizerState* state)
{
    if (cachedRasterizerState_ == state) return;
    cachedRasterizerState_ = state;

    auto* ctx = context_.Get();
    if (!ctx) return;
    ctx->RSSetState(state ? state->GetD3DRasterizerState() : nullptr);
}

//===========================================================================
// シェーダー設定（Shader）
//===========================================================================
void GraphicsContext::SetVertexShader(Shader* shader)
{
    if (cachedVS_ == shader) return;
    cachedVS_ = shader;

    auto* ctx = context_.Get();
    if (!ctx) return;
    ctx->VSSetShader(shader ? shader->AsVs() : nullptr, nullptr, 0);
}

void GraphicsContext::SetPixelShader(Shader* shader)
{
    if (cachedPS_ == shader) return;
    cachedPS_ = shader;

    auto* ctx = context_.Get();
    if (!ctx) return;
    ctx->PSSetShader(shader ? shader->AsPs() : nullptr, nullptr, 0);
}

void GraphicsContext::SetGeometryShader(Shader* shader)
{
    auto* ctx = context_.Get();
    if (!ctx) return;
    ctx->GSSetShader(shader ? shader->AsGs() : nullptr, nullptr, 0);
}

void GraphicsContext::SetHullShader(Shader* shader)
{
    auto* ctx = context_.Get();
    if (!ctx) return;
    ctx->HSSetShader(shader ? shader->AsHs() : nullptr, nullptr, 0);
}

void GraphicsContext::SetDomainShader(Shader* shader)
{
    auto* ctx = context_.Get();
    if (!ctx) return;
    ctx->DSSetShader(shader ? shader->AsDs() : nullptr, nullptr, 0);
}

void GraphicsContext::SetComputeShader(Shader* shader)
{
    auto* ctx = context_.Get();
    if (!ctx) return;
    ctx->CSSetShader(shader ? shader->AsCs() : nullptr, nullptr, 0);
}

//===========================================================================
// UAV設定
//===========================================================================
void GraphicsContext::SetCSUnorderedAccessView(uint32_t slot, Texture* texture)
{
    auto* ctx = context_.Get();
    if (!ctx) return;
    ID3D11UnorderedAccessView* uavs[] = { texture ? texture->Uav() : nullptr };
    ctx->CSSetUnorderedAccessViews(slot, 1, uavs, nullptr);
}

void GraphicsContext::SetCSUnorderedAccessView(uint32_t slot, Buffer* buffer, uint32_t initialCount)
{
    auto* ctx = context_.Get();
    if (!ctx) return;
    ID3D11UnorderedAccessView* uavs[] = { buffer ? buffer->Uav() : nullptr };
    UINT counts[] = { initialCount };
    ctx->CSSetUnorderedAccessViews(slot, 1, uavs, counts);
}

void GraphicsContext::SetCSUnorderedAccessViewDirect(uint32_t slot, ID3D11UnorderedAccessView* uav, uint32_t initialCount)
{
    auto* ctx = context_.Get();
    if (!ctx) return;
    UINT counts[] = { initialCount };
    ctx->CSSetUnorderedAccessViews(slot, 1, &uav, counts);
}

//===========================================================================
// カウンター操作
//===========================================================================
void GraphicsContext::CopyStructureCount(Buffer* destBuffer, uint32_t destAlignedByteOffset, ID3D11UnorderedAccessView* srcUAV)
{
    auto* ctx = context_.Get();
    if (!ctx || !destBuffer || !srcUAV) return;
    ctx->CopyStructureCount(destBuffer->Get(), destAlignedByteOffset, srcUAV);
}

//===========================================================================
// リソースコピー
//===========================================================================
void GraphicsContext::CopyResource(ID3D11Resource* dest, ID3D11Resource* src)
{
    auto* ctx = context_.Get();
    if (!ctx || !dest || !src) return;
    ctx->CopyResource(dest, src);
}

void GraphicsContext::UpdateSubresource(ID3D11Resource* dest, uint32_t destSubresource,
                                        const D3D11_BOX* destBox, const void* srcData,
                                        uint32_t srcRowPitch, uint32_t srcDepthPitch)
{
    auto* ctx = context_.Get();
    if (!ctx || !dest || !srcData) return;
    ctx->UpdateSubresource(dest, destSubresource, destBox, srcData, srcRowPitch, srcDepthPitch);
}

//===========================================================================
// 低レベルMap/Unmap
//===========================================================================
D3D11_MAPPED_SUBRESOURCE GraphicsContext::Map(ID3D11Resource* resource, uint32_t subresource, D3D11_MAP mapType)
{
    D3D11_MAPPED_SUBRESOURCE mapped{};
    auto* ctx = context_.Get();
    if (!ctx || !resource) return mapped;
    if (FAILED(ctx->Map(resource, subresource, mapType, 0, &mapped))) {
        return D3D11_MAPPED_SUBRESOURCE{};
    }
    return mapped;
}

void GraphicsContext::Unmap(ID3D11Resource* resource, uint32_t subresource)
{
    auto* ctx = context_.Get();
    if (!ctx || !resource) return;
    ctx->Unmap(resource, subresource);
}
