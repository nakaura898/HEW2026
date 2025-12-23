//----------------------------------------------------------------------------
//! @file   stage_background.cpp
//! @brief  ステージ背景実装
//----------------------------------------------------------------------------
#include "stage_background.h"
#include "engine/texture/texture_manager.h"
#include "engine/shader/shader_manager.h"
#include "engine/c_systems/sprite_batch.h"
#include "engine/component/camera2d.h"
#include "dx11/gpu/texture.h"
#include "dx11/graphics_device.h"
#include "dx11/graphics_context.h"
#include "common/logging/logging.h"
#include <algorithm>
#include <DirectXMath.h>

//----------------------------------------------------------------------------
// ステージサイズ設定（ここを変更してサイズ調整）
//----------------------------------------------------------------------------
constexpr float STAGE_WIDTH  = 5120.0f;
constexpr float STAGE_HEIGHT = 2880.0f;

//----------------------------------------------------------------------------
void StageBackground::Initialize(const std::string& stageId, float screenWidth, float screenHeight)
{
    screenWidth_ = screenWidth;
    screenHeight_ = screenHeight;

    // ステージサイズ
    stageWidth_ = STAGE_WIDTH;
    stageHeight_ = STAGE_HEIGHT;

    // 乱数初期化
    std::random_device rd;
    rng_ = std::mt19937(rd());

    // テクスチャパスのベース
    std::string basePath = stageId + "/";

    // ベース地面テクスチャ作成（1x1単色、シェーダーのBASE_COLORと同じ）
    // sRGB: (0.30, 0.52, 0.28) = RGB(76, 133, 71) = #4C8547
    uint8_t baseColorData[4] = { 76, 133, 71, 255 };  // RGBA
    baseGroundTexture_ = TextureManager::Get().Create2D(
        1, 1,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        D3D11_BIND_SHADER_RESOURCE,
        baseColorData,
        4  // rowPitch = 4 bytes for 1 pixel RGBA
    );
    if (baseGroundTexture_) {
        baseGroundWidth_ = 1.0f;
        baseGroundHeight_ = 1.0f;
        LOG_INFO("[StageBackground] Base ground color texture created (1x1)");
    } else {
        LOG_ERROR("[StageBackground] Failed to create base ground texture");
    }

    // 地面テクスチャ読み込み
    groundTexture_ = TextureManager::Get().LoadTexture2D(basePath + "ground.png");
    if (groundTexture_) {
        float texW = static_cast<float>(groundTexture_->Width());
        float texH = static_cast<float>(groundTexture_->Height());

        // タイルサイズ（テクスチャ全体を使用）
        tileWidth_ = texW;
        tileHeight_ = texH;

        // オーバーラップ率（fadeWidth=0.30 に合わせて50%に増加）
        float overlapRatio = 0.50f;
        float stepX = tileWidth_ * (1.0f - overlapRatio);
        float stepY = tileHeight_ * (1.0f - overlapRatio);

        // ランダム回転（0, 90, 180, 270度）
        std::uniform_int_distribution<int> rotDist(0, 3);
        std::uniform_int_distribution<int> flipDist(0, 1);

        // ステージ全体をカバーするタイル数を計算（50%オーバーラップ + オフセット分）
        int tilesX = static_cast<int>(std::ceil(stageWidth_ / stepX)) + 4;
        int tilesY = static_cast<int>(std::ceil(stageHeight_ / stepY)) + 4;

        // タイルを配置（オーバーラップあり、シェーダーで端フェード）
        // エッジフェードを考慮して、最初のタイルを左上にオフセット
        float offsetX = -tileWidth_ * 0.50f;   // オーバーラップ分
        float offsetY = -tileHeight_ * 0.50f;  // オーバーラップ分

        for (int y = 0; y < tilesY; ++y) {
            for (int x = 0; x < tilesX; ++x) {
                GroundTile tile;
                tile.position = Vector2(
                    x * stepX + tileWidth_ * 0.5f + offsetX,
                    y * stepY + tileHeight_ * 0.5f + offsetY
                );
                tile.rotation = rotDist(rng_) * 1.5707963f;  // 0, 90, 180, 270度
                tile.flipX = flipDist(rng_) == 1;
                tile.flipY = flipDist(rng_) == 1;
                tile.alpha = 1.0f;  // フルアルファ（シェーダーで端フェード）
                groundTiles_.push_back(tile);
            }
        }

        LOG_INFO("[StageBackground] Ground tiles: " + std::to_string(groundTiles_.size()) + " (edge fade shader + overlap)");
    } else {
        LOG_ERROR("[StageBackground] Failed to load ground texture: " + basePath + "ground.png");
    }

    // 地面用シェーダー読み込み（端フェード付き）
    groundVertexShader_ = ShaderManager::Get().LoadVertexShader("ground_vs.hlsl");
    groundPixelShader_ = ShaderManager::Get().LoadPixelShader("ground_ps.hlsl");
    if (groundVertexShader_ && groundPixelShader_) {
        LOG_INFO("[StageBackground] Ground shaders loaded");
    } else {
        LOG_WARN("[StageBackground] Ground shaders not loaded, using default");
    }

    // 正規化シェーダー読み込み（2パス目）
    normalizePixelShader_ = ShaderManager::Get().LoadPixelShader("ground_normalize_ps.hlsl");
    if (normalizePixelShader_) {
        LOG_INFO("[StageBackground] Normalize shader loaded");
    } else {
        LOG_WARN("[StageBackground] Normalize shader not loaded");
    }

    // 蓄積用レンダーターゲット作成（RGBA16F、ステージサイズ）
    accumulationRT_ = TextureManager::Get().CreateRenderTarget(
        static_cast<uint32_t>(stageWidth_),
        static_cast<uint32_t>(stageHeight_),
        DXGI_FORMAT_R16G16B16A16_FLOAT);
    if (accumulationRT_) {
        LOG_INFO("[StageBackground] Accumulation RT created: " +
                 std::to_string(static_cast<int>(stageWidth_)) + "x" +
                 std::to_string(static_cast<int>(stageHeight_)));
    } else {
        LOG_ERROR("[StageBackground] Failed to create accumulation RT");
    }

    // 純粋加算ブレンドステート作成（ONE + ONE）
    {
        D3D11_BLEND_DESC desc{};
        desc.AlphaToCoverageEnable = FALSE;
        desc.IndependentBlendEnable = FALSE;
        desc.RenderTarget[0].BlendEnable = TRUE;
        desc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;        // Src * 1
        desc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;       // + Dest * 1
        desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;   // SrcA * 1
        desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;  // + DestA * 1
        desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        additiveBlendState_ = BlendState::Create(desc);
    }
    if (additiveBlendState_) {
        LOG_INFO("[StageBackground] Additive blend state created");
    }

    // クランプサンプラー作成（チャンク描画用）
    clampSamplerState_ = SamplerState::CreateClamp();
    if (clampSamplerState_) {
        LOG_INFO("[StageBackground] Clamp sampler state created");
    }

    // 地面テクスチャをプリベイク（2パス正規化）
    BakeGroundTexture();

    // 装飾を配置
    PlaceDecorations(stageId, screenWidth, screenHeight);

    LOG_INFO("[StageBackground] Initialized with " + std::to_string(decorations_.size()) + " decorations");
}

