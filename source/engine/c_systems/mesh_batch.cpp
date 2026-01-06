//----------------------------------------------------------------------------
//! @file   mesh_batch.cpp
//! @brief  メッシュバッチレンダラー実装
//----------------------------------------------------------------------------

#include "mesh_batch.h"
#include "engine/component/camera3d.h"
#include "engine/component/mesh_renderer.h"
#include "engine/component/transform.h"
#include "engine/mesh/mesh_manager.h"
#include "engine/mesh/mesh.h"
#include "engine/material/material_manager.h"
#include "engine/material/material.h"
#include "engine/lighting/shadow_map.h"
#include "engine/shader/shader_manager.h"
#include "engine/graphics2d/render_state_manager.h"
#include "engine/texture/texture_manager.h"
#include "dx11/graphics_device.h"
#include "dx11/graphics_context.h"
#include "common/logging/logging.h"
#include <algorithm>

//============================================================================
// シングルトン
//============================================================================

MeshBatch& MeshBatch::Get()
{
    return *instance_;
}

void MeshBatch::Create()
{
    if (!instance_) {
        instance_ = std::unique_ptr<MeshBatch>(new MeshBatch());
    }
}

void MeshBatch::Destroy()
{
    instance_.reset();
}

MeshBatch::~MeshBatch()
{
    Shutdown();
}

//============================================================================
// 初期化・終了
//============================================================================

bool MeshBatch::Initialize()
{
    if (initialized_) {
        return true;
    }

    if (!CreateShaders()) {
        LOG_ERROR("[MeshBatch] シェーダー作成失敗");
        return false;
    }

    if (!CreateConstantBuffers()) {
        LOG_ERROR("[MeshBatch] 定数バッファ作成失敗");
        return false;
    }

    // ライティング初期化
    lightingConstants_ = {};
    lightingConstants_.ambientColor = Color(0.1f, 0.1f, 0.1f, 1.0f);
    lightingConstants_.numLights = 0;

    initialized_ = true;
    LOG_INFO("[MeshBatch] 初期化完了");
    return true;
}

void MeshBatch::Shutdown()
{
    if (!initialized_) {
        return;
    }

    drawQueue_.clear();
    perFrameBuffer_.reset();
    perObjectBuffer_.reset();
    lightingBuffer_.reset();
    shadowBuffer_.reset();
    shadowPassBuffer_.reset();
    vertexShader_.reset();
    pixelShader_.reset();
    shadowVertexShader_.reset();
    shadowPixelShader_.reset();
    inputLayout_.Reset();

    initialized_ = false;
    LOG_INFO("[MeshBatch] シャットダウン完了");
}

//============================================================================
// シェーダー・定数バッファ作成
//============================================================================

bool MeshBatch::CreateShaders()
{
    auto& shaderMgr = ShaderManager::Get();

    // メインパスシェーダー
    vertexShader_ = shaderMgr.LoadVertexShader("shaders:/mesh_vs.hlsl");
    if (!vertexShader_) {
        LOG_ERROR("[MeshBatch] mesh_vs.hlsl のロードに失敗");
        return false;
    }

    pixelShader_ = shaderMgr.LoadPixelShader("shaders:/mesh_ps.hlsl");
    if (!pixelShader_) {
        LOG_ERROR("[MeshBatch] mesh_ps.hlsl のロードに失敗");
        return false;
    }

    // シャドウパスシェーダー
    shadowVertexShader_ = shaderMgr.LoadVertexShader("shaders:/shadow_vs.hlsl");
    if (!shadowVertexShader_) {
        LOG_ERROR("[MeshBatch] shadow_vs.hlsl のロードに失敗");
        return false;
    }

    shadowPixelShader_ = shaderMgr.LoadPixelShader("shaders:/shadow_ps.hlsl");
    if (!shadowPixelShader_) {
        LOG_ERROR("[MeshBatch] shadow_ps.hlsl のロードに失敗");
        return false;
    }

    // 入力レイアウト（MeshVertex構造体に対応）
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0,  0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TANGENT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 40, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 48, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    inputLayout_ = shaderMgr.CreateInputLayout(
        vertexShader_.get(),
        layout,
        _countof(layout)
    );
    if (!inputLayout_) {
        LOG_ERROR("[MeshBatch] InputLayout作成失敗");
        return false;
    }

    return true;
}

