//----------------------------------------------------------------------------
//! @file   test_scene.h
//! @brief  テストシーン
//----------------------------------------------------------------------------
#pragma once

#include "engine/scene/scene.h"
#include "engine/component/game_object.h"
#include "engine/component/transform2d.h"
#include "engine/component/sprite_renderer.h"
#include "engine/component/camera2d.h"
#include "engine/component/collider2d.h"
#include "dx11/gpu/texture.h"
#include <memory>
#include <vector>

//----------------------------------------------------------------------------
//! @brief テストシーン
//----------------------------------------------------------------------------
class TestScene : public Scene
{
public:
    void OnEnter() override;
    void OnExit() override;
    void Update() override;
    void Render() override;
    [[nodiscard]] const char* GetName() const override { return "TestScene"; }

private:
    float time_ = 0.0f;

    // カメラ
    std::unique_ptr<GameObject> cameraObj_;
    Camera2D* camera_ = nullptr;

    // プレイヤー（衝突テスト用）
    std::unique_ptr<GameObject> player_;
    Transform2D* playerTransform_ = nullptr;
    SpriteRenderer* playerSprite_ = nullptr;

    // 障害物オブジェクト
    std::vector<std::unique_ptr<GameObject>> objects_;

    // テスト用テクスチャ
    TexturePtr testTexture_;

    // 衝突カウント（デバッグ用）
    int collisionCount_ = 0;
};
