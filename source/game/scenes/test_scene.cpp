//----------------------------------------------------------------------------
//! @file   test_scene.cpp
//! @brief  テストシーン実装 - A-RAS!ゲームプロトタイプ
//----------------------------------------------------------------------------
#include "test_scene.h"
#include "result_scene.h"
#include "dx11/graphics_context.h"
#include "engine/scene/scene_manager.h"
#include "engine/platform/renderer.h"
#include "engine/platform/application.h"
#include "engine/texture/texture_manager.h"
#include "engine/c_systems/sprite_batch.h"
#include "engine/c_systems/collision_manager.h"
#include "engine/c_systems/collision_layers.h"
#include "engine/debug/debug_draw.h"
#include "engine/debug/circle_renderer.h"
#include "engine/math/color.h"
#include "engine/input/input_manager.h"
#include "common/logging/logging.h"

// ゲームシステム
#include "game/entities/elf.h"
#include "game/entities/knight.h"
#include "game/entities/arrow_manager.h"
#include "game/bond/bond_manager.h"
#include "engine/time/time_manager.h"
#include "game/systems/bind_system.h"
#include "game/systems/cut_system.h"
#include "game/systems/combat_system.h"
#include "game/systems/combat_mediator.h"
#include "game/systems/relationship_context.h"
#include "game/systems/game_state_manager.h"
#include "game/systems/fe_system.h"
#include "game/systems/stagger_system.h"
#include "game/systems/insulation_system.h"
#include "game/systems/faction_manager.h"
#include "engine/event/event_bus.h"
#include "game/systems/event/game_events.h"
#include "game/ui/radial_menu.h"
#include "game/systems/love_bond_system.h"
#include "game/relationships/relationship_facade.h"
#include "game/stage/stage_loader.h"
#include "game/systems/stage_progress_manager.h"
#include "game/systems/wave_manager.h"
#include "game/systems/group_manager.h"
#include <set>
#include <unordered_map>
#include <cmath>

