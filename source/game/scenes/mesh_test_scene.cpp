//----------------------------------------------------------------------------
//! @file   mesh_test_scene.cpp
//! @brief  3Dメッシュレンダリングテストシーン 実装
//----------------------------------------------------------------------------
#include "mesh_test_scene.h"
#include "engine/mesh/mesh_manager.h"
#include "engine/material/material_manager.h"
#include "engine/c_systems/mesh_batch.h"
#include "engine/lighting/lighting_manager.h"
#include "engine/input/input_manager.h"
#include "engine/component/transform.h"
#include "engine/platform/renderer.h"
#include "engine/time/time_manager.h"
#include "dx11/graphics_context.h"
#include "common/logging/logging.h"
#include <cmath>
#include <algorithm>

//----------------------------------------------------------------------------
void MeshTestScene::OnEnter()
{
    LOG_INFO("[MeshTestScene] シーン開始");

    // カメラ作成
    cameraObj_ = std::make_unique<GameObject>("Camera3D");
    camera_ = cameraObj_->AddComponent<Camera3D>();
    camera_->SetFOV(60.0f);
    camera_->SetAspectRatio(16.0f / 9.0f);
    camera_->SetNearPlane(0.1f);
    camera_->SetFarPlane(1000.0f);

    // プリミティブメッシュ生成
    boxMesh_ = MeshManager::Get().CreateBox(Vector3(2.0f, 2.0f, 2.0f));
    sphereMesh_ = MeshManager::Get().CreateSphere(1.0f, 24);
    planeMesh_ = MeshManager::Get().CreatePlane(20.0f, 20.0f, 1, 1);
    cylinderMesh_ = MeshManager::Get().CreateCylinder(0.5f, 3.0f, 24);

    // マテリアル作成
    MaterialDesc redDesc;
    redDesc.params.albedoColor = Color(1.0f, 0.2f, 0.2f, 1.0f);
    redDesc.params.roughness = 0.5f;
    redDesc.params.metallic = 0.0f;
    redMaterial_ = MaterialManager::Get().Create(redDesc);

    MaterialDesc greenDesc;
    greenDesc.params.albedoColor = Color(0.2f, 1.0f, 0.2f, 1.0f);
    greenDesc.params.roughness = 0.3f;
    greenDesc.params.metallic = 0.0f;
    greenMaterial_ = MaterialManager::Get().Create(greenDesc);

    MaterialDesc blueDesc;
    blueDesc.params.albedoColor = Color(0.2f, 0.4f, 1.0f, 1.0f);
    blueDesc.params.roughness = 0.2f;
    blueDesc.params.metallic = 0.8f;
    blueMaterial_ = MaterialManager::Get().Create(blueDesc);

    MaterialDesc whiteDesc;
    whiteDesc.params.albedoColor = Color(0.9f, 0.9f, 0.9f, 1.0f);
    whiteDesc.params.roughness = 0.7f;
    whiteDesc.params.metallic = 0.0f;
    whiteMaterial_ = MaterialManager::Get().Create(whiteDesc);

    MaterialDesc groundDesc;
    groundDesc.params.albedoColor = Color(0.4f, 0.4f, 0.4f, 1.0f);
    groundDesc.params.roughness = 0.9f;
    groundDesc.params.metallic = 0.0f;
    groundMaterial_ = MaterialManager::Get().Create(groundDesc);

    // オブジェクト配置
    objectTransforms_.clear();

    // 地面（Y=0）
    Matrix groundTransform = Matrix::CreateTranslation(0.0f, 0.0f, 0.0f);
    objectTransforms_.push_back(groundTransform);

    // 赤いボックス
    Matrix boxTransform = Matrix::CreateTranslation(-3.0f, 1.0f, 0.0f);
    objectTransforms_.push_back(boxTransform);

    // 緑の球
    Matrix sphereTransform = Matrix::CreateTranslation(0.0f, 1.0f, 0.0f);
    objectTransforms_.push_back(sphereTransform);

    // 青いシリンダー
    Matrix cylinderTransform = Matrix::CreateTranslation(3.0f, 1.5f, 0.0f);
    objectTransforms_.push_back(cylinderTransform);

    // 白いボックス（後ろ）
    Matrix backBoxTransform = Matrix::CreateTranslation(0.0f, 1.0f, -4.0f);
    objectTransforms_.push_back(backBoxTransform);

    // シャドウマップ作成
    ShadowMapSettings shadowSettings;
    shadowSettings.resolution = 2048;
    shadowSettings.orthoSize = 25.0f;
    shadowSettings.nearPlane = 1.0f;
    shadowSettings.farPlane = 50.0f;
    shadowMap_ = ShadowMap::Create(shadowSettings);

    // ライト設定
    lightDirection_.Normalize();
    shadowMap_->SetDirectionalLight(lightDirection_, Vector3::Zero);

    // ライティングマネージャー設定
    LightingManager::Get().SetAmbientColor(Color(0.15f, 0.15f, 0.2f, 1.0f));
    LightingManager::Get().ClearAllLights();

    (void)LightingManager::Get().AddDirectionalLight(
        lightDirection_,
        Color(1.0f, 0.95f, 0.9f, 1.0f),
        1.5f
    );

    // MeshBatchにシャドウマップを設定
    MeshBatch::Get().SetShadowMap(shadowMap_.get());
    MeshBatch::Get().SetShadowEnabled(true);
    MeshBatch::Get().SetShadowStrength(0.7f);

    LOG_INFO("[MeshTestScene] 初期化完了");
}

