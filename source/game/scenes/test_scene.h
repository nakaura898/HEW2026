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
#include "game/stage/stage_background.h"
#include "game/entities/group.h"
#include "game/ai/group_ai.h"
#include "game/bond/bondable_entity.h"
#include "game/bond/bond.h"
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

    //! @brief プレイヤーが触れているグループを取得（1つだけの場合）
    //! @return 触れているGroup（1つだけ）、複数or0ならnullptr
    [[nodiscard]] Group* GetGroupTouchingPlayer() const;

    //! @brief モード表示テキストを取得
    [[nodiscard]] const char* GetModeText() const;

    //! @brief ゲーム状態表示テキストを取得
    [[nodiscard]] const char* GetStateText() const;

    //! @brief 縁をデバッグ描画
    void DrawBonds();

#ifdef _DEBUG
    //! @brief 索敵範囲を描画（デバッグ用）
    void DrawDetectionRanges();

    //! @brief 個体のコライダーを描画（デバッグ用）
    void DrawIndividualColliders();
#endif

    //! @brief UI情報を描画
    void DrawUI();

    //! @brief AIステータスをログ出力
    void LogAIStatus();

    float time_ = 0.0f;
    float statusLogTimer_ = 0.0f;       //!< ステータスログ用タイマー
    float statusLogInterval_ = 3.0f;    //!< ステータスログ間隔（秒）

    // FPS計測
    float fpsTimer_ = 0.0f;
    int frameCount_ = 0;
    float currentFps_ = 0.0f;

    // カメラ
    std::unique_ptr<GameObject> cameraObj_;
    Camera2D* camera_ = nullptr;

    // プレイヤー
    std::unique_ptr<Player> player_;

    // 敵グループ
    std::vector<std::unique_ptr<Group>> enemyGroups_;

    // グループAI
    std::vector<std::unique_ptr<GroupAI>> groupAIs_;

    // ステージ背景
    StageBackground stageBackground_;

    // UI用テクスチャ
    TexturePtr whiteTexture_;

    // 画面サイズ
    float screenWidth_ = 0.0f;
    float screenHeight_ = 0.0f;

    // EventBus購読ID
    std::vector<uint32_t> eventSubscriptions_;

    // EventBus購読を設定
    void SetupEventSubscriptions();

    //------------------------------------------------------------------------
    // 縁タイプ選択UI
    //------------------------------------------------------------------------

    //! @brief ラジアルメニューを表示するか
    bool showRadialMenu_ = false;

    //! @brief ラジアルメニュー表示中心位置（スクリーン座標）
    Vector2 radialMenuCenter_ = Vector2::Zero;

    //! @brief 選択中の縁タイプ
    BondType selectedBondType_ = BondType::Basic;

    //! @brief プレイヤーとグループを結ぶ（簡易操作）
    void BindPlayerToGroup(Group* group);

    //! @brief デバッグ描画表示フラグ（F1で切替）
    bool showDebugDraw_ = true;
};