//----------------------------------------------------------------------------
void TestScene::OnEnter()
{
    Application& app = Application::Get();
    Window* window = app.GetWindow();
    screenWidth_ = static_cast<float>(window->GetWidth());
    screenHeight_ = static_cast<float>(window->GetHeight());

    // マップサイズ定数（3エリア分）
    constexpr float kAreaHeight = 1080.0f;
    constexpr int kTotalAreas = 3;
    constexpr float kMapWidth = 1920.0f;
    constexpr float kMapHeight = kAreaHeight * kTotalAreas;  // 3240

    // カメラ作成（Wave 1エリアの中央 = 最下部）
    // Wave 1 = 最下部: Y = (kTotalAreas - 1) * kAreaHeight + kAreaHeight/2 = 2700
    float initialCameraY = (kTotalAreas - 1) * kAreaHeight + kAreaHeight * 0.5f;
    cameraObj_ = std::make_unique<GameObject>("MainCamera");
    cameraObj_->AddComponent<Transform>(Vector2(kMapWidth * 0.5f, initialCameraY));
    camera_ = cameraObj_->AddComponent<Camera2D>(screenWidth_, screenHeight_);

    // UI用白テクスチャ作成
    std::vector<uint32_t> whitePixels(32 * 32, 0xFFFFFFFF);
    whiteTexture_ = TextureManager::Get().Create2D(
        32, 32,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        D3D11_BIND_SHADER_RESOURCE,
        whitePixels.data(),
        32 * sizeof(uint32_t)
    );

    // 縁描画用テクスチャをロード
    ropeNormalTexture_ = TextureManager::Get().LoadTexture2D("rope_normal.png");
    ropeFriendsTexture_ = TextureManager::Get().LoadTexture2D("rope_friends.png");
    ropeLoveTexture_ = TextureManager::Get().LoadTexture2D("rope_love.png");
    LOG_INFO("[TestScene] Rope textures loaded: normal=" + std::to_string(ropeNormalTexture_ != nullptr) +
             ", friends=" + std::to_string(ropeFriendsTexture_ != nullptr) +
             ", love=" + std::to_string(ropeLoveTexture_ != nullptr));

    // ステージ背景初期化（3エリア分）
    stageBackground_.Initialize("stage1", kMapWidth, kMapHeight);

    // CSVからステージデータ読み込み
    stageData_ = StageLoader::LoadFromCSV("stages:/stage1");
    if (!stageData_.IsValid()) {
        LOG_ERROR("[TestScene] Failed to load stage data from CSV!");
        return;
    }

    // 回数制限を設定
    BindSystem::Get().SetMaxBindCount(stageData_.maxBindCount);
    BindSystem::Get().ResetBindCount();
    CutSystem::Get().SetMaxCutCount(stageData_.maxCutCount);
    CutSystem::Get().ResetCutCount();
    LOG_INFO("[TestScene] Bind limit: " +
             (stageData_.maxBindCount < 0 ? "unlimited" : std::to_string(stageData_.maxBindCount)));
    LOG_INFO("[TestScene] Cut limit: " +
             (stageData_.maxCutCount < 0 ? "unlimited" : std::to_string(stageData_.maxCutCount)));

    // プレイヤー作成
    player_ = std::make_unique<Player>();
    player_->Initialize(Vector2(stageData_.playerX, stageData_.playerY));
    player_->SetMaxHp(stageData_.playerHp);
    player_->SetMaxFe(stageData_.playerFe);
    player_->SetMoveSpeed(stageData_.playerSpeed);

    // WaveManagerを初期化
    WaveManager::Get().SetAreaHeight(kAreaHeight);
    WaveManager::Get().Initialize(stageData_.waves);
    WaveManager::Get().SetGroupSpawner([this](const GroupData& gd) -> Group* {
        return SpawnGroup(gd);
    });
    WaveManager::Get().SetOnWaveCleared([](int wave) {
        LOG_INFO("[Wave] Wave " + std::to_string(wave) + " cleared!");
    });
    WaveManager::Get().SetOnAllWavesCleared([this]() {
        LOG_INFO("[Wave] All waves cleared! Stage complete!");
        // GameStateManagerが勝利判定を行う
    });

    // 最初のウェーブをスポーン
    WaveManager::Get().SpawnCurrentWave();

    // 持ち越しグループを味方として復元
    const std::vector<CarryOverGroupData>& carryOverGroups = StageProgressManager::Get().GetCarryOverGroups();
    for (const CarryOverGroupData& data : carryOverGroups) {
        std::unique_ptr<Group> group = std::make_unique<Group>(data.id);
        group->SetBaseThreat(data.threat);
        group->SetDetectionRange(300.0f);  // デフォルト値
        group->SetFaction(GroupFaction::Ally);  // 味方として設定

        // プレイヤー付近に配置
        Vector2 groupCenter(stageData_.playerX + 100.0f, stageData_.playerY + 100.0f);

        // 個体を復元
        for (int i = 0; i < data.aliveCount; ++i) {
            std::unique_ptr<Knight> knight = std::make_unique<Knight>(data.id + "_K" + std::to_string(i));
            knight->Initialize(groupCenter);
            knight->SetColor(Color(0.3f, 0.8f, 1.0f, 1.0f));  // 味方色（シアン）
            knight->SetMaxHp(data.aliveCount > 0 ? data.totalHp / data.aliveCount : data.totalHp);
            group->AddIndividual(std::move(knight));
        }

        group->Initialize(groupCenter);

        std::unique_ptr<GroupAI> ai = std::make_unique<GroupAI>(group.get());
        ai->SetPlayer(player_.get());
        ai->SetCamera(camera_);
        group->SetAI(ai.get());
        groupAIs_.push_back(std::move(ai));

        // GroupManagerに登録（所有権移譲）
        Group* ptr = GroupManager::Get().AddGroup(std::move(group));
        CombatSystem::Get().RegisterGroup(ptr);
        // 味方なのでGameStateManagerには登録しない

        LOG_INFO("[TestScene] Restored ally group: " + data.id);
    }

    // システム初期化
    RelationshipContext::Get().Initialize();
    RelationshipFacade::Get().Initialize();
    RelationshipFacade::Get().SetPlayer(player_.get());
    CombatMediator::Get().Initialize();
    CombatSystem::Get().SetPlayer(player_.get());
    CombatMediator::Get().SetPlayer(player_.get());
    GameStateManager::Get().SetPlayer(player_.get());
    GameStateManager::Get().Initialize();
    FESystem::Get().SetPlayer(player_.get());

    // 持ち越しグループとプレイヤーの縁を復元
    for (const CarryOverGroupData& data : carryOverGroups) {
        // 復元したグループを探す
        for (const auto& group : GroupManager::Get().GetAllGroups()) {
            if (group->GetId() == data.id && group->IsAlly()) {
                // プレイヤーとの縁を作成
                BondableEntity playerEntity = player_.get();
                BondableEntity groupEntity = group.get();
                Bond* bond = BondManager::Get().CreateBond(playerEntity, groupEntity, BondType::Basic);
                if (bond) {
                    RelationshipFacade::Get().Bind(playerEntity, groupEntity, BondType::Basic);
                    LOG_INFO("[TestScene] Restored bond: Player <-> " + group->GetId());
                }
                break;
            }
        }
    }

    // グループIDからGroupポインタを取得するマップ作成
    std::unordered_map<std::string, Group*> groupMap;
    for (const auto& group : GroupManager::Get().GetAllGroups()) {
        groupMap[group->GetId()] = group.get();
    }

    // 縁を作成
    LOG_INFO("[TestScene] Creating bonds from CSV...");
    for (const BondData& bd : stageData_.bonds) {
        Group* fromGroup = groupMap[bd.fromId];
        Group* toGroup = groupMap[bd.toId];

        if (fromGroup && toGroup) {
            BondType bondType = BondType::Basic;
            if (bd.type == "Friends") {
                bondType = BondType::Friends;
            } else if (bd.type == "Love") {
                bondType = BondType::Love;
            }
            Bond* bond = BondManager::Get().CreateBond(fromGroup, toGroup, bondType);
            if (bond) {
                // RelationshipFacadeにも同期
                RelationshipFacade::Get().Bind(fromGroup, toGroup, bondType);
            } else {
                LOG_WARN("[TestScene] Failed to create bond: " + bd.fromId + " <-> " + bd.toId +
                         " (type=" + bd.type + ")");
            }
        }
    }
    LOG_INFO("[TestScene] Bonds created: " + std::to_string(BondManager::Get().GetAllBonds().size()));

    // AI状態変更コールバック設定
    for (const auto& group : GroupManager::Get().GetAllGroups()) {
        GroupAI* ai = group->GetAI();
        if (!ai) continue;
        Group* groupPtr = group.get();
        ai->SetOnStateChanged([groupPtr](AIState newState) {
            const char* stateNames[] = { "Wander", "Seek", "Flee" };
            LOG_INFO("[AI] " + groupPtr->GetId() + " -> " + stateNames[static_cast<int>(newState)]);
        });
    }

    // コールバック設定
    GameStateManager::Get().SetOnVictory([this]() {
        LOG_INFO("[TestScene] VICTORY!");

        // 味方グループを持ち越しデータとして保存
        StageProgressManager::Get().ClearCarryOver();
        for (const auto& group : GroupManager::Get().GetAllGroups()) {
            if (!group || group->IsDefeated()) continue;
            if (!group->IsAlly()) continue;  // 味方のみ

            const std::vector<Individual*>& aliveMembers = group->GetAliveIndividuals();
            CarryOverGroupData data;
            data.id = group->GetId();
            data.aliveCount = static_cast<int>(aliveMembers.size());
            // 生存個体のHP合計
            float totalHp = 0.0f;
            for (Individual* ind : aliveMembers) {
                totalHp += ind->GetHp();
            }
            data.totalHp = totalHp;
            data.threat = group->GetThreat();

            StageProgressManager::Get().AddCarryOverGroup(data);
            LOG_INFO("[TestScene] Saved ally group for carry-over: " + data.id +
                     " (" + std::to_string(data.aliveCount) + " members)");
        }

        // ステージを進める
        StageProgressManager::Get().AdvanceToNextStage();
    });
    GameStateManager::Get().SetOnDefeat([]() {
        LOG_INFO("[TestScene] DEFEAT!");
    });

    BindSystem::Get().SetOnBondCreated([](const BondableEntity& a, const BondableEntity& b) {
        LOG_INFO("[TestScene] Bond created: " + BondableHelper::GetId(a) + " <-> " + BondableHelper::GetId(b));
        // Note: RelationshipFacadeはBindSystem内で自動同期される
    });

    CutSystem::Get().SetOnBondCut([](const BondableEntity& a, const BondableEntity& b) {
        LOG_INFO("[TestScene] Bond cut: " + BondableHelper::GetId(a) + " <-> " + BondableHelper::GetId(b));
        // Note: RelationshipFacadeはCutSystem内で自動同期される
    });

    // ラジアルメニュー初期化
    RadialMenu::Get().Initialize();

    LOG_INFO("[TestScene] A-RAS! Prototype started");
    LOG_INFO("  WASD: Move player");
    LOG_INFO("  Right Click: Bind mode (create bonds)");
    LOG_INFO("  Left Click: Cut mode (cut bonds)");
    LOG_INFO("  ESC: Cancel mode");

    // テスト: 起動時に矢を1本発射
    std::vector<Group*> enemies = GroupManager::Get().GetEnemyGroups();
    if (!enemies.empty()) {
        Individual* shooter = enemies[0]->GetRandomAliveIndividual();
        if (shooter && player_) {
            ArrowManager::Get().ShootAtPlayer(shooter, player_.get(), shooter->GetPosition(), 5.0f);
            LOG_INFO("[TestScene] TEST: Shot arrow at player!");
        }
    }

    // EventBus購読を設定
    SetupEventSubscriptions();

    // 初期化完了フラグを設定
    systemsInitialized_ = true;
}

