//----------------------------------------------------------------------------
//! @file   sprite_batch.cpp
//! @brief  スプライトバッチ描画システム実装
//----------------------------------------------------------------------------
#include "sprite_batch.h"
#include "engine/component/transform.h"
#include "engine/component/camera2d.h"
#include "engine/component/animator.h"
#include "engine/graphics2d/render_state_manager.h"
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

    // RenderStateManagerの初期化確認
    if (!RenderStateManager::Get().IsInitialized()) {
        LOG_ERROR("SpriteBatch: RenderStateManagerが初期化されていません");
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
        ID3D11Buffer* nullCB[1] = { nullptr };
        d3dCtx->VSSetConstantBuffers(0, 1, nullCB);
        ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
        d3dCtx->PSSetShaderResources(0, 1, nullSRV);
        d3dCtx->Flush();
    }

    vertexBuffer_.reset();
    indexBuffer_.reset();
    constantBuffer_.reset();
    vertexShader_.reset();
    pixelShader_.reset();
    inputLayout_.Reset();
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

    // sortingLayer/orderInLayerから深度値を計算
    float z = CalculateDepth(sortingLayer, orderInLayer);

    info.vertices[0] = { Vector3(p0.x, p0.y, z), Vector2(u0, v0), color };
    info.vertices[1] = { Vector3(p1.x, p1.y, z), Vector2(u1, v0), color };
    info.vertices[2] = { Vector3(p2.x, p2.y, z), Vector2(u0, v1), color };
    info.vertices[3] = { Vector3(p3.x, p3.y, z), Vector2(u1, v1), color };

    spriteQueue_.push_back(info);
}

void SpriteBatch::Draw(
    Texture* texture,
    const Vector2& position,
    const Vector4& sourceRect,
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
    if (texWidth <= 0.0f || texHeight <= 0.0f) {
        return;
    }

    // ソース矩形からUV座標を計算（ピクセル→正規化）
    float u0 = sourceRect.x / texWidth;
    float v0 = sourceRect.y / texHeight;
    float u1 = (sourceRect.x + sourceRect.z) / texWidth;
    float v1 = (sourceRect.y + sourceRect.w) / texHeight;

    // 反転
    if (flipX) std::swap(u0, u1);
    if (flipY) std::swap(v0, v1);

    // スプライトサイズ（ソース矩形のサイズを使用）
    float width = sourceRect.z * scale.x;
    float height = sourceRect.w * scale.y;

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

    // sortingLayer/orderInLayerから深度値を計算
    float z = CalculateDepth(sortingLayer, orderInLayer);

    info.vertices[0] = { Vector3(p0.x, p0.y, z), Vector2(u0, v0), color };
    info.vertices[1] = { Vector3(p1.x, p1.y, z), Vector2(u1, v0), color };
    info.vertices[2] = { Vector3(p2.x, p2.y, z), Vector2(u0, v1), color };
    info.vertices[3] = { Vector3(p3.x, p3.y, z), Vector2(u1, v1), color };

    spriteQueue_.push_back(info);
}

