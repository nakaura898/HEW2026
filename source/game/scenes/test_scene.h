//----------------------------------------------------------------------------
//! @file   test_scene.h
//! @brief  テストシーン（Animatorテスト）
//----------------------------------------------------------------------------
#pragma once

#include "engine/scene/scene.h"
#include "engine/component/game_object.h"
#include "engine/component/transform2d.h"
#include "engine/component/sprite_renderer.h"
#include "engine/component/camera2d.h"
#include "engine/component/animator.h"
#include "dx11/gpu/texture.h"
#include <memory>

//----------------------------------------------------------------------------
//! @brief テストシーン（Animatorテスト）
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

    // 背景
    std::unique_ptr<GameObject> background_;
    Transform2D* bgTransform_ = nullptr;
    SpriteRenderer* bgSprite_ = nullptr;

    // アニメーションスプライト
    std::unique_ptr<GameObject> sprite_;
    Transform2D* spriteTransform_ = nullptr;
    SpriteRenderer* spriteRenderer_ = nullptr;
    Animator* animator_ = nullptr;

    // テクスチャ
    TexturePtr backgroundTexture_;
    TexturePtr spriteTexture_;
};