//----------------------------------------------------------------------------
void TestScene::OnExit()
{
    // ========================================================================
    // Phase 1: シングルトンのポインタをクリア（ダングリングポインタ防止）
    // ========================================================================
    CombatSystem::Get().SetPlayer(nullptr);
    GameStateManager::Get().SetPlayer(nullptr);
    FESystem::Get().SetPlayer(nullptr);
    CombatMediator::Get().SetPlayer(nullptr);
    LoveBondSystem::Get().SetPlayer(nullptr);
    RelationshipFacade::Get().SetPlayer(nullptr);

    // GroupAIのポインタをクリア
    for (std::unique_ptr<GroupAI>& ai : groupAIs_) {
        if (ai) {
            ai->SetPlayer(nullptr);
            ai->SetCamera(nullptr);
        }
    }

    // ========================================================================
    // Phase 2: システムシャットダウン（イベント発行が必要な処理）
    // ========================================================================
    if (systemsInitialized_) {
        CombatMediator::Get().Shutdown();
        RelationshipContext::Get().Shutdown();
        RelationshipFacade::Get().Shutdown();
        systemsInitialized_ = false;
    }

    // ========================================================================
    // Phase 3: システムキャッシュクリア
    // ========================================================================
    ArrowManager::Get().Clear();
    CombatSystem::Get().ClearGroups();
    WaveManager::Get().Reset();  // ウェーブ状態リセット
    BondManager::Get().Clear();
    StaggerSystem::Get().Clear();
    InsulationSystem::Get().Clear();
    FactionManager::Get().ClearEntities();
    LoveBondSystem::Get().Clear();  // キャッシュクリア（ダングリングポインタ防止）
    BindSystem::Get().Disable();
    CutSystem::Get().Disable();
    TimeManager::Get().Resume();

    // ========================================================================
    // Phase 4: エンティティ削除
    // ========================================================================
    groupAIs_.clear();
    GroupManager::Get().Clear();

    if (player_) {
        player_->Shutdown();
        player_.reset();
    }

    stageBackground_.Shutdown();
    cameraObj_.reset();
    whiteTexture_.reset();

    // ========================================================================
    // Phase 5: EventBusクリア（最後に実行）
    // ========================================================================
    eventSubscriptions_.clear();
    EventBus::Get().Clear();
}

//----------------------------------------------------------------------------
void TestScene::Update()
{
    float rawDt = Application::Get().GetDeltaTime();
    float dt = TimeManager::Get().GetScaledDeltaTime(rawDt);
    time_ += dt;

    // FPS計測
    frameCount_++;
    fpsTimer_ += rawDt;
    if (fpsTimer_ >= 1.0f) {
        currentFps_ = static_cast<float>(frameCount_) / fpsTimer_;
        frameCount_ = 0;
        fpsTimer_ = 0.0f;
    }

    // 入力処理（時間停止中も受け付ける）
    HandleInput(rawDt);

    // ゲーム状態チェック
    GameStateManager::Get().Update();

    // ゲーム終了時はリザルトシーンへ遷移
    if (!GameStateManager::Get().IsPlaying()) {
        resultTransitionTimer_ += rawDt;
        if (resultTransitionTimer_ >= kResultTransitionDelay) {
            SceneManager::Get().Load<Result_Scene>();
        }
        return;
    }

    // トランジション処理
    WaveManager& waveManager = WaveManager::Get();
    if (waveManager.IsTransitioning()) {
        waveManager.UpdateTransition(dt);

        // カメラをスムーズにスクロール（イージング）
        if (Transform* camTr = cameraObj_->GetComponent<Transform>()) {
            float t = waveManager.GetTransitionProgress();
            // イーズアウト（減速）
            float easedT = 1.0f - (1.0f - t) * (1.0f - t);

            float startY = waveManager.GetStartCameraY();
            float targetY = waveManager.GetTargetCameraY();
            float newY = startY + (targetY - startY) * easedT;
            camTr->SetPosition(camTr->GetPosition().x, newY);

            float deltaY = (targetY - startY) * easedT;

            // 初回のみ開始位置を記録
            if (t < 0.01f) {
                // プレイヤーの開始位置
                if (player_ && player_->GetTransform()) {
                    transitionPlayerStartY_ = player_->GetTransform()->GetPosition().y;
                }
                // 味方グループの開始位置
                transitionAllyStartY_.clear();
                for (const auto& group : GroupManager::Get().GetAllGroups()) {
                    if (group && group->IsAlly() && !group->IsDefeated()) {
                        transitionAllyStartY_[group.get()] = group->GetPosition().y;
                    }
                }
            }

            // プレイヤーを移動
            if (player_ && player_->GetTransform()) {
                Transform* playerTr = player_->GetTransform();
                playerTr->SetPosition(playerTr->GetPosition().x, transitionPlayerStartY_ + deltaY);
            }

            // 味方グループも一緒に移動
            for (const auto& group : GroupManager::Get().GetAllGroups()) {
                if (group && group->IsAlly() && !group->IsDefeated()) {
                    auto it = transitionAllyStartY_.find(group.get());
                    if (it != transitionAllyStartY_.end()) {
                        float allyStartY = it->second;
                        Vector2 currentPos = group->GetPosition();
                        group->SetPosition(Vector2(currentPos.x, allyStartY + deltaY));
                    }
                }
            }
        }
        return;  // トランジション中は他の更新をスキップ
    }

    // マップサイズ定数
    constexpr float kMapWidth = 1920.0f;
    constexpr float kAreaHeight = 1080.0f;

    // プレイヤー更新（時間停止中も動ける）
    if (player_ && camera_) {
        player_->Update(rawDt, *camera_);

        // プレイヤーを現在エリア内にクランプ
        if (Transform* playerTr = player_->GetTransform()) {
            Vector2 playerPos = playerTr->GetPosition();

            // 現在ウェーブのエリア範囲
            float cameraY = waveManager.GetCurrentWaveCameraY();
            float areaMinY = cameraY - kAreaHeight * 0.5f;
            float areaMaxY = cameraY + kAreaHeight * 0.5f;

            float clampedPX = (std::max)(0.0f, (std::min)(playerPos.x, kMapWidth));
            float clampedPY = (std::max)(areaMinY, (std::min)(playerPos.y, areaMaxY));
            playerTr->SetPosition(clampedPX, clampedPY);
        }

        // カメラは各ウェーブのエリア中央に固定
    }

    // AI更新（時間停止中は動かない）
    if (!TimeManager::Get().IsFrozen()) {
        for (std::unique_ptr<GroupAI>& ai : groupAIs_) {
            ai->Update(dt);
        }

        // 定期ステータスログ
        statusLogTimer_ += dt;
        if (statusLogTimer_ >= statusLogInterval_) {
            statusLogTimer_ = 0.0f;
            LogAIStatus();
        }
    }

    // グループ更新
    for (const auto& group : GroupManager::Get().GetAllGroups()) {
        group->Update(dt);
    }

    // 戦闘システム更新（時間停止中は動かない）
    if (!TimeManager::Get().IsFrozen()) {
        CombatSystem::Get().Update(dt);
    }

    // 硬直システム更新
    StaggerSystem::Get().Update(dt);

    // 矢の更新（時間停止中も飛び続ける）
    ArrowManager::Get().Update(rawDt);

    // 衝突判定
    CollisionManager::Get().Update(rawDt);

    // ウェーブマネージャー更新
    WaveManager::Get().Update();
}

