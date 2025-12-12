//----------------------------------------------------------------------------
//! @file   sprite_batch.cpp
//! @brief  スプライトバッチ描画システム実装
//----------------------------------------------------------------------------
#include "sprite_batch.h"
#include "engine/component/transform2d.h"
#include "engine/component/camera2d.h"
#include "engine/component/animator.h"
#include "dx11/graphics_device.h"
#include "dx11/graphics_context.h"
#include "engine/shader/shader_manager.h"
#include "common/logging/logging.h"
#include <algorithm>

//============================================================================
// SpriteBatch 実装
//============================================================================

bool SpriteBatch::Initialize() {
    if (initialized_) {
        return true;
    }

    // シェーダー作成
    if (!CreateShaders()) {
        return false;
    }

    // 頂点バッファ（動的）
    vertexBuffer_ = Buffer::CreateVertex(
        sizeof(SpriteVertex) * 4 * MaxSpritesPerBatch,
        sizeof(SpriteVertex),
        true  // dynamic
    );
    if (!vertexBuffer_) {
        LOG_ERROR("SpriteBatch: 頂点バッファ作成失敗");
        return false;
    }

    // インデックスバッファ（静的）
    std::vector<uint16_t> indices(6 * MaxSpritesPerBatch);
    for (uint32_t i = 0; i < MaxSpritesPerBatch; ++i) {
        uint16_t baseVertex = static_cast<uint16_t>(i * 4);
        indices[i * 6 + 0] = baseVertex + 0;
        indices[i * 6 + 1] = baseVertex + 1;
        indices[i * 6 + 2] = baseVertex + 2;
        indices[i * 6 + 3] = baseVertex + 2;
        indices[i * 6 + 4] = baseVertex + 1;
        indices[i * 6 + 5] = baseVertex + 3;
    }
    indexBuffer_ = Buffer::CreateIndex(
        static_cast<uint32_t>(sizeof(uint16_t) * indices.size()),
        false,  // not dynamic
        indices.data()
    );
    if (!indexBuffer_) {
        LOG_ERROR("SpriteBatch: インデックスバッファ作成失敗");
        return false;
    }

    // 定数バッファ
    constantBuffer_ = Buffer::CreateConstant(sizeof(CBufferData));
    if (!constantBuffer_) {
        LOG_ERROR("SpriteBatch: 定数バッファ作成失敗");
        return false;
    }

    // パイプラインステート
    blendState_ = BlendState::CreateAlphaBlend();
    samplerState_ = SamplerState::CreateDefault();
    rasterizerState_ = RasterizerState::CreateNoCull();
    depthStencilState_ = DepthStencilState::CreateDisabled();

    if (!blendState_ || !samplerState_ || !rasterizerState_ || !depthStencilState_) {
        LOG_ERROR("SpriteBatch: パイプラインステート作成失敗");
        return false;
    }

    spriteQueue_.reserve(MaxSpritesPerBatch);
    sortIndices_.reserve(MaxSpritesPerBatch);
    initialized_ = true;
    LOG_INFO("SpriteBatch: 初期化完了");
    return true;
}

