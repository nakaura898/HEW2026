//----------------------------------------------------------------------------
//! @file   circle_renderer.cpp
//! @brief  円描画クラス実装
//----------------------------------------------------------------------------
#include "circle_renderer.h"

#ifdef _DEBUG

#include "dx11/graphics_context.h"
#include "engine/shader/shader_manager.h"
#include "engine/component/camera2d.h"
#include "common/logging/logging.h"
#include <d3d11.h>

//----------------------------------------------------------------------------
CircleRenderer& CircleRenderer::Get()
{
    static CircleRenderer instance;
    return instance;
}

//----------------------------------------------------------------------------
bool CircleRenderer::Initialize()
{
    if (initialized_) return true;

    ShaderManager& shaderMgr = ShaderManager::Get();

    // シェーダー読み込み
    vertexShader_ = shaderMgr.LoadVertexShader("sprite_vs.hlsl");
    pixelShader_ = shaderMgr.LoadPixelShader("circle_ps.hlsl");

    if (!vertexShader_ || !pixelShader_) {
        LOG_ERROR("[CircleRenderer] シェーダーの読み込みに失敗");
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
        LOG_ERROR("[CircleRenderer] 入力レイアウト作成失敗");
        return false;
    }

    // 頂点バッファ（1つの四角形 = 4頂点）
    vertexBuffer_ = Buffer::CreateVertex(
        sizeof(CircleVertex) * 4,
        sizeof(CircleVertex),
        true,    // dynamic
        nullptr  // no initial data
    );

    // インデックスバッファ
    uint16_t indices[6] = { 0, 1, 2, 2, 1, 3 };
    indexBuffer_ = Buffer::CreateIndex(sizeof(indices), false, indices);

    // 定数バッファ
    constantBuffer_ = Buffer::CreateConstant(sizeof(Matrix));

    // パイプラインステート
    blendState_ = BlendState::CreateAlphaBlend();
    samplerState_ = SamplerState::CreateDefault();
    rasterizerState_ = RasterizerState::CreateNoCull();
    depthStencilState_ = DepthStencilState::CreateLessEqual();

    if (!blendState_ || !samplerState_ || !rasterizerState_ || !depthStencilState_) {
        LOG_ERROR("[CircleRenderer] パイプラインステート作成失敗");
        return false;
    }

    initialized_ = true;
    LOG_INFO("[CircleRenderer] 初期化完了");
    return true;
}

//----------------------------------------------------------------------------
void CircleRenderer::Shutdown()
{
    vertexShader_.reset();
    pixelShader_.reset();
    inputLayout_.Reset();
    vertexBuffer_.reset();
    indexBuffer_.reset();
    constantBuffer_.reset();
    blendState_.reset();
    samplerState_.reset();
    rasterizerState_.reset();
    depthStencilState_.reset();
    instances_.clear();
    initialized_ = false;
}

//----------------------------------------------------------------------------
void CircleRenderer::Begin(const Camera2D& camera)
{
    if (!initialized_) {
        if (!Initialize()) {
            return;
        }
    }

    instances_.clear();
    isBegun_ = true;

    // ビュープロジェクション行列を取得
    constantData_ = Matrix(camera.GetViewProjectionMatrix());
}

//----------------------------------------------------------------------------
void CircleRenderer::DrawFilled(
    const Vector2& center,
    float radius,
    const Color& color)
{
    if (!isBegun_) return;

    instances_.push_back({ center, radius, color });
}

//----------------------------------------------------------------------------
void CircleRenderer::End()
{
    if (!isBegun_ || instances_.empty()) {
        isBegun_ = false;
        return;
    }

    GraphicsContext& ctx = GraphicsContext::Get();

    // 定数バッファ更新
    ctx.UpdateBuffer(constantBuffer_.get(), &constantData_, sizeof(Matrix));

    // パイプライン設定
    ctx.SetInputLayout(inputLayout_.Get());
    ctx.SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    ctx.SetVertexBuffer(0, vertexBuffer_.get(), sizeof(CircleVertex));
    ctx.SetIndexBuffer(indexBuffer_.get(), DXGI_FORMAT_R16_UINT, 0);

    ctx.SetVertexShader(vertexShader_.get());
    ctx.SetVSConstantBuffer(0, constantBuffer_.get());

    ctx.SetPixelShader(pixelShader_.get());
    ctx.SetPSSampler(0, samplerState_.get());

    ctx.SetBlendState(blendState_.get());
    ctx.SetDepthStencilState(depthStencilState_.get());
    ctx.SetRasterizerState(rasterizerState_.get());

    // 各円を描画
    for (const CircleInstance& inst : instances_) {
        float left = inst.center.x - inst.radius;
        float top = inst.center.y - inst.radius;
        float right = inst.center.x + inst.radius;
        float bottom = inst.center.y + inst.radius;
        float z = 0.85f;  // 手前に描画

        CircleVertex vertices[4] = {
            { Vector3(left, top, z),     Vector2(0.0f, 0.0f), inst.color },
            { Vector3(right, top, z),    Vector2(1.0f, 0.0f), inst.color },
            { Vector3(left, bottom, z),  Vector2(0.0f, 1.0f), inst.color },
            { Vector3(right, bottom, z), Vector2(1.0f, 1.0f), inst.color }
        };

        auto* mappedVerts = static_cast<CircleVertex*>(ctx.MapBuffer(vertexBuffer_.get()));
        if (mappedVerts) {
            memcpy(mappedVerts, vertices, sizeof(vertices));
            ctx.UnmapBuffer(vertexBuffer_.get());
        }

        ctx.DrawIndexed(6);
    }

    instances_.clear();
    isBegun_ = false;
}

#endif // _DEBUG