//----------------------------------------------------------------------------
void TestScene::HandleInput(float /*dt*/)
{
    auto& input = InputManager::Get();

    Keyboard& kb = input.GetKeyboard();
    Mouse& mouse = input.GetMouse();
    Vector2 mouseScreenPos(static_cast<float>(mouse.GetX()), static_cast<float>(mouse.GetY()));

    RadialMenu& radialMenu = RadialMenu::Get();

    //------------------------------------------------------------------------
    // 右クリック: 結モード
    //------------------------------------------------------------------------
    if (mouse.IsButtonDown(MouseButton::Right)) {
        // 切モード中なら無効化
        if (CutSystem::Get().IsEnabled()) {
            CutSystem::Get().Disable();
        }

        // ラジアルメニューが開いていて何も選択していない場合はキャンセル
        if (radialMenu.IsOpen() && radialMenu.GetHoveredIndex() < 0) {
            radialMenu.Close();
            BindSystem::Get().Disable();
            TimeManager::Get().Resume();
            LOG_INFO("[TestScene] Radial menu cancelled (Right Click)");
        }
        else if (!radialMenu.IsOpen()) {
            // 結モードを有効化
            if (!BindSystem::Get().IsEnabled()) {
                BindSystem::Get().Enable();
                TimeManager::Get().Freeze();
                LOG_INFO("[TestScene] Bind mode ON (Right Click)");
            }

            // ラジアルメニューをカメラ中央に開く（ワールド座標で）
            if (camera_) {
                radialMenuCenter_ = camera_->GetPosition();
                radialMenu.Open(radialMenuCenter_);
            }
        }
    }

    //------------------------------------------------------------------------
    // 左クリック: 縁タイプ確定 or グループ選択 or 切モード
    //------------------------------------------------------------------------
    if (mouse.IsButtonDown(MouseButton::Left)) {
        // ラジアルメニューが開いていて何か選択している場合は縁タイプ確定
        if (radialMenu.IsOpen() && radialMenu.GetHoveredIndex() >= 0) {
            selectedBondType_ = radialMenu.Confirm();
            BindSystem::Get().SetPendingBondType(selectedBondType_);
            LOG_INFO("[TestScene] Bond type selected: " +
                     std::string(selectedBondType_ == BondType::Basic ? "Basic" :
                                 selectedBondType_ == BondType::Friends ? "Friends" : "Love"));
            // ラジアルメニューは閉じるが、結モードは継続（グループ選択待ち）
        }
        // 結モードでラジアルメニューが閉じている場合（グループ選択）
        else if (BindSystem::Get().IsEnabled() && !radialMenu.IsOpen()) {
            Group* touchingGroup = GetGroupTouchingPlayer();
            if (touchingGroup && player_) {
                // プレイヤーとグループを結ぶ
                BindPlayerToGroup(touchingGroup);
                LOG_INFO("[TestScene] Bound player to " + touchingGroup->GetId());
                // モード終了
                BindSystem::Get().Disable();
                TimeManager::Get().Resume();
            }
        }
        // 切モード中に何もない場所をクリックしたらキャンセル
        else if (CutSystem::Get().IsEnabled()) {
            Group* groupUnderCursor = GetGroupUnderCursor();
            if (!groupUnderCursor) {
                CutSystem::Get().Disable();
                TimeManager::Get().Resume();
                LOG_INFO("[TestScene] Cut mode cancelled (Left Click)");
            }
        }
        // 通常時は切モード
        else if (!BindSystem::Get().IsEnabled()) {
            CutSystem::Get().Enable();
            TimeManager::Get().Freeze();
            LOG_INFO("[TestScene] Cut mode ON (Left Click)");
        }
    }

    //------------------------------------------------------------------------
    // ラジアルメニュー更新（カメラ中央に追従）
    //------------------------------------------------------------------------
    if (radialMenu.IsOpen() && camera_) {
        // 毎フレームカメラ中央を再計算（ワールド座標）
        radialMenuCenter_ = camera_->GetPosition();
        radialMenu.SetCenter(radialMenuCenter_);

        // マウス位置もワールド座標に変換
        Vector2 mouseWorldPos = camera_->ScreenToWorld(mouseScreenPos);
        radialMenu.Update(mouseWorldPos);
    }

    //------------------------------------------------------------------------
    // ESCキー: モードキャンセル
    //------------------------------------------------------------------------
    if (kb.IsKeyDown(Key::Escape)) {
        if (radialMenu.IsOpen()) {
            radialMenu.Close();
        }
        if (BindSystem::Get().IsEnabled()) {
            BindSystem::Get().Disable();
            TimeManager::Get().Resume();
            LOG_INFO("[TestScene] Bind mode cancelled");
        }
        if (CutSystem::Get().IsEnabled()) {
            CutSystem::Get().Disable();
            TimeManager::Get().Resume();
            LOG_INFO("[TestScene] Cut mode cancelled");
        }
    }

    //------------------------------------------------------------------------
    // F1キー: デバッグ描画ON/OFF
    //------------------------------------------------------------------------
    if (kb.IsKeyDown(Key::F1)) {
        showDebugDraw_ = !showDebugDraw_;
        LOG_INFO("[TestScene] Debug draw: " + std::string(showDebugDraw_ ? "ON" : "OFF"));
    }

    //------------------------------------------------------------------------
    // 切モード中: プレイヤーが縁を通過したら切断
    //------------------------------------------------------------------------
    if (CutSystem::Get().IsEnabled() && player_ != nullptr && player_->GetCollider() != nullptr) {
        Collider2D* playerCollider = player_->GetCollider();
        Bond* bondToCut = nullptr;

        const std::vector<std::unique_ptr<Bond>>& bonds = BondManager::Get().GetAllBonds();
        for (const std::unique_ptr<Bond>& bond : bonds) {
            Vector2 posA = BondableHelper::GetPosition(bond->GetEntityA());
            Vector2 posB = BondableHelper::GetPosition(bond->GetEntityB());

            std::vector<Collider2D*> hits;
            CollisionManager::Get().QueryLineSegment(posA, posB, hits, CollisionLayer::Player);

            for (Collider2D* hitCollider : hits) {
                if (hitCollider == playerCollider) {
                    bondToCut = bond.get();
                    break;
                }
            }
            if (bondToCut != nullptr) break;
        }

        if (bondToCut != nullptr) {
            if (CutSystem::Get().CutBond(bondToCut)) {
                LOG_INFO("[TestScene] Bond cut!");
            }
        }
    }
}