bool SpriteBatch::CreateShaders() {
    auto& shaderMgr = ShaderManager::Get();
    if (!shaderMgr.IsInitialized()) {
        LOG_ERROR("SpriteBatch: ShaderManagerが初期化されていません");
        return false;
    }

    // シェーダーロード
    vertexShader_ = shaderMgr.LoadVertexShader("sprite_vs.hlsl");
    pixelShader_ = shaderMgr.LoadPixelShader("sprite_ps.hlsl");
    if (!vertexShader_ || !pixelShader_) {
        LOG_ERROR("SpriteBatch: シェーダーロード失敗");
        return false;
    }

    // 入力レイアウト作成
    D3D11_INPUT_ELEMENT_DESC inputElements[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    inputLayout_ = shaderMgr.CreateInputLayout(
        vertexShader_.get(),
        inputElements,
        _countof(inputElements)
    );
    if (!inputLayout_) {
        LOG_ERROR("SpriteBatch: 入力レイアウト作成失敗");
        return false;
    }

    return true;
}

void SpriteBatch::Shutdown() {
    if (!initialized_) return;

    // パイプラインからステートをアンバインドしてから解放
    // これにより、パイプラインが保持する参照が解放される
    auto& ctx = GraphicsContext::Get();
    auto* d3dCtx = ctx.GetContext();
    if (d3dCtx) {
        // 使用していたステートをnullでアンバインド
        d3dCtx->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
        d3dCtx->OMSetDepthStencilState(nullptr, 0);
        d3dCtx->RSSetState(nullptr);
        ID3D11SamplerState* nullSamplers[1] = { nullptr };
        d3dCtx->PSSetSamplers(0, 1, nullSamplers);
        d3dCtx->VSSetShader(nullptr, nullptr, 0);
        d3dCtx->PSSetShader(nullptr, nullptr, 0);
        d3dCtx->IASetInputLayout(nullptr);
        ID3D11Buffer* nullBuffers[1] = { nullptr };
        UINT strides[1] = { 0 };
        UINT offsets[1] = { 0 };
        d3dCtx->IASetVertexBuffers(0, 1, nullBuffers, strides, offsets);
        d3dCtx->IASetIndexBuffer(nullptr, DXGI_FORMAT_R16_UINT, 0);
    }

    vertexBuffer_.reset();
    indexBuffer_.reset();
    constantBuffer_.reset();
    vertexShader_.reset();
    pixelShader_.reset();
    inputLayout_.Reset();
    blendState_.reset();
    samplerState_.reset();
    rasterizerState_.reset();
    depthStencilState_.reset();
    spriteQueue_.clear();
    sortIndices_.clear();

    initialized_ = false;
    LOG_INFO("SpriteBatch: シャットダウン完了");
}

void SpriteBatch::SetCamera(Camera2D& camera) {
    cbufferData_.viewProjection = camera.GetViewProjectionMatrix();
}

void SpriteBatch::SetViewProjection(const Matrix& viewProjection) {
    cbufferData_.viewProjection = viewProjection;
}

void SpriteBatch::Begin() {
    if (!initialized_) {
        LOG_WARN("SpriteBatch: 初期化されていません");
        return;
    }
    if (isBegun_) {
        LOG_WARN("SpriteBatch: 既にBegin()が呼ばれています");
        return;
    }

    spriteQueue_.clear();
    drawCallCount_ = 0;
    spriteCount_ = 0;
    isBegun_ = true;
}

void SpriteBatch::Draw(
    Texture* texture,
    const Vector2& position,
    const Color& color,
    float rotation,
    const Vector2& origin,
    const Vector2& scale,
    bool flipX,
    bool flipY,
    int sortingLayer,
    int orderInLayer)
{
    if (!isBegun_) {
        LOG_WARN("SpriteBatch: Begin()が呼ばれていません");
        return;
    }
    if (!texture) {
        return;
    }

    // テクスチャサイズ取得
    float texWidth = static_cast<float>(texture->Width());
    float texHeight = static_cast<float>(texture->Height());

    // UV座標（テクスチャ全体を使用）
    float u0 = 0.0f;
    float v0 = 0.0f;
    float u1 = 1.0f;
    float v1 = 1.0f;

    // 反転
    if (flipX) std::swap(u0, u1);
    if (flipY) std::swap(v0, v1);

    // スプライトサイズ
    float width = texWidth * scale.x;
    float height = texHeight * scale.y;

    // 4頂点の計算（原点を考慮）
    float x0 = -origin.x * scale.x;
    float y0 = -origin.y * scale.y;
    float x1 = x0 + width;
    float y1 = y0 + height;

    // 回転行列
    float cosR = std::cos(rotation);
    float sinR = std::sin(rotation);

    auto rotatePoint = [&](float x, float y) -> Vector2 {
        return Vector2(
            x * cosR - y * sinR + position.x,
            x * sinR + y * cosR + position.y
        );
    };

    SpriteInfo info;
    info.texture = texture;
    info.sortingLayer = sortingLayer;
    info.orderInLayer = orderInLayer;

    Vector2 p0 = rotatePoint(x0, y0);
    Vector2 p1 = rotatePoint(x1, y0);
    Vector2 p2 = rotatePoint(x0, y1);
    Vector2 p3 = rotatePoint(x1, y1);

    info.vertices[0] = { Vector3(p0.x, p0.y, 0.0f), Vector2(u0, v0), color };
    info.vertices[1] = { Vector3(p1.x, p1.y, 0.0f), Vector2(u1, v0), color };
    info.vertices[2] = { Vector3(p2.x, p2.y, 0.0f), Vector2(u0, v1), color };
    info.vertices[3] = { Vector3(p3.x, p3.y, 0.0f), Vector2(u1, v1), color };

    spriteQueue_.push_back(info);
}

void SpriteBatch::Draw(const SpriteRenderer& renderer, const Transform2D& transform) {
    if (!isBegun_ || !renderer.GetTexture()) {
        return;
    }

    Texture* texture = renderer.GetTexture();

    // サイズ決定（カスタムサイズまたはテクスチャサイズ）
    Vector2 size = renderer.GetSize();
    if (size.x <= 0 || size.y <= 0) {
        size = Vector2(
            static_cast<float>(texture->Width()),
            static_cast<float>(texture->Height())
        );
    }

    // Transform2Dからパラメータ取得
    Vector2 position = transform.GetPosition();
    float rotation = transform.GetRotation();
    Vector2 scale = transform.GetScale();

    // SpriteRendererのpivotを使用（なければ左上原点）
    Vector2 pivot = renderer.GetPivot();

    Draw(texture, position, renderer.GetColor(),
         rotation, pivot, scale,
         renderer.IsFlipX(), renderer.IsFlipY(),
         renderer.GetSortingLayer(), renderer.GetOrderInLayer());
}

void SpriteBatch::Draw(const SpriteRenderer& renderer, const Transform2D& transform, const Animator& animator) {
    if (!isBegun_ || !renderer.GetTexture()) {
        return;
    }

    Texture* texture = renderer.GetTexture();

    // AnimatorからUV情報を取得
    Vector2 uvCoord = animator.GetUVCoord();
    Vector2 uvSize = animator.GetUVSize();

    // フレームサイズ（テクスチャサイズ * UVサイズ）
    float frameWidth = static_cast<float>(texture->Width()) * std::abs(uvSize.x);
    float frameHeight = static_cast<float>(texture->Height()) * std::abs(uvSize.y);

    // Transform2Dからパラメータ取得
    Vector2 position = transform.GetPosition();
    float rotation = transform.GetRotation();
    Vector2 scale = transform.GetScale();

    // SpriteRendererのpivotを使用
    Vector2 pivot = renderer.GetPivot();

    // 原点：pivotが設定されていればそれを使用、なければフレーム中心
    Vector2 origin;
    if (renderer.HasPivot()) {
        origin = pivot;
        // ミラー時はX軸のpivotを反転（フレーム幅からの相対位置に）
        if (animator.GetMirror()) {
            origin.x = frameWidth - pivot.x;
        }
    } else {
        origin = Vector2(frameWidth * 0.5f, frameHeight * 0.5f);
    }

    // スプライトサイズ
    float width = frameWidth * scale.x;
    float height = frameHeight * scale.y;

    // 4頂点の計算（原点を考慮）
    float x0 = -origin.x * scale.x;
    float y0 = -origin.y * scale.y;
    float x1 = x0 + width;
    float y1 = y0 + height;

    // 回転行列
    float cosR = std::cos(rotation);
    float sinR = std::sin(rotation);

    auto rotatePoint = [&](float x, float y) -> Vector2 {
        return Vector2(
            x * cosR - y * sinR + position.x,
            x * sinR + y * cosR + position.y
        );
    };

    // UV座標（反転考慮）
    float u0 = uvCoord.x;
    float v0 = uvCoord.y;
    float u1 = uvCoord.x + uvSize.x;
    float v1 = uvCoord.y + uvSize.y;

    // SpriteRendererの反転
    if (renderer.IsFlipX()) std::swap(u0, u1);
    if (renderer.IsFlipY()) std::swap(v0, v1);

    SpriteInfo info;
    info.texture = texture;
    info.sortingLayer = renderer.GetSortingLayer();
    info.orderInLayer = renderer.GetOrderInLayer();

    Vector2 p0 = rotatePoint(x0, y0);
    Vector2 p1 = rotatePoint(x1, y0);
    Vector2 p2 = rotatePoint(x0, y1);
    Vector2 p3 = rotatePoint(x1, y1);

    info.vertices[0] = { Vector3(p0.x, p0.y, 0.0f), Vector2(u0, v0), renderer.GetColor() };
    info.vertices[1] = { Vector3(p1.x, p1.y, 0.0f), Vector2(u1, v0), renderer.GetColor() };
    info.vertices[2] = { Vector3(p2.x, p2.y, 0.0f), Vector2(u0, v1), renderer.GetColor() };
    info.vertices[3] = { Vector3(p3.x, p3.y, 0.0f), Vector2(u1, v1), renderer.GetColor() };

    spriteQueue_.push_back(info);
}

void SpriteBatch::End() {
    if (!isBegun_) {
        LOG_WARN("SpriteBatch: Begin()が呼ばれていません");
        return;
    }

    if (!spriteQueue_.empty()) {
        SortSprites();
        FlushBatch();
    }

    isBegun_ = false;
}

void SpriteBatch::SortSprites() {
    // インデックス配列を初期化
    uint32_t count = static_cast<uint32_t>(spriteQueue_.size());
    sortIndices_.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        sortIndices_[i] = i;
    }

    // インデックスをソート（SpriteInfo自体は移動しない）
    std::stable_sort(sortIndices_.begin(), sortIndices_.end(),
        [this](uint32_t a, uint32_t b) {
            const SpriteInfo& sa = spriteQueue_[a];
            const SpriteInfo& sb = spriteQueue_[b];
            if (sa.sortingLayer != sb.sortingLayer) {
                return sa.sortingLayer < sb.sortingLayer;
            }
            return sa.orderInLayer < sb.orderInLayer;
        });
}