//----------------------------------------------------------------------------
void StageBackground::PlaceDecorations(const std::string& stageId, float screenWidth, float screenHeight)
{
    std::string basePath = stageId + "/";

    // 分布設定
    std::uniform_real_distribution<float> xDist(0.0f, screenWidth);
    std::uniform_real_distribution<float> yFullDist(screenHeight * 0.3f, screenHeight);
    std::uniform_real_distribution<float> yGroundDist(screenHeight * 0.6f, screenHeight * 0.95f);
    std::uniform_real_distribution<float> scaleDist(0.8f, 1.2f);
    std::uniform_real_distribution<float> smallScaleDist(0.5f, 1.0f);
    std::uniform_real_distribution<float> rotationDist(-0.1f, 0.1f);
    std::uniform_int_distribution<int> countDist5_8(5, 8);
    std::uniform_int_distribution<int> countDist10_15(10, 15);
    std::uniform_int_distribution<int> countDist15_25(15, 25);

    // 遺跡・木（奥レイヤー -120）
    {
        std::vector<std::string> bigObjects = {
            "ruins fragment.png",
            "ruins fragment 2.png",
            "ruins fragment 3.png",
            "tree.png"
        };

        int count = countDist5_8(rng_);
        std::uniform_int_distribution<size_t> objDist(0, bigObjects.size() - 1);

        for (int i = 0; i < count; ++i) {
            TexturePtr tex = TextureManager::Get().LoadTexture2D(basePath + bigObjects[objDist(rng_)]);
            if (tex) {
                Vector2 pos(xDist(rng_), yFullDist(rng_));
                Vector2 scale(scaleDist(rng_), scaleDist(rng_));
                AddDecoration(tex, pos, -90, scale, rotationDist(rng_));
            }
        }
    }

    // 草・石（中レイヤー -100）
    {
        std::vector<std::string> mediumObjects = {
            "grass big.png",
            "grass long.png",
            "stone 1.png",
            "stone 2.png",
            "stone 3.png",
            "stone 4.png",
            "stone 5.png",
            "stone 6.png",
            "stone 7.png",
            "stone 8.png"
        };

        int count = countDist10_15(rng_);
        std::uniform_int_distribution<size_t> objDist(0, mediumObjects.size() - 1);

        for (int i = 0; i < count; ++i) {
            TexturePtr tex = TextureManager::Get().LoadTexture2D(basePath + mediumObjects[objDist(rng_)]);
            if (tex) {
                Vector2 pos(xDist(rng_), yGroundDist(rng_));
                Vector2 scale(scaleDist(rng_), scaleDist(rng_));
                AddDecoration(tex, pos, -100, scale, rotationDist(rng_));
            }
        }
    }

    // 葉・木片・焚火・小さい草（手前レイヤー -80）
    {
        std::vector<std::string> smallObjects = {
            "grass small.png",
            "leaf 1.png",
            "leaf 2.png",
            "leaf 3.png",
            "leaf 4.png",
            "leaf 5.png",
            "leaf 6.png",
            "leaf 7.png",
            "leaf 8.png",
            "wood chips 1.png",
            "wood chips 2.png",
            "wood chips 3.png",
            "wood chips 4.png",
            "wood chips 5.png",
            "wood chips 6.png"
        };

        int count = countDist15_25(rng_);
        std::uniform_int_distribution<size_t> objDist(0, smallObjects.size() - 1);

        for (int i = 0; i < count; ++i) {
            TexturePtr tex = TextureManager::Get().LoadTexture2D(basePath + smallObjects[objDist(rng_)]);
            if (tex) {
                Vector2 pos(xDist(rng_), yFullDist(rng_));
                Vector2 scale(smallScaleDist(rng_), smallScaleDist(rng_));
                AddDecoration(tex, pos, -80, scale, rotationDist(rng_));
            }
        }

        // 焚火（1つだけ、画面中央付近）
        TexturePtr bonfire = TextureManager::Get().LoadTexture2D(basePath + "bonfire.png");
        if (bonfire) {
            Vector2 pos(screenWidth * 0.5f + xDist(rng_) * 0.2f - screenWidth * 0.1f,
                        screenHeight * 0.75f);
            AddDecoration(bonfire, pos, -80, Vector2::One, 0.0f);
        }
    }
}