//----------------------------------------------------------------------------
void TestScene::BindPlayerToGroup(Group* group)
{
    if (!player_ || !group) return;

    // プレイヤーとグループの間に縁を作成
    BondableEntity playerEntity = player_.get();
    BondableEntity groupEntity = group;

    // FEコスト確認
    FESystem& fe = FESystem::Get();
    if (!fe.CanConsume(BindSystem::Get().GetBindCost())) {
        LOG_INFO("[TestScene] Not enough FE to create bond!");
        return;
    }

    // 既に縁があるか確認
    if (BondManager::Get().GetBond(playerEntity, groupEntity) != nullptr) {
        LOG_INFO("[TestScene] Bond already exists between Player and " + group->GetId());
        return;
    }

    // 縁作成を試みる（成功したらFE消費）
    Bond* bond = BondManager::Get().CreateBond(playerEntity, groupEntity, selectedBondType_);

    if (bond) {
        // 縁作成成功時のみFEを消費
        fe.Consume(BindSystem::Get().GetBindCost());
        // RelationshipFacadeにも同期
        RelationshipFacade::Get().Bind(playerEntity, groupEntity, selectedBondType_);

        // グループを味方に変換
        if (group->IsEnemy()) {
            group->SetFaction(GroupFaction::Ally);
            LOG_INFO("[TestScene] Group " + group->GetId() + " became ally");

            // EventBus通知
            EventBus::Get().Publish(GroupBecameAllyEvent{ group });
        }

        LOG_INFO("[TestScene] Player-Group bond created: " + group->GetId() +
                 " (Type: " + std::to_string(static_cast<int>(selectedBondType_)) + ")");
    } else {
        LOG_WARN("[TestScene] Failed to create bond with " + group->GetId());
    }
}

//----------------------------------------------------------------------------
Group* TestScene::SpawnGroup(const GroupData& gd)
{
    // 陣営ごとの色
    static const Color factionColors[6] = {
        Color(1.0f, 0.3f, 0.3f, 1.0f),  // Faction1: 赤
        Color(1.0f, 0.6f, 0.2f, 1.0f),  // Faction2: オレンジ
        Color(1.0f, 1.0f, 0.3f, 1.0f),  // Faction3: 黄
        Color(0.3f, 1.0f, 0.3f, 1.0f),  // Faction4: 緑
        Color(0.3f, 0.5f, 1.0f, 1.0f),  // Faction5: 青
        Color(0.7f, 0.3f, 1.0f, 1.0f),  // Faction6: 紫
    };

    // グループIDからFaction番号を取得
    auto getFactionIndex = [](const std::string& id) -> int {
        size_t pos = id.find("Faction");
        if (pos != std::string::npos && pos + 7 < id.size()) {
            char c = id[pos + 7];
            if (c >= '1' && c <= '6') {
                return c - '1';
            }
        }
        return 0;
    };

    std::unique_ptr<Group> group = std::make_unique<Group>(gd.id);
    group->SetBaseThreat(gd.threat);
    group->SetDetectionRange(gd.detectionRange);

    Vector2 groupCenter(gd.x, gd.y);
    int factionIdx = getFactionIndex(gd.id);
    Color factionColor = factionColors[factionIdx];

    // 個体追加
    for (int i = 0; i < gd.count; ++i) {
        if (gd.species == "Elf") {
            std::unique_ptr<Elf> elf = std::make_unique<Elf>(gd.id + "_E" + std::to_string(i));
            elf->Initialize(groupCenter);
            elf->SetMaxHp(gd.hp);
            elf->SetAttackDamage(gd.attack);
            elf->SetMoveSpeed(gd.speed);
            group->AddIndividual(std::move(elf));
        } else if (gd.species == "Knight") {
            std::unique_ptr<Knight> knight = std::make_unique<Knight>(gd.id + "_K" + std::to_string(i));
            knight->Initialize(groupCenter);
            knight->SetColor(factionColor);
            knight->SetMaxHp(gd.hp);
            knight->SetAttackDamage(gd.attack);
            knight->SetMoveSpeed(gd.speed);
            group->AddIndividual(std::move(knight));
        }
    }

    group->Initialize(groupCenter);

    std::unique_ptr<GroupAI> ai = std::make_unique<GroupAI>(group.get());
    ai->SetPlayer(player_.get());
    ai->SetCamera(camera_);
    ai->SetDetectionRange(gd.detectionRange);
    group->SetAI(ai.get());
    groupAIs_.push_back(std::move(ai));

    // GroupManagerに登録（所有権移譲）
    Group* groupPtr = GroupManager::Get().AddGroup(std::move(group));
    GroupManager::Get().AssignToWave(groupPtr, gd.wave);
    CombatSystem::Get().RegisterGroup(groupPtr);
    // GameStateManagerは直接GroupManagerから敵グループを取得する

    LOG_INFO("[TestScene] Spawned group: " + gd.id + " (Wave " + std::to_string(gd.wave) + ")");
    return groupPtr;
}

//----------------------------------------------------------------------------
Group* TestScene::GetGroupUnderCursor() const
{
    if (!camera_) return nullptr;

    Mouse& mouse = InputManager::Get().GetMouse();
    Vector2 mouseWorld = camera_->ScreenToWorld(
        Vector2(static_cast<float>(mouse.GetX()), static_cast<float>(mouse.GetY()))
    );

    // CollisionManagerでマウス位置のコライダーを検索
    std::vector<Collider2D*> hits;
    CollisionManager::Get().QueryPoint(mouseWorld, hits, CollisionLayer::Individual);

    for (Collider2D* hitCollider : hits) {
        for (const auto& group : GroupManager::Get().GetAllGroups()) {
            if (group->IsDefeated()) continue;

            for (Individual* individual : group->GetAliveIndividuals()) {
                if (individual->GetCollider() == hitCollider) {
                    return group.get();
                }
            }
        }
    }

    return nullptr;
}

//----------------------------------------------------------------------------
Group* TestScene::GetGroupTouchingPlayer() const
{
    if (!player_ || !player_->GetCollider()) return nullptr;

    Collider2D* playerCollider = player_->GetCollider();
    AABB playerAABB = playerCollider->GetAABB();

    // プレイヤーと接触しているグループを探す
    std::set<Group*> touchingGroups;

    for (const auto& group : GroupManager::Get().GetAllGroups()) {
        if (group->IsDefeated()) continue;

        for (Individual* individual : group->GetAliveIndividuals()) {
            Collider2D* indivCollider = individual->GetCollider();
            if (!indivCollider) continue;

            AABB indivAABB = indivCollider->GetAABB();

            // AABB同士の衝突判定
            if (playerAABB.Intersects(indivAABB)) {
                touchingGroups.insert(group.get());
                break;  // このグループは触れている、次のグループへ
            }
        }
    }

    // 1つのグループだけに触れている場合のみ返す
    if (touchingGroups.size() == 1) {
        return *touchingGroups.begin();
    }

    return nullptr;
}