bool MeshBatch::CreateConstantBuffers()
{
    // PerFrame (b0)
    perFrameBuffer_ = Buffer::CreateConstant(sizeof(PerFrameConstants));
    if (!perFrameBuffer_) {
        return false;
    }

    // PerObject (b1)
    perObjectBuffer_ = Buffer::CreateConstant(sizeof(PerObjectConstants));
    if (!perObjectBuffer_) {
        return false;
    }

    // Lighting (b3)
    lightingBuffer_ = Buffer::CreateConstant(sizeof(LightingConstants));
    if (!lightingBuffer_) {
        return false;
    }

    // Shadow (b4)
    shadowBuffer_ = Buffer::CreateConstant(sizeof(ShadowConstants));
    if (!shadowBuffer_) {
        return false;
    }

    // ShadowPass (b0 for shadow pass)
    shadowPassBuffer_ = Buffer::CreateConstant(sizeof(ShadowPassConstants));
    if (!shadowPassBuffer_) {
        return false;
    }

    return true;
}

//============================================================================
// カメラ設定
//============================================================================

void MeshBatch::SetCamera(const Camera3D& camera)
{
    viewMatrix_ = camera.GetViewMatrix();
    projectionMatrix_ = camera.GetProjectionMatrix();
    cameraPosition_ = camera.GetPosition();
}

void MeshBatch::SetViewProjection(const Matrix& view, const Matrix& projection)
{
    viewMatrix_ = view;
    projectionMatrix_ = projection;
    // カメラ位置はビュー行列の逆から計算（簡易）
    Matrix invView;
    view.Invert(invView);
    cameraPosition_ = Vector3(invView._41, invView._42, invView._43);
}

//============================================================================
// ライティング設定
//============================================================================

void MeshBatch::SetAmbientLight(const Color& color)
{
    lightingConstants_.ambientColor = color;
}

bool MeshBatch::AddLight(const LightData& light)
{
    if (lightingConstants_.numLights >= kMaxLights) {
        LOG_WARN("[MeshBatch] ライト数が最大値に達しています");
        return false;
    }
    lightingConstants_.lights[lightingConstants_.numLights] = light;
    lightingConstants_.numLights++;
    return true;
}

void MeshBatch::ClearLights()
{
    lightingConstants_.numLights = 0;
}

//============================================================================
// シャドウ設定
//============================================================================

void MeshBatch::SetShadowMap(ShadowMap* shadowMap)
{
    shadowMap_ = shadowMap;
}

//============================================================================
// 描画
//============================================================================

void MeshBatch::Begin()
{
    if (!initialized_) {
        LOG_ERROR("[MeshBatch] 初期化されていません");
        return;
    }

    if (isBegun_) {
        LOG_WARN("[MeshBatch] Begin()が二重呼び出しされました");
        return;
    }

    drawQueue_.clear();
    drawCallCount_ = 0;
    meshCount_ = 0;
    isBegun_ = true;
}

void MeshBatch::Draw(MeshHandle mesh, MaterialHandle material, const Matrix& world)
{
    if (!isBegun_) {
        LOG_WARN("[MeshBatch] Begin()が呼び出されていません");
        return;
    }

    if (!mesh.IsValid()) {
        return;
    }

    // メッシュを取得してサブメッシュごとにコマンドを追加
    Mesh* meshPtr = MeshManager::Get().Get(mesh);
    if (!meshPtr) {
        return;
    }

    const auto& subMeshes = meshPtr->GetSubMeshes();
    for (uint32_t i = 0; i < subMeshes.size(); ++i) {
        DrawCommand cmd;
        cmd.mesh = mesh;
        cmd.material = material;
        cmd.subMeshIndex = i;
        cmd.worldMatrix = world;

        // カメラからの距離を計算（ソート用）
        Vector3 meshCenter = Vector3(world._41, world._42, world._43);
        cmd.distanceToCamera = (meshCenter - cameraPosition_).LengthSquared();

        drawQueue_.push_back(cmd);
    }
}