void SpriteBatch::Draw(const SpriteRenderer& renderer, const Transform& transform) {
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

    // Transformからパラメータ取得
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

void SpriteBatch::Draw(const SpriteRenderer& renderer, const Transform& transform, const Animator& animator) {
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

    // Transformからパラメータ取得
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

    // sortingLayer/orderInLayerから深度値を計算
    float z = CalculateDepth(info.sortingLayer, info.orderInLayer);

    info.vertices[0] = { Vector3(p0.x, p0.y, z), Vector2(u0, v0), renderer.GetColor() };
    info.vertices[1] = { Vector3(p1.x, p1.y, z), Vector2(u1, v0), renderer.GetColor() };
    info.vertices[2] = { Vector3(p2.x, p2.y, z), Vector2(u0, v1), renderer.GetColor() };
    info.vertices[3] = { Vector3(p3.x, p3.y, z), Vector2(u1, v1), renderer.GetColor() };

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

void SpriteBatch::SetCustomShaders(Shader* vs, Shader* ps) {
    customVertexShader_ = vs;
    customPixelShader_ = ps;
}

void SpriteBatch::ClearCustomShaders() {
    customVertexShader_ = nullptr;
    customPixelShader_ = nullptr;
}

void SpriteBatch::SetCustomBlendState(BlendState* blendState) {
    customBlendState_ = blendState;
}

void SpriteBatch::ClearCustomBlendState() {
    customBlendState_ = nullptr;
}

void SpriteBatch::SetCustomSamplerState(SamplerState* samplerState) {
    customSamplerState_ = samplerState;
}

void SpriteBatch::ClearCustomSamplerState() {
    customSamplerState_ = nullptr;
}

void SpriteBatch::SortSprites() {
    // インデックス配列を初期化
    uint32_t count = static_cast<uint32_t>(spriteQueue_.size());
    sortIndices_.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        sortIndices_[i] = i;
    }

    // インデックスをソート（SpriteInfo自体は移動しない）
    // ソートキー: 1) sortingLayer, 2) orderInLayer, 3) textureポインタ
    // 同一深度のスプライトをテクスチャでグループ化し、描画状態変更を削減
    std::stable_sort(sortIndices_.begin(), sortIndices_.end(),
        [this](uint32_t a, uint32_t b) {
            const SpriteInfo& sa = spriteQueue_[a];
            const SpriteInfo& sb = spriteQueue_[b];
            if (sa.sortingLayer != sb.sortingLayer) {
                return sa.sortingLayer < sb.sortingLayer;
            }
            if (sa.orderInLayer != sb.orderInLayer) {
                return sa.orderInLayer < sb.orderInLayer;
            }
            // 同一深度ならテクスチャでグループ化
            return sa.texture < sb.texture;
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

    // カスタムシェーダーがあれば使用、なければデフォルト
    Shader* vs = customVertexShader_ ? customVertexShader_ : vertexShader_.get();
    Shader* ps = customPixelShader_ ? customPixelShader_ : pixelShader_.get();

    ctx.SetVertexShader(vs);
    ctx.SetVSConstantBuffer(0, constantBuffer_.get());

    ctx.SetPixelShader(ps);

    // RenderStateManagerからステートを取得
    auto& rsm = RenderStateManager::Get();

    // カスタムサンプラーステートがあれば使用、なければデフォルト
    SamplerState* ss = customSamplerState_ ? customSamplerState_ : rsm.GetLinearWrap();
    ctx.SetPSSampler(0, ss);

    // カスタムブレンドステートがあれば使用、なければデフォルト
    BlendState* bs = customBlendState_ ? customBlendState_ : rsm.GetAlphaBlend();
    ctx.SetBlendState(bs);
    ctx.SetDepthStencilState(rsm.GetDepthLessEqual());
    ctx.SetRasterizerState(rsm.GetNoCull());

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

//----------------------------------------------------------------------------
// 深度値計算
//----------------------------------------------------------------------------

float SpriteBatch::CalculateDepth(int sortingLayer, int orderInLayer) const noexcept {
    // sortingLayer: 大きいほど手前
    // orderInLayer: 大きいほど手前
    //
    // DirectXTK右手系投影行列では大きいZ値ほど小さいNDC Zになる
    // そのため、手前のスプライト（大きいsortingLayer）には大きいZ値を設定
    // スプライト用Z値範囲: 0.1 ～ 0.9（0.0/1.0は3D用に予約）

    constexpr float minDepth = 0.1f;
    constexpr float maxDepth = 0.9f;
    constexpr float depthRange = maxDepth - minDepth;

    constexpr int maxLayer = 100;      // sortingLayerの想定範囲: -100 ～ 100
    constexpr int maxOrder = 1000;     // orderInLayerの想定範囲: -1000 ～ 1000

    // sortingLayerを正規化（大きいほど1に近い = 大きいZ値 = 手前）
    // Note: Windowsのmax/minマクロとの衝突を避けるため括弧を使用
    int clampedLayer = (std::max)(-maxLayer, (std::min)(maxLayer, sortingLayer));
    float layerNorm = static_cast<float>(clampedLayer + maxLayer) / static_cast<float>(2 * maxLayer);

    // orderInLayerを正規化（レイヤー内の細かい順序）
    int clampedOrder = (std::max)(-maxOrder, (std::min)(maxOrder, orderInLayer));
    float orderNorm = static_cast<float>(clampedOrder + maxOrder) / static_cast<float>(2 * maxOrder);

    // layerNormが主要、orderNormは微調整（0.001倍）
    float normalizedDepth = layerNorm + orderNorm * 0.001f;

    return minDepth + depthRange * normalizedDepth;
}