//----------------------------------------------------------------------------
const char* TestScene::GetModeText() const
{
    if (BindSystem::Get().IsEnabled()) {
        return "BIND MODE";
    }
    if (CutSystem::Get().IsEnabled()) {
        return "CUT MODE";
    }
    return "NORMAL";
}

//----------------------------------------------------------------------------
const char* TestScene::GetStateText() const
{
    switch (GameStateManager::Get().GetState()) {
    case GameState::Playing:
        return "PLAYING";
    case GameState::Victory:
        return "VICTORY!";
    case GameState::Defeat:
        return "DEFEAT";
    default:
        return "UNKNOWN";
    }
}

//----------------------------------------------------------------------------
void TestScene::Render()
{
    GraphicsContext& ctx = GraphicsContext::Get();
    Renderer& renderer = Renderer::Get();

    Texture* backBuffer = renderer.GetBackBuffer();
    Texture* depthBuffer = renderer.GetDepthBuffer();
    if (!backBuffer) return;

    ctx.SetRenderTarget(backBuffer, depthBuffer);
    ctx.SetViewport(0, 0,
        static_cast<float>(backBuffer->Width()),
        static_cast<float>(backBuffer->Height()));

    // 背景色（#4C8447 = ground simpleに合わせた緑）
    float clearColor[4] = { 0.30f, 0.52f, 0.28f, 1.0f };
    ctx.ClearRenderTarget(backBuffer, clearColor);
    ctx.ClearDepthStencil(depthBuffer, 1.0f, 0);

    SpriteBatch& spriteBatch = SpriteBatch::Get();
    spriteBatch.SetCamera(*camera_);
    spriteBatch.Begin();

    // ステージ背景描画
    stageBackground_.Render(spriteBatch, *camera_);

    // 縁の描画
    DrawBonds();

    // 敵グループ描画
    for (const auto& group : GroupManager::Get().GetAllGroups()) {
        group->Render(spriteBatch);
    }

#ifdef _DEBUG
    if (showDebugDraw_) {
        DrawIndividualColliders();
        if (player_ && player_->GetCollider()) {
            AABB playerAABB = player_->GetCollider()->GetAABB();
            Color playerColliderColor(0.0f, 1.0f, 0.0f, 0.8f);
            DEBUG_RECT(playerAABB.GetCenter(), playerAABB.GetSize(), playerColliderColor, 2.0f);
        }
    }
#endif

    // 矢の描画
    ArrowManager::Get().Render(spriteBatch);

    // プレイヤー描画
    if (player_) {
        player_->Render(spriteBatch);
    }

    // UI描画
    DrawUI();

    // モード別オーバーレイ描画（画面全体に半透明）
#ifdef _DEBUG
    if (BindSystem::Get().IsEnabled() || CutSystem::Get().IsEnabled()) {
        DebugDraw& debug = DebugDraw::Get();
        Vector2 cameraPos = camera_->GetPosition();
        float viewWidth = camera_->GetViewportWidth();
        float viewHeight = camera_->GetViewportHeight();
        Vector2 overlaySize(viewWidth, viewHeight);

        if (BindSystem::Get().IsEnabled()) {
            // 結モード: 黄の半透明
            debug.DrawRectFilled(cameraPos, overlaySize, Color(1.0f, 1.0f, 0.0f, 0.2f));
        } else if (CutSystem::Get().IsEnabled()) {
            // 切モード: 赤の半透明
            debug.DrawRectFilled(cameraPos, overlaySize, Color(1.0f, 0.0f, 0.0f, 0.15f));
        }
    }
#endif

    // ラジアルメニュー描画
    RadialMenu::Get().Render(spriteBatch);

    spriteBatch.End();

#ifdef _DEBUG
    if (showDebugDraw_) {
        CircleRenderer::Get().Begin(*camera_);
        DrawDetectionRanges();
        CircleRenderer::Get().End();
    }
#endif
}

//----------------------------------------------------------------------------
void TestScene::DrawBondTexture(const Vector2& posA, const Vector2& posB, Texture* texture, const Color& color)
{
    if (!texture) return;

    SpriteBatch& spriteBatch = SpriteBatch::Get();

    // テクスチャサイズ取得
    float texWidth = static_cast<float>(texture->Width());
    float texHeight = static_cast<float>(texture->Height());

    // 2点間の距離と角度を計算
    Vector2 delta = posB - posA;
    float distance = delta.Length();
    if (distance < 1.0f) return;  // 距離が短すぎる場合はスキップ

    float angle = std::atan2(delta.y, delta.x);
    Vector2 direction = delta / distance;  // 正規化

    // 原点をテクスチャ中央に設定
    Vector2 origin(texWidth * 0.5f, texHeight * 0.5f);

    // セグメント間隔（テクスチャ幅の40%で大きく重ねる）
    float spacing = texWidth * 0.4f;

    // 始点から終点まで等間隔でテクスチャを配置
    float currentDist = 0.0f;
    int order = 0;
    while (currentDist < distance) {
        Vector2 segmentPos = posA + direction * currentDist;

        spriteBatch.Draw(
            texture,
            segmentPos,
            color,
            angle,
            origin,
            Vector2::One,
            false, false,
            -50,  // sortingLayer: 縁は背景より手前、キャラより奥
            order++
        );

        currentDist += spacing;
    }
}

