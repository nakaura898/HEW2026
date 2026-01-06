//----------------------------------------------------------------------------
//! @file   mesh_test_scene.h
//! @brief  3Dメッシュレンダリングテストシーン
//----------------------------------------------------------------------------
#pragma once

#include "engine/scene/scene.h"
#include "engine/component/game_object.h"
#include "engine/component/camera3d.h"
#include "engine/mesh/mesh_handle.h"
#include "engine/material/material_handle.h"
#include "engine/lighting/shadow_map.h"
#include "engine/math/math_types.h"
#include <memory>
#include <vector>

//============================================================================
//! @brief 3Dメッシュテストシーン
//! @details MeshBatch、ライティング、シャドウマップの動作確認用
//============================================================================
class MeshTestScene : public Scene
{
public:
    void OnEnter() override;
    void OnExit() override;
    void Update() override;
    void Render() override;
    [[nodiscard]] const char* GetName() const override { return "MeshTestScene"; }

private:
    //! @brief 入力処理
    void HandleInput(float dt);

    //! @brief カメラの軌道制御
    void UpdateOrbitCamera(float dt);

    float time_ = 0.0f;

    // カメラ
    std::unique_ptr<GameObject> cameraObj_;
    Camera3D* camera_ = nullptr;

    // カメラ軌道パラメータ
    float cameraDistance_ = 15.0f;
    float cameraYaw_ = 0.0f;
    float cameraPitch_ = 30.0f;  // 度
    Vector3 cameraTarget_ = Vector3::Zero;

    // メッシュハンドル
    MeshHandle boxMesh_;
    MeshHandle sphereMesh_;
    MeshHandle planeMesh_;
    MeshHandle cylinderMesh_;

    // マテリアルハンドル
    MaterialHandle redMaterial_;
    MaterialHandle greenMaterial_;
    MaterialHandle blueMaterial_;
    MaterialHandle whiteMaterial_;
    MaterialHandle groundMaterial_;

    // シャドウマップ
    std::unique_ptr<ShadowMap> shadowMap_;

    // ライト方向
    Vector3 lightDirection_ = Vector3(0.5f, -1.0f, 0.3f);

    // オブジェクトのワールド行列
    std::vector<Matrix> objectTransforms_;
};