void MeshBatch::Draw(const MeshRenderer& renderer, Transform& transform)
{
    if (!renderer.IsVisible()) {
        return;
    }

    MeshHandle mesh = renderer.GetMesh();
    if (!mesh.IsValid()) {
        return;
    }

    // ワールド行列を取得
    Matrix world = transform.GetWorldMatrix();

    // サブメッシュごとにマテリアルを適用
    Mesh* meshPtr = MeshManager::Get().Get(mesh);
    if (!meshPtr) {
        return;
    }

    const auto& subMeshes = meshPtr->GetSubMeshes();
    for (uint32_t i = 0; i < subMeshes.size(); ++i) {
        MaterialHandle material = renderer.GetMaterial(i);
        if (!material.IsValid() && renderer.GetMaterialCount() > 0) {
            material = renderer.GetMaterial(0);  // フォールバック
        }

        DrawCommand cmd;
        cmd.mesh = mesh;
        cmd.material = material;
        cmd.subMeshIndex = i;
        cmd.worldMatrix = world;

        Vector3 meshCenter = Vector3(world._41, world._42, world._43);
        cmd.distanceToCamera = (meshCenter - cameraPosition_).LengthSquared();

        drawQueue_.push_back(cmd);
    }
}

void MeshBatch::RenderShadowPass()
{
    if (!shadowMap_ || !shadowEnabled_) {
        return;
    }

    if (drawQueue_.empty()) {
        return;
    }

    auto& ctx = GraphicsContext::Get();
    auto* d3dCtx = ctx.GetContext();
    if (!d3dCtx) {
        return;
    }

    // シャドウマップにレンダリング
    shadowMap_->BeginShadowPass();

    // パイプライン設定
    ctx.SetInputLayout(inputLayout_.Get());
    ctx.SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // シャドウパスシェーダー
    ctx.SetVertexShader(shadowVertexShader_.get());
    ctx.SetPixelShader(shadowPixelShader_.get());

    // シャドウパス定数バッファ更新
    ShadowPassConstants shadowPass;
    shadowPass.lightViewProjection = shadowMap_->GetViewProjectionMatrix().Transpose();
    ctx.UpdateConstantBuffer(shadowPassBuffer_.get(), shadowPass);
    ctx.SetVSConstantBuffer(0, shadowPassBuffer_.get());

    // 各メッシュを描画
    for (const auto& cmd : drawQueue_) {
        RenderMeshShadow(cmd);
    }

    shadowMap_->EndShadowPass();
}

void MeshBatch::End()
{
    if (!isBegun_) {
        LOG_WARN("[MeshBatch] Begin()が呼び出されていません");
        return;
    }

    isBegun_ = false;

    if (drawQueue_.empty()) {
        return;
    }

    // ソートしてバッチ描画
    SortDrawCommands();
    FlushBatch();
}

//============================================================================
// 内部メソッド
//============================================================================

void MeshBatch::SortDrawCommands()
{
    // マテリアルでソート（状態変更を最小化）
    // 不透明は手前から、半透明は奥から描画
    std::stable_sort(drawQueue_.begin(), drawQueue_.end(),
        [](const DrawCommand& a, const DrawCommand& b) {
            // まずマテリアルでグループ化
            if (a.material.id != b.material.id) {
                return a.material.id < b.material.id;
            }
            // 同じマテリアルなら距離でソート
            return a.distanceToCamera < b.distanceToCamera;
        });
}