void SpriteBatch::FlushBatch() {
    if (spriteQueue_.empty()) return;

    auto& ctx = GraphicsContext::Get();

    // 定数バッファ更新
    ctx.UpdateConstantBuffer(constantBuffer_.get(), cbufferData_);

    // パイプライン設定
    ctx.SetInputLayout(inputLayout_.Get());
    ctx.SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    ctx.SetVertexBuffer(0, vertexBuffer_.get(), sizeof(SpriteVertex));
    ctx.SetIndexBuffer(indexBuffer_.get(), DXGI_FORMAT_R16_UINT, 0);

    ctx.SetVertexShader(vertexShader_.get());
    ctx.SetVSConstantBuffer(0, constantBuffer_.get());

    ctx.SetPixelShader(pixelShader_.get());
    ctx.SetPSSampler(0, samplerState_.get());

    ctx.SetBlendState(blendState_.get());
    ctx.SetDepthStencilState(depthStencilState_.get());
    ctx.SetRasterizerState(rasterizerState_.get());

    // バッチ描画
    Texture* currentTexture = nullptr;
    uint32_t batchStart = 0;
    uint32_t spriteIndex = 0;

    // 頂点データをマップ
    auto* vertices = static_cast<SpriteVertex*>(ctx.MapBuffer(vertexBuffer_.get()));
    if (!vertices) {
        LOG_ERROR("SpriteBatch: 頂点バッファのマップに失敗");
        return;
    }

    for (uint32_t idx : sortIndices_) {
        const SpriteInfo& sprite = spriteQueue_[idx];

        // テクスチャが変わったらフラッシュ
        if (currentTexture && sprite.texture != currentTexture) {
            ctx.UnmapBuffer(vertexBuffer_.get());

            ctx.SetPSShaderResource(0, currentTexture);

            uint32_t indexCount = (spriteIndex - batchStart) * 6;
            ctx.DrawIndexed(indexCount, batchStart * 6, 0);
            ++drawCallCount_;

            // 再マップ
            vertices = static_cast<SpriteVertex*>(ctx.MapBuffer(vertexBuffer_.get()));
            if (!vertices) break;

            batchStart = spriteIndex;
        }

        currentTexture = sprite.texture;

        // 頂点コピー
        memcpy(&vertices[spriteIndex * 4], sprite.vertices, sizeof(SpriteVertex) * 4);
        ++spriteIndex;
        ++spriteCount_;
    }

    // 最後のバッチを描画
    if (spriteIndex > batchStart && currentTexture) {
        ctx.UnmapBuffer(vertexBuffer_.get());

        ctx.SetPSShaderResource(0, currentTexture);

        uint32_t indexCount = (spriteIndex - batchStart) * 6;
        ctx.DrawIndexed(indexCount, batchStart * 6, 0);
        ++drawCallCount_;
    }
}
