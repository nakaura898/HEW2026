//----------------------------------------------------------------------------
//! @file   test_scene.h
//! @brief  テストシーン - A-RAS!ゲームプロトタイプ
//----------------------------------------------------------------------------
#pragma once

#include "engine/scene/scene.h"
#include "engine/component/game_object.h"
#include "engine/component/transform2d.h"
#include "engine/component/sprite_renderer.h"
#include "engine/component/camera2d.h"
#include "engine/component/collider2d.h"
#include "dx11/gpu/texture.h"
#include "game/entities/player.h"
#include "game/entities/group.h"
#include "game/ai/group_ai.h"
#include "game/bond/bondable_entity.h"
#include <memory>
#include <vector>
#include <cstdint>

//----------------------------------------------------------------------------
//! @brief テストシーン - A-RAS!ゲームプロトタイプ
//! @details プレイヤー、敵グループ、縁システムの統合テスト
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
    //! @brief 入力処理
    void HandleInput(float dt);

    //! @brief マウスカーソル下のグループを取得
    //! @return カーソル下のGroup、なければnullptr
    [[nodiscard]] Group* GetGroupUnderCursor() const;

    //! @brief モード表示テキストを取得
    [[nodiscard]] const char* GetModeText() const;

    //! @brief ゲーム状態表示テキストを取得
    [[nodiscard]] const char* GetStateText() const;

    //! @brief 縁をデバッグ描画
    void DrawBonds();

    //! @brief UI情報を描画
    void DrawUI();

    //! @brief AIステータスをログ出力
    void LogAIStatus();

    float time_ = 0.0f;
    float statusLogTimer_ = 0.0f;       //!< ステータスログ用タイマー
    float statusLogInterval_ = 3.0f;    //!< ステータスログ間隔（秒）

    // カメラ
    std::unique_ptr<GameObject> cameraObj_;
    Camera2D* camera_ = nullptr;

    // プレイヤー
    std::unique_ptr<Player> player_;

    // 敵グループ
    std::vector<std::unique_ptr<Group>> enemyGroups_;

    // グループAI
    std::vector<std::unique_ptr<GroupAI>> groupAIs_;

    // 背景
    std::unique_ptr<GameObject> background_;
    Transform2D* bgTransform_ = nullptr;
    SpriteRenderer* bgSprite_ = nullptr;
    TexturePtr backgroundTexture_;

    // UI用テクスチャ
    TexturePtr whiteTexture_;

    // 画面サイズ
    float screenWidth_ = 0.0f;
    float screenHeight_ = 0.0f;

    // EventBus購読ID
    std::vector<uint32_t> eventSubscriptions_;

    // EventBus購読を設定
    void SetupEventSubscriptions();
};