void MeshBatch::FlushBatch()
{
    auto& ctx = GraphicsContext::Get();
    auto* d3dCtx = ctx.GetContext();
    if (!d3dCtx) {
        return;
    }

    // パイプライン設定
    ctx.SetInputLayout(inputLayout_.Get());
    ctx.SetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // シェーダー設定
    ctx.SetVertexShader(vertexShader_.get());
    ctx.SetPixelShader(pixelShader_.get());

    // PerFrame定数バッファ更新
    PerFrameConstants perFrame;
    Matrix viewProj = viewMatrix_ * projectionMatrix_;
    perFrame.viewProjection = viewProj.Transpose();
    perFrame.cameraPosition = Vector4(cameraPosition_.x, cameraPosition_.y, cameraPosition_.z, 1.0f);
    ctx.UpdateConstantBuffer(perFrameBuffer_.get(), perFrame);

    ctx.SetVSConstantBuffer(0, perFrameBuffer_.get());
    ctx.SetPSConstantBuffer(0, perFrameBuffer_.get());

    // Lighting定数バッファ更新
    lightingConstants_.cameraPosition = perFrame.cameraPosition;
    ctx.UpdateConstantBuffer(lightingBuffer_.get(), lightingConstants_);
    ctx.SetPSConstantBuffer(3, lightingBuffer_.get());

    // Shadow定数バッファ更新
    ShadowConstants shadow;
    if (shadowMap_ && shadowEnabled_) {
        shadow.lightViewProjection = shadowMap_->GetViewProjectionMatrix().Transpose();
        shadow.shadowParams = Vector4(
            shadowMap_->GetDepthBias(),
            shadowMap_->GetNormalBias(),
            shadowStrength_,
            1.0f  // enabled
        );
        // シャドウマップをバインド
        ctx.SetPSShaderResource(5, shadowMap_->GetDepthTexture());
    } else {
        shadow.lightViewProjection = Matrix::Identity;
        shadow.shadowParams = Vector4(0.0f, 0.0f, 0.0f, 0.0f);  // disabled
    }
    ctx.UpdateConstantBuffer(shadowBuffer_.get(), shadow);
    ctx.SetPSConstantBuffer(4, shadowBuffer_.get());

    // サンプラー設定
    auto& rsm = RenderStateManager::Get();
    ctx.SetPSSampler(0, rsm.GetLinearWrap());

    // 各メッシュを描画
    MaterialHandle currentMaterial = MaterialHandle::Invalid();

    for (const auto& cmd : drawQueue_) {
        // マテリアル変更
        if (cmd.material.id != currentMaterial.id) {
            currentMaterial = cmd.material;
            Material* mat = MaterialManager::Get().Get(currentMaterial);
            if (mat) {
                // 定数バッファ更新
                mat->UpdateConstantBuffer();
                ctx.SetPSConstantBuffer(2, mat->GetConstantBuffer());

                // テクスチャバインド
                BindMaterialTextures(mat);
            }
        }

        RenderMesh(cmd);
    }

    meshCount_ = static_cast<uint32_t>(drawQueue_.size());
}

void MeshBatch::BindMaterialTextures(Material* mat)
{
    auto& ctx = GraphicsContext::Get();
    auto& texMgr = TextureManager::Get();

    // Albedo (t0)
    TextureHandle albedoHandle = mat->GetTexture(MaterialTextureSlot::Albedo);
    if (albedoHandle.IsValid()) {
        Texture* tex = texMgr.Get(albedoHandle);
        if (tex) {
            ctx.SetPSShaderResource(0, tex);
        }
    }

    // Normal (t1)
    TextureHandle normalHandle = mat->GetTexture(MaterialTextureSlot::Normal);
    if (normalHandle.IsValid()) {
        Texture* tex = texMgr.Get(normalHandle);
        if (tex) {
            ctx.SetPSShaderResource(1, tex);
        }
    }

    // Metallic (t2)
    TextureHandle metallicHandle = mat->GetTexture(MaterialTextureSlot::Metallic);
    if (metallicHandle.IsValid()) {
        Texture* tex = texMgr.Get(metallicHandle);
        if (tex) {
            ctx.SetPSShaderResource(2, tex);
        }
    }

    // Roughness (t3)
    TextureHandle roughnessHandle = mat->GetTexture(MaterialTextureSlot::Roughness);
    if (roughnessHandle.IsValid()) {
        Texture* tex = texMgr.Get(roughnessHandle);
        if (tex) {
            ctx.SetPSShaderResource(3, tex);
        }
    }

    // AO (t4)
    TextureHandle aoHandle = mat->GetTexture(MaterialTextureSlot::AO);
    if (aoHandle.IsValid()) {
        Texture* tex = texMgr.Get(aoHandle);
        if (tex) {
            ctx.SetPSShaderResource(4, tex);
        }
    }
}