//----------------------------------------------------------------------------
void StageBackground::AddDecoration(TexturePtr texture, const Vector2& position,
                                     int sortingLayer, const Vector2& scale, float rotation)
{
    DecorationObject obj;
    obj.texture = texture;
    obj.position = position;
    obj.scale = scale;
    obj.rotation = rotation;
    obj.sortingLayer = sortingLayer;
    decorations_.push_back(std::move(obj));
}

//----------------------------------------------------------------------------
void StageBackground::BakeGroundTexture()
{
    // 必要なリソースが揃っているか確認
    if (!groundTexture_ || !groundVertexShader_ || !groundPixelShader_ ||
        !accumulationRT_ || !additiveBlendState_ || !normalizePixelShader_) {
        LOG_WARN("[StageBackground] Cannot bake ground texture - missing resources");
        return;
    }

    auto& ctx = GraphicsContext::Get();
    ID3D11DeviceContext4* d3dCtx = ctx.GetContext();
    SpriteBatch& spriteBatch = SpriteBatch::Get();

    // 現在のレンダーターゲットを保存
    ComPtr<ID3D11RenderTargetView> savedRTV;
    ComPtr<ID3D11DepthStencilView> savedDSV;
    d3dCtx->OMGetRenderTargets(1, savedRTV.GetAddressOf(), savedDSV.GetAddressOf());

    D3D11_VIEWPORT savedViewport;
    UINT numViewports = 1;
    d3dCtx->RSGetViewports(&numViewports, &savedViewport);

    // ステージ全体をカバーする直交投影行列を作成（カメラなし）
    // 左上が(0,0)、右下が(stageWidth, stageHeight)
    DirectX::XMMATRIX orthoProj = DirectX::XMMatrixOrthographicOffCenterLH(
        0.0f, stageWidth_,      // left, right
        stageHeight_, 0.0f,     // bottom, top (Y軸反転)
        0.0f, 1.0f              // nearZ, farZ
    );
    DirectX::XMMATRIX transposed = DirectX::XMMatrixTranspose(orthoProj);
    Matrix viewProj;
    DirectX::XMStoreFloat4x4(reinterpret_cast<DirectX::XMFLOAT4X4*>(&viewProj), transposed);

    // === パス1: 蓄積パス（accumulationRTに加算ブレンドで描画） ===
    ctx.SetRenderTarget(accumulationRT_.get(), nullptr);
    ctx.SetViewport(0, 0, stageWidth_, stageHeight_);

    // 黒でクリア（蓄積開始点）
    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    ctx.ClearRenderTarget(accumulationRT_.get(), clearColor);

    // 固定投影行列を設定
    spriteBatch.SetViewProjection(viewProj);
    spriteBatch.SetCustomShaders(groundVertexShader_.get(), groundPixelShader_.get());
    spriteBatch.SetCustomBlendState(additiveBlendState_.get());
    spriteBatch.Begin();

    Vector2 origin(tileWidth_ * 0.5f, tileHeight_ * 0.5f);
    for (const GroundTile& tile : groundTiles_) {
        spriteBatch.Draw(
            groundTexture_.get(),
            tile.position,
            Color(1.0f, 1.0f, 1.0f, tile.alpha),
            tile.rotation,
            origin,
            Vector2::One,
            tile.flipX, tile.flipY,
            0, 0  // sortingLayerは不要
        );
    }

    spriteBatch.End();
    spriteBatch.ClearCustomShaders();
    spriteBatch.ClearCustomBlendState();

    // === パス2: 正規化パス（蓄積結果を正規化してbakedGroundTextureに描画） ===
    // ベイク済みテクスチャ用RTを作成（ステージサイズ）
    bakedGroundTexture_ = TextureManager::Get().CreateRenderTarget(
        static_cast<uint32_t>(stageWidth_),
        static_cast<uint32_t>(stageHeight_),
        DXGI_FORMAT_R8G8B8A8_UNORM);

    if (!bakedGroundTexture_) {
        LOG_ERROR("[StageBackground] Failed to create baked ground texture");
        // RTを復元して終了
        d3dCtx->OMSetRenderTargets(1, savedRTV.GetAddressOf(), savedDSV.Get());
        d3dCtx->RSSetViewports(1, &savedViewport);
        return;
    }

    ctx.SetRenderTarget(bakedGroundTexture_.get(), nullptr);
    ctx.SetViewport(0, 0, stageWidth_, stageHeight_);

    // 透明でクリア
    float clearTransparent[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    ctx.ClearRenderTarget(bakedGroundTexture_.get(), clearTransparent);

    // 正規化シェーダーで蓄積RTを描画
    spriteBatch.SetViewProjection(viewProj);
    spriteBatch.SetCustomShaders(nullptr, normalizePixelShader_.get());
    spriteBatch.Begin();

    // 蓄積RTをステージ全体に描画（ステージサイズで）
    Vector2 rtOrigin(stageWidth_ * 0.5f, stageHeight_ * 0.5f);
    spriteBatch.Draw(
        accumulationRT_.get(),
        Vector2(stageWidth_ * 0.5f, stageHeight_ * 0.5f),
        Colors::White,
        0.0f,
        rtOrigin,
        Vector2::One,
        false, false,
        0, 0
    );

    spriteBatch.End();
    spriteBatch.ClearCustomShaders();

    // === 復元 ===
    d3dCtx->OMSetRenderTargets(1, savedRTV.GetAddressOf(), savedDSV.Get());
    d3dCtx->RSSetViewports(1, &savedViewport);

    // ベイク完了後、不要なリソースを解放
    accumulationRT_.reset();
    groundTiles_.clear();

    LOG_INFO("[StageBackground] Ground texture baked successfully");

    // TODO: BC圧縮は緑色アーティファクトの問題があるため一時無効化
    // 非圧縮: 59MB VRAM使用
    LOG_INFO("[StageBackground] 非圧縮テクスチャを使用（59MB VRAM）");
}

//----------------------------------------------------------------------------
void StageBackground::SplitIntoChunks()
{
    if (!bakedGroundTexture_) {
        LOG_WARN("[StageBackground] No baked texture to split");
        return;
    }

    auto& ctx = GraphicsContext::Get();
    ID3D11DeviceContext4* d3dCtx = ctx.GetContext();

    // チャンク配列を初期化
    chunks_.clear();
    chunks_.reserve(kChunksX * kChunksY);

    for (int y = 0; y < kChunksY; ++y) {
        for (int x = 0; x < kChunksX; ++x) {
            GroundChunk chunk;
            chunk.position = Vector2(x * kChunkSize, y * kChunkSize);

            // チャンクサイズを計算（端のチャンクは小さい場合がある）
            uint32_t chunkW = static_cast<uint32_t>((std::min)(kChunkSize, stageWidth_ - x * kChunkSize));
            uint32_t chunkH = static_cast<uint32_t>((std::min)(kChunkSize, stageHeight_ - y * kChunkSize));

            // チャンク用テクスチャを作成
            chunk.texture = TextureManager::Get().CreateRenderTarget(
                static_cast<uint32_t>(kChunkSize),
                static_cast<uint32_t>(kChunkSize),
                DXGI_FORMAT_R8G8B8A8_UNORM);

            if (chunk.texture) {
                // ソース領域を定義
                D3D11_BOX srcBox;
                srcBox.left = static_cast<UINT>(x * kChunkSize);
                srcBox.top = static_cast<UINT>(y * kChunkSize);
                srcBox.front = 0;
                srcBox.right = srcBox.left + chunkW;
                srcBox.bottom = srcBox.top + chunkH;
                srcBox.back = 1;

                // CopySubresourceRegionでコピー
                d3dCtx->CopySubresourceRegion(
                    chunk.texture->Get(),
                    0,
                    0, 0, 0,
                    bakedGroundTexture_->Get(),
                    0,
                    &srcBox
                );
            }

            chunks_.push_back(std::move(chunk));
        }
    }

    // 元の大きいテクスチャを解放
    bakedGroundTexture_.reset();

    LOG_INFO("[StageBackground] Split into " + std::to_string(chunks_.size()) + " chunks");
}

//----------------------------------------------------------------------------
void StageBackground::Render(SpriteBatch& spriteBatch, [[maybe_unused]] const Camera2D& camera)
{
    // 1. ベース地面カラーを描画（1x1テクスチャをステージ全体にスケール）
    if (baseGroundTexture_) {
        Vector2 origin(0.5f, 0.5f);  // 1x1テクスチャの中心
        Vector2 scale(stageWidth_, stageHeight_);  // ステージ全体にスケール
        spriteBatch.Draw(
            baseGroundTexture_.get(),
            Vector2(stageWidth_ * 0.5f, stageHeight_ * 0.5f),
            Colors::White,
            0.0f,
            origin,
            scale,
            false, false,
            -99, 0
        );
    }

    // 2. ベイク済み地面テクスチャを描画（単一テクスチャ、継ぎ目なし）
    if (bakedGroundTexture_) {
        Vector2 origin(stageWidth_ * 0.5f, stageHeight_ * 0.5f);
        spriteBatch.Draw(
            bakedGroundTexture_.get(),
            Vector2(stageWidth_ * 0.5f, stageHeight_ * 0.5f),
            Colors::White,
            0.0f,
            origin,
            Vector2::One,
            false, false,
            -98, 0
        );
    }

    // 3. 装飾描画
    for (const DecorationObject& obj : decorations_) {
        if (!obj.texture) continue;

        float texW = static_cast<float>(obj.texture->Width());
        float texH = static_cast<float>(obj.texture->Height());
        Vector2 origin(texW * 0.5f, texH * 0.5f);

        spriteBatch.Draw(
            obj.texture.get(),
            obj.position,
            Color(1.0f, 1.0f, 1.0f, 1.0f),
            obj.rotation,
            origin,
            obj.scale,
            false, false,
            obj.sortingLayer, 0
        );
    }
}

//----------------------------------------------------------------------------
void StageBackground::Shutdown()
{
    // チャンクをクリア
    for (GroundChunk& chunk : chunks_) {
        chunk.texture.reset();
    }
    chunks_.clear();

    groundTiles_.clear();
    decorations_.clear();
    groundTexture_.reset();
    baseGroundTexture_.reset();
    groundVertexShader_.reset();
    groundPixelShader_.reset();
    normalizePixelShader_.reset();
    accumulationRT_.reset();
    additiveBlendState_.reset();
    clampSamplerState_.reset();

    LOG_INFO("[StageBackground] Shutdown");
}