//----------------------------------------------------------------------------
void TestScene::DrawBonds()
{
    const std::vector<std::unique_ptr<Bond>>& bonds = BondManager::Get().GetAllBonds();

    static int logCounter = 0;
    if (++logCounter % 60 == 1) {  // 1秒に1回ログ
        LOG_DEBUG("[DrawBonds] Bond count: " + std::to_string(bonds.size()));
    }

    for (const std::unique_ptr<Bond>& bond : bonds) {
        // 全滅したグループの縁は描画しない
        if (Group* groupA = BondableHelper::AsGroup(bond->GetEntityA())) {
            if (groupA->IsDefeated()) continue;
        }
        if (Group* groupB = BondableHelper::AsGroup(bond->GetEntityB())) {
            if (groupB->IsDefeated()) continue;
        }

        Vector2 posA = BondableHelper::GetPosition(bond->GetEntityA());
        Vector2 posB = BondableHelper::GetPosition(bond->GetEntityB());

        // 縁タイプ別のテクスチャと色を選択
        Texture* ropeTexture = nullptr;
        Color bondColor = Colors::White;

        switch (bond->GetType()) {
        case BondType::Basic:
            ropeTexture = ropeNormalTexture_.get();
            bondColor = Color(1.0f, 1.0f, 1.0f, 0.9f);
            break;
        case BondType::Friends:
            ropeTexture = ropeFriendsTexture_.get();
            bondColor = Color(1.0f, 1.0f, 1.0f, 0.9f);
            break;
        case BondType::Love:
            ropeTexture = ropeLoveTexture_.get();
            bondColor = Color(1.0f, 1.0f, 1.0f, 0.9f);
            break;
        default:
            ropeTexture = ropeNormalTexture_.get();
            bondColor = Color(0.8f, 0.8f, 0.2f, 0.9f);
            break;
        }

        // テクスチャで縁を描画
        DrawBondTexture(posA, posB, ropeTexture, bondColor);
    }

    // 個体コライダーの描画（デバッグ用）
    Color colliderColor(0.0f, 1.0f, 1.0f, 0.5f);  // シアン
    for (const auto& group : GroupManager::Get().GetAllGroups()) {
        if (group->IsDefeated()) continue;

        for (Individual* individual : group->GetAliveIndividuals()) {
            Collider2D* collider = individual->GetCollider();
            if (!collider) continue;

            AABB aabb = collider->GetAABB();
            Vector2 center((aabb.minX + aabb.maxX) * 0.5f, (aabb.minY + aabb.maxY) * 0.5f);
            Vector2 size(aabb.maxX - aabb.minX, aabb.maxY - aabb.minY);
            DEBUG_RECT(center, size, colliderColor);
        }
    }

    // プレイヤーコライダーの描画
    if (player_ && player_->GetCollider()) {
        AABB aabb = player_->GetCollider()->GetAABB();
        Vector2 center((aabb.minX + aabb.maxX) * 0.5f, (aabb.minY + aabb.maxY) * 0.5f);
        Vector2 size(aabb.maxX - aabb.minX, aabb.maxY - aabb.minY);
        Color playerColliderColor(1.0f, 1.0f, 0.0f, 0.5f);  // 黄色
        DEBUG_RECT(center, size, playerColliderColor);
    }
}

#ifdef _DEBUG
//----------------------------------------------------------------------------
void TestScene::DrawDetectionRanges()
{
    Color detectionRangeColor(1.0f, 0.5f, 0.0f, 0.3f);  // オレンジ（半透明）

    for (const auto& group : GroupManager::Get().GetAllGroups()) {
        if (group->IsDefeated()) continue;

        Vector2 groupPos = group->GetPosition();
        float detectionRange = group->GetDetectionRange();
        CircleRenderer::Get().DrawFilled(groupPos, detectionRange, detectionRangeColor);
    }
}

//----------------------------------------------------------------------------
void TestScene::DrawIndividualColliders()
{
    Color colliderColor(0.0f, 1.0f, 1.0f, 0.8f);  // シアン

    for (const auto& group : GroupManager::Get().GetAllGroups()) {
        if (group->IsDefeated()) continue;

        for (Individual* individual : group->GetAliveIndividuals()) {
            Collider2D* collider = individual->GetCollider();
            if (!collider) continue;

            AABB aabb = collider->GetAABB();
            DEBUG_RECT(aabb.GetCenter(), aabb.GetSize(), colliderColor, 2.0f);
        }
    }
}
#endif

//----------------------------------------------------------------------------
void TestScene::DrawUI()
{
    // FPS表示（左上）- 色でFPSを表示（緑=60+, 黄=30+, 赤=30未満）
    {
        Vector2 fpsPos = camera_->ScreenToWorld(Vector2(30.0f, 20.0f));
        Color fpsColor;
        if (currentFps_ >= 55.0f) {
            fpsColor = Color(0.0f, 1.0f, 0.0f, 0.9f);  // 緑
        } else if (currentFps_ >= 30.0f) {
            fpsColor = Color(1.0f, 1.0f, 0.0f, 0.9f);  // 黄
        } else {
            fpsColor = Color(1.0f, 0.0f, 0.0f, 0.9f);  // 赤
        }
        // FPSに応じたバー幅（最大60FPSで100px）
        float barWidth = (std::min)(currentFps_ / 60.0f, 1.0f) * 100.0f;
        DEBUG_RECT_FILL(fpsPos + Vector2(barWidth * 0.5f, 0.0f), Vector2(barWidth, 15.0f), fpsColor);
    }

    // HP/FEバー表示（画面右上）
    if (player_) {
        float hpRatio = player_->GetHpRatio();
        float feRatio = player_->GetFeRatio();

        // HPバー背景
        Color hpBgColor(0.3f, 0.0f, 0.0f, 0.8f);
        Color hpColor(0.0f, 1.0f, 0.0f, 0.9f);
        Color feBgColor(0.0f, 0.0f, 0.3f, 0.8f);
        Color feColor(0.3f, 0.6f, 1.0f, 0.9f);

        Vector2 hpPos = camera_->ScreenToWorld(Vector2(screenWidth_ - 220.0f, 20.0f));
        Vector2 fePos = camera_->ScreenToWorld(Vector2(screenWidth_ - 220.0f, 45.0f));

        // HPバー（背景 + 現在値）
        DEBUG_RECT_FILL(hpPos + Vector2(100.0f, 0.0f), Vector2(200.0f, 20.0f), hpBgColor);
        if (hpRatio > 0.0f) {
            DEBUG_RECT_FILL(hpPos + Vector2(hpRatio * 100.0f, 0.0f), Vector2(hpRatio * 200.0f, 20.0f), hpColor);
        }

        // FEバー（背景 + 現在値）
        DEBUG_RECT_FILL(fePos + Vector2(100.0f, 0.0f), Vector2(200.0f, 20.0f), feBgColor);
        if (feRatio > 0.0f) {
            DEBUG_RECT_FILL(fePos + Vector2(feRatio * 100.0f, 0.0f), Vector2(feRatio * 200.0f, 20.0f), feColor);
        }
    }

    // 結ぶ/切る残り回数表示（HP/FEバーの下）
    {
        int remainingBinds = BindSystem::Get().GetRemainingBinds();
        int maxBinds = BindSystem::Get().GetMaxBindCount();
        int remainingCuts = CutSystem::Get().GetRemainingCuts();
        int maxCuts = CutSystem::Get().GetMaxCutCount();

        Vector2 bindPos = camera_->ScreenToWorld(Vector2(screenWidth_ - 220.0f, 75.0f));
        Vector2 cutPos = camera_->ScreenToWorld(Vector2(screenWidth_ - 220.0f, 100.0f));

        Color bindBgColor(0.3f, 0.3f, 0.0f, 0.8f);
        Color bindColor(1.0f, 1.0f, 0.3f, 0.9f);  // 黄色
        Color cutBgColor(0.3f, 0.0f, 0.0f, 0.8f);
        Color cutColor(1.0f, 0.3f, 0.3f, 0.9f);   // 赤

        // 結ぶ残り回数バー
        DEBUG_RECT_FILL(bindPos + Vector2(100.0f, 0.0f), Vector2(200.0f, 15.0f), bindBgColor);
        if (maxBinds > 0) {
            float bindRatio = static_cast<float>(remainingBinds) / static_cast<float>(maxBinds);
            if (bindRatio > 0.0f) {
                DEBUG_RECT_FILL(bindPos + Vector2(bindRatio * 100.0f, 0.0f),
                               Vector2(bindRatio * 200.0f, 15.0f), bindColor);
            }
        } else {
            // 無制限
            DEBUG_RECT_FILL(bindPos + Vector2(100.0f, 0.0f), Vector2(200.0f, 15.0f), bindColor);
        }

        // 切る残り回数バー
        DEBUG_RECT_FILL(cutPos + Vector2(100.0f, 0.0f), Vector2(200.0f, 15.0f), cutBgColor);
        if (maxCuts > 0) {
            float cutRatio = static_cast<float>(remainingCuts) / static_cast<float>(maxCuts);
            if (cutRatio > 0.0f) {
                DEBUG_RECT_FILL(cutPos + Vector2(cutRatio * 100.0f, 0.0f),
                               Vector2(cutRatio * 200.0f, 15.0f), cutColor);
            }
        } else {
            // 無制限
            DEBUG_RECT_FILL(cutPos + Vector2(100.0f, 0.0f), Vector2(200.0f, 15.0f), cutColor);
        }
    }

    // ウェーブ表示（画面上部中央）
    {
        int currentWave = WaveManager::Get().GetCurrentWave();
        int totalWaves = WaveManager::Get().GetTotalWaves();

        // ウェーブ進捗バー
        Vector2 wavePos = camera_->ScreenToWorld(Vector2(screenWidth_ * 0.5f - 100.0f, 30.0f));
        Color waveBgColor(0.2f, 0.2f, 0.4f, 0.8f);
        Color waveColor(0.5f, 0.5f, 1.0f, 0.9f);

        DEBUG_RECT_FILL(wavePos + Vector2(100.0f, 0.0f), Vector2(200.0f, 20.0f), waveBgColor);
        if (totalWaves > 0) {
            float waveRatio = static_cast<float>(currentWave) / static_cast<float>(totalWaves);
            if (waveRatio > 0.0f) {
                DEBUG_RECT_FILL(wavePos + Vector2(waveRatio * 100.0f, 0.0f),
                               Vector2(waveRatio * 200.0f, 20.0f), waveColor);
            }
        }

        // ウェーブ区切りマーカー
        Color markerColor(1.0f, 1.0f, 1.0f, 0.6f);
        if (totalWaves > 1) {
            for (int w = 1; w < totalWaves; ++w) {
                float markerX = static_cast<float>(w) / static_cast<float>(totalWaves) * 200.0f;
                DEBUG_RECT_FILL(wavePos + Vector2(markerX, 0.0f), Vector2(2.0f, 20.0f), markerColor);
            }
        }
    }

    // 勝敗表示
    if (!GameStateManager::Get().IsPlaying()) {
        Color resultColor = GameStateManager::Get().IsVictory()
            ? Color(0.0f, 1.0f, 0.0f, 0.9f)
            : Color(1.0f, 0.0f, 0.0f, 0.9f);

        Vector2 center = camera_->ScreenToWorld(Vector2(screenWidth_ * 0.5f, screenHeight_ * 0.5f));
        DEBUG_RECT_FILL(center, Vector2(200.0f, 200.0f), resultColor);
    }
}