void MeshBatch::RenderMesh(const DrawCommand& cmd)
{
    auto& ctx = GraphicsContext::Get();
    auto* d3dCtx = ctx.GetContext();

    Mesh* mesh = MeshManager::Get().Get(cmd.mesh);
    if (!mesh) {
        return;
    }

    // PerObject定数バッファ更新
    PerObjectConstants perObject;
    perObject.world = cmd.worldMatrix.Transpose();
    Matrix invWorld;
    cmd.worldMatrix.Invert(invWorld);
    perObject.worldInvTranspose = invWorld.Transpose();
    ctx.UpdateConstantBuffer(perObjectBuffer_.get(), perObject);

    ctx.SetVSConstantBuffer(1, perObjectBuffer_.get());

    // 頂点バッファ設定
    Buffer* vb = mesh->GetVertexBuffer();
    if (vb) {
        UINT stride = sizeof(MeshVertex);
        UINT offset = 0;
        ID3D11Buffer* buffers[] = { vb->Get() };
        d3dCtx->IASetVertexBuffers(0, 1, buffers, &stride, &offset);
    }

    // インデックスバッファ設定
    Buffer* ib = mesh->GetIndexBuffer();
    if (ib) {
        d3dCtx->IASetIndexBuffer(ib->Get(), DXGI_FORMAT_R32_UINT, 0);
    }

    // サブメッシュ描画
    const auto& subMeshes = mesh->GetSubMeshes();
    if (cmd.subMeshIndex < subMeshes.size()) {
        const SubMesh& sub = subMeshes[cmd.subMeshIndex];
        d3dCtx->DrawIndexed(sub.indexCount, sub.indexOffset, 0);
        drawCallCount_++;
    }
}

void MeshBatch::RenderMeshShadow(const DrawCommand& cmd)
{
    auto& ctx = GraphicsContext::Get();
    auto* d3dCtx = ctx.GetContext();

    Mesh* mesh = MeshManager::Get().Get(cmd.mesh);
    if (!mesh) {
        return;
    }

    // PerObject定数バッファ更新
    PerObjectConstants perObject;
    perObject.world = cmd.worldMatrix.Transpose();
    Matrix invWorld;
    cmd.worldMatrix.Invert(invWorld);
    perObject.worldInvTranspose = invWorld.Transpose();
    ctx.UpdateConstantBuffer(perObjectBuffer_.get(), perObject);

    ctx.SetVSConstantBuffer(1, perObjectBuffer_.get());

    // 頂点バッファ設定
    Buffer* vb = mesh->GetVertexBuffer();
    if (vb) {
        UINT stride = sizeof(MeshVertex);
        UINT offset = 0;
        ID3D11Buffer* buffers[] = { vb->Get() };
        d3dCtx->IASetVertexBuffers(0, 1, buffers, &stride, &offset);
    }

    // インデックスバッファ設定
    Buffer* ib = mesh->GetIndexBuffer();
    if (ib) {
        d3dCtx->IASetIndexBuffer(ib->Get(), DXGI_FORMAT_R32_UINT, 0);
    }

    // サブメッシュ描画
    const auto& subMeshes = mesh->GetSubMeshes();
    if (cmd.subMeshIndex < subMeshes.size()) {
        const SubMesh& sub = subMeshes[cmd.subMeshIndex];
        d3dCtx->DrawIndexed(sub.indexCount, sub.indexOffset, 0);
    }
}