//----------------------------------------------------------------------------
void MeshTestScene::OnExit()
{
    LOG_INFO("[MeshTestScene] シーン終了");

    MeshBatch::Get().SetShadowMap(nullptr);
    shadowMap_.reset();

    objectTransforms_.clear();

    // ハンドルは自動解放（スコープベース）
    cameraObj_.reset();
    camera_ = nullptr;
}

//----------------------------------------------------------------------------
void MeshTestScene::Update()
{
    float dt = TimeManager::Get().GetDeltaTime();
    time_ += dt;

    HandleInput(dt);
    UpdateOrbitCamera(dt);

    // オブジェクトを少し回転させる（アニメーション）
    if (objectTransforms_.size() >= 5) {
        // ボックス回転
        float boxAngle = time_ * 0.5f;
        objectTransforms_[1] = Matrix::CreateRotationY(boxAngle) *
                               Matrix::CreateTranslation(-3.0f, 1.0f, 0.0f);

        // 球は上下に浮遊
        float sphereY = 1.0f + std::sin(time_ * 2.0f) * 0.3f;
        objectTransforms_[2] = Matrix::CreateTranslation(0.0f, sphereY, 0.0f);

        // シリンダー回転
        float cylinderAngle = time_ * 0.3f;
        objectTransforms_[3] = Matrix::CreateRotationY(cylinderAngle) *
                               Matrix::CreateTranslation(3.0f, 1.5f, 0.0f);
    }

    cameraObj_->Update(dt);
}

//----------------------------------------------------------------------------
void MeshTestScene::Render()
{
    auto& ctx = GraphicsContext::Get();
    auto& renderer = Renderer::Get();

    // バックバッファと深度バッファを取得
    Texture* backBuffer = renderer.GetBackBuffer();
    Texture* depthBuffer = renderer.GetDepthBuffer();

    if (!backBuffer || !depthBuffer) {
        return;
    }

    // レンダーターゲット設定
    ctx.SetRenderTarget(backBuffer, depthBuffer);
    ctx.SetViewport(0.0f, 0.0f,
                    static_cast<float>(backBuffer->Width()),
                    static_cast<float>(backBuffer->Height()));

    // 背景クリア（暗めの青空）
    float clearColor[4] = { 0.1f, 0.15f, 0.25f, 1.0f };
    ctx.ClearRenderTarget(backBuffer, clearColor);
    ctx.ClearDepthStencil(depthBuffer, 1.0f, 0);

    // MeshBatch描画
    MeshBatch::Get().SetCamera(*camera_);
    MeshBatch::Get().Begin();

    // 地面
    MeshBatch::Get().Draw(planeMesh_, groundMaterial_, objectTransforms_[0]);

    // オブジェクト
    MeshBatch::Get().Draw(boxMesh_, redMaterial_, objectTransforms_[1]);
    MeshBatch::Get().Draw(sphereMesh_, greenMaterial_, objectTransforms_[2]);
    MeshBatch::Get().Draw(cylinderMesh_, blueMaterial_, objectTransforms_[3]);
    MeshBatch::Get().Draw(boxMesh_, whiteMaterial_, objectTransforms_[4]);

    // シャドウパス → メインパス
    MeshBatch::Get().RenderShadowPass();
    MeshBatch::Get().End();
}

//----------------------------------------------------------------------------
void MeshTestScene::HandleInput(float dt)
{
    auto& input = InputManager::Get();

    // ESCでタイトルに戻る
    if (input.GetKeyboard().IsKeyPressed(Key::Escape)) {
        LOG_INFO("[MeshTestScene] ESCキーが押されました");
    }

    // WASDでカメラターゲット移動
    float moveSpeed = 5.0f * dt;
    if (input.GetKeyboard().IsKeyDown(Key::W)) {
        cameraTarget_.z -= moveSpeed;
    }
    if (input.GetKeyboard().IsKeyDown(Key::S)) {
        cameraTarget_.z += moveSpeed;
    }
    if (input.GetKeyboard().IsKeyDown(Key::A)) {
        cameraTarget_.x -= moveSpeed;
    }
    if (input.GetKeyboard().IsKeyDown(Key::D)) {
        cameraTarget_.x += moveSpeed;
    }

    // マウスドラッグでカメラ回転
    auto& mouse = input.GetMouse();
    if (mouse.IsButtonDown(MouseButton::Right)) {
        float sensitivity = 0.3f;
        cameraYaw_ += mouse.GetDeltaX() * sensitivity;
        cameraPitch_ -= mouse.GetDeltaY() * sensitivity;
        cameraPitch_ = std::clamp(cameraPitch_, -89.0f, 89.0f);
    }

    // マウスホイールでズーム
    float scroll = mouse.GetWheelDelta();
    if (scroll != 0.0f) {
        cameraDistance_ -= scroll * 0.1f;
        cameraDistance_ = std::clamp(cameraDistance_, 3.0f, 50.0f);
    }
}

//----------------------------------------------------------------------------
void MeshTestScene::UpdateOrbitCamera(float dt)
{
    (void)dt;

    // 球面座標からカメラ位置を計算
    float yawRad = cameraYaw_ * 3.14159265f / 180.0f;
    float pitchRad = cameraPitch_ * 3.14159265f / 180.0f;

    float x = cameraDistance_ * std::cos(pitchRad) * std::sin(yawRad);
    float y = cameraDistance_ * std::sin(pitchRad);
    float z = cameraDistance_ * std::cos(pitchRad) * std::cos(yawRad);

    Vector3 cameraPos = cameraTarget_ + Vector3(x, y, z);

    // カメラのビュー行列を更新
    camera_->SetPosition(cameraPos);
    camera_->LookAt(cameraTarget_, Vector3::Up);
}