//----------------------------------------------------------------------------
void TestScene::LogAIStatus()
{
    const char* stateNames[] = { "Wander", "Seek", "Flee" };

    LOG_INFO("=== AI Status ===");
    for (const auto& group : GroupManager::Get().GetAllGroups()) {
        if (!group || group->IsDefeated()) continue;

        GroupAI* ai = group->GetAI();
        if (!ai) continue;

        std::string status = group->GetId();
        status += " [" + std::string(stateNames[static_cast<int>(ai->GetState())]) + "]";
        status += " HP:" + std::to_string(static_cast<int>(group->GetHpRatio() * 100)) + "%";
        status += " Threat:" + std::to_string(static_cast<int>(group->GetThreat()));

        // ターゲット情報
        AITarget target = ai->GetTarget();
        if (std::holds_alternative<Group*>(target)) {
            Group* tgt = std::get<Group*>(target);
            if (tgt) status += " -> " + tgt->GetId();
        } else if (std::holds_alternative<Player*>(target)) {
            status += " -> Player";
        }

        // 硬直中か
        if (StaggerSystem::Get().IsStaggered(group.get())) {
            status += " [STAGGER]";
        }

        LOG_INFO("  " + status);
    }

    // 縁の数
    LOG_INFO("  Bonds: " + std::to_string(BondManager::Get().GetAllBonds().size()));
}

//----------------------------------------------------------------------------
void TestScene::SetupEventSubscriptions()
{
    // 結モード変更
    eventSubscriptions_.push_back(
        EventBus::Get().Subscribe<BindModeChangedEvent>([](const BindModeChangedEvent& e) {
            LOG_INFO("[EventBus] BindMode: " + std::string(e.enabled ? "ON" : "OFF"));
        })
    );

    // 切モード変更
    eventSubscriptions_.push_back(
        EventBus::Get().Subscribe<CutModeChangedEvent>([](const CutModeChangedEvent& e) {
            LOG_INFO("[EventBus] CutMode: " + std::string(e.enabled ? "ON" : "OFF"));
        })
    );

    // エンティティマーク
    eventSubscriptions_.push_back(
        EventBus::Get().Subscribe<EntityMarkedEvent>([](const EntityMarkedEvent& e) {
            LOG_INFO("[EventBus] Entity marked: " + BondableHelper::GetId(e.entity));
        })
    );

    // 縁作成
    eventSubscriptions_.push_back(
        EventBus::Get().Subscribe<BondCreatedEvent>([](const BondCreatedEvent& e) {
            LOG_INFO("[EventBus] Bond created: " +
                     BondableHelper::GetId(e.entityA) + " <-> " +
                     BondableHelper::GetId(e.entityB));
            // Note: RelationshipFacadeはBindSystem/CutSystem内で自動同期される
        })
    );

    // 縁削除
    eventSubscriptions_.push_back(
        EventBus::Get().Subscribe<BondRemovedEvent>([](const BondRemovedEvent& e) {
            LOG_INFO("[EventBus] Bond removed: " +
                     BondableHelper::GetId(e.entityA) + " <-> " +
                     BondableHelper::GetId(e.entityB));
            // Note: RelationshipFacadeはBindSystem/CutSystem内で自動同期される
        })
    );

    // グループ全滅
    eventSubscriptions_.push_back(
        EventBus::Get().Subscribe<GroupDefeatedEvent>([](const GroupDefeatedEvent& e) {
            LOG_INFO("[EventBus] Group defeated: " + e.group->GetId());

            // 全滅したグループの縁を全て削除
            BondableEntity entity = e.group;
            BondManager::Get().RemoveAllBondsFor(entity);
            RelationshipFacade::Get().CutAll(entity);
        })
    );

    LOG_INFO("[TestScene] EventBus subscriptions registered");
}
