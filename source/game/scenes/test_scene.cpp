//----------------------------------------------------------------------------
//! @file   test_scene.cpp
//! @brief  テストシーン実装 - A-RAS!ゲームプロトタイプ
//----------------------------------------------------------------------------
#include "test_scene.h"
#include "dx11/graphics_context.h"
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
#include "game/systems/time_manager.h"
#include "game/systems/bind_system.h"
#include "game/systems/cut_system.h"
#include "game/systems/combat_system.h"
#include "game/systems/game_state_manager.h"
#include "game/systems/fe_system.h"
#include "game/systems/stagger_system.h"
#include "game/systems/insulation_system.h"
#include "game/systems/faction_manager.h"
#include "game/systems/event/event_bus.h"
#include "game/systems/event/game_events.h"
#include "game/ui/radial_menu.h"
#include "game/systems/love_bond_system.h"
#include <set>

//----------------------------------------------------------------------------
void TestScene::OnEnter()
{
    Application& app = Application::Get();
    Window* window = app.GetWindow();
    screenWidth_ = static_cast<float>(window->GetWidth());
    screenHeight_ = static_cast<float>(window->GetHeight());

    // カメラ作成
    cameraObj_ = std::make_unique<GameObject>("MainCamera");
    cameraObj_->AddComponent<Transform2D>(Vector2(2000.0f, 2000.0f));  // マップ中央
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

    // ステージ背景初期化
    float stageWidth = 4000.0f;
    float stageHeight = 4000.0f;
    stageBackground_.Initialize("stage1", stageWidth, stageHeight);

    // プレイヤー作成（マップ中央）
    player_ = std::make_unique<Player>();
    player_->Initialize(Vector2(2000.0f, 2000.0f));

    // 6陣営を配置（各陣営: 3グループ×4個体、縁で接続）
    // 陣営の中心位置（マップ周辺に配置、中央はプレイヤー）
    // type: 0=エルフのみ, 1=騎士のみ, 2=混合
    struct FactionPos { float x; float y; Color color; int type; };
    // グループオフセット(±250,±220)を考慮
    // 4000x4000マップ、中央(2000,2000)にプレイヤー
    // 6陣営を六角形状に配置
    FactionPos factionPositions[6] = {
        { 2000.0f, 800.0f,  Color(1.0f, 0.3f, 0.3f, 1.0f), 1 },  // 上（赤）騎士
        { 3000.0f, 1400.0f, Color(1.0f, 0.6f, 0.2f, 1.0f), 0 },  // 右上（オレンジ）エルフ
        { 3000.0f, 2600.0f, Color(1.0f, 1.0f, 0.3f, 1.0f), 2 },  // 右下（黄）混合
        { 2000.0f, 3200.0f, Color(0.3f, 1.0f, 0.3f, 1.0f), 1 },  // 下（緑）騎士
        { 1000.0f, 2600.0f, Color(0.3f, 0.5f, 1.0f, 1.0f), 0 },  // 左下（青）エルフ
        { 1000.0f, 1400.0f, Color(0.7f, 0.3f, 1.0f, 1.0f), 2 },  // 左上（紫）混合
    };

    // グループ内オフセット（三角形配置・広め）
    Vector2 groupOffsets[3] = {
        Vector2(0.0f, -250.0f),    // 上
        Vector2(-220.0f, 150.0f),  // 左下
        Vector2(220.0f, 150.0f),   // 右下
    };

    for (int f = 0; f < 6; ++f) {
        FactionPos& fpos = factionPositions[f];
        std::string factionName = "Faction" + std::to_string(f + 1);

        // 3グループ作成
        for (int g = 0; g < 3; ++g) {
            std::string groupId = factionName + "_G" + std::to_string(g + 1);
            std::unique_ptr<Group> group = std::make_unique<Group>(groupId);
            group->SetBaseThreat(80.0f + static_cast<float>(f * 5));
            group->SetDetectionRange(500.0f);

            Vector2 groupCenter(fpos.x + groupOffsets[g].x, fpos.y + groupOffsets[g].y);

            // 4個体追加
            for (int i = 0; i < 4; ++i) {
                if (fpos.type == 0) {
                    // エルフのみ
                    std::unique_ptr<Elf> elf = std::make_unique<Elf>(groupId + "_E" + std::to_string(i));
                    elf->Initialize(groupCenter);
                    group->AddIndividual(std::move(elf));
                } else if (fpos.type == 1) {
                    // 騎士のみ
                    std::unique_ptr<Knight> knight = std::make_unique<Knight>(groupId + "_K" + std::to_string(i));
                    knight->Initialize(groupCenter);
                    knight->SetColor(fpos.color);
                    group->AddIndividual(std::move(knight));
                } else {
                    // 混合（交互）
                    if (i % 2 == 0) {
                        std::unique_ptr<Elf> elf = std::make_unique<Elf>(groupId + "_E" + std::to_string(i));
                        elf->Initialize(groupCenter);
                        group->AddIndividual(std::move(elf));
                    } else {
                        std::unique_ptr<Knight> knight = std::make_unique<Knight>(groupId + "_K" + std::to_string(i));
                        knight->Initialize(groupCenter);
                        knight->SetColor(fpos.color);
                        group->AddIndividual(std::move(knight));
                    }
                }
            }

            group->Initialize(groupCenter);

            std::unique_ptr<GroupAI> ai = std::make_unique<GroupAI>(group.get());
            ai->SetPlayer(player_.get());
            ai->SetCamera(camera_);
            ai->SetDetectionRange(500.0f);
            group->SetAI(ai.get());
            groupAIs_.push_back(std::move(ai));

            CombatSystem::Get().RegisterGroup(group.get());
            GameStateManager::Get().RegisterEnemyGroup(group.get());

            enemyGroups_.push_back(std::move(group));
        }
    }

    // システム初期化
    CombatSystem::Get().SetPlayer(player_.get());
    GameStateManager::Get().SetPlayer(player_.get());
    GameStateManager::Get().Initialize();
    FESystem::Get().SetPlayer(player_.get());

    // FactionManagerにエンティティを登録
    FactionManager::Get().ClearEntities();
    FactionManager::Get().RegisterEntity(player_.get());
    for (const auto& group : enemyGroups_) {
        FactionManager::Get().RegisterEntity(group.get());
    }

    // 各陣営内のグループを縁で接続（三角形に結ぶ）
    LOG_INFO("[TestScene] Creating faction bonds...");
    for (int f = 0; f < 6; ++f) {
        int baseIdx = f * 3;  // 各陣営の最初のグループインデックス
        Group* g0 = enemyGroups_[baseIdx].get();
        Group* g1 = enemyGroups_[baseIdx + 1].get();
        Group* g2 = enemyGroups_[baseIdx + 2].get();

        // A-B-C の直線接続
        BondManager::Get().CreateBond(g0, g1, BondType::Basic);
        BondManager::Get().CreateBond(g1, g2, BondType::Basic);
    }
    LOG_INFO("[TestScene] Faction bonds created: " + std::to_string(BondManager::Get().GetAllBonds().size()));

    // Factionを再構築
    FactionManager::Get().RebuildFactions();

    // AI状態変更コールバック設定
    for (size_t i = 0; i < groupAIs_.size(); ++i) {
        Group* group = enemyGroups_[i].get();
        groupAIs_[i]->SetOnStateChanged([group](AIState newState) {
            const char* stateNames[] = { "Wander", "Seek", "Flee" };
            LOG_INFO("[AI] " + group->GetId() + " -> " + stateNames[static_cast<int>(newState)]);
        });
    }

    // コールバック設定
    GameStateManager::Get().SetOnVictory([]() {
        LOG_INFO("[TestScene] VICTORY!");
    });
    GameStateManager::Get().SetOnDefeat([]() {
        LOG_INFO("[TestScene] DEFEAT!");
    });

    BindSystem::Get().SetOnBondCreated([](const BondableEntity& a, const BondableEntity& b) {
        LOG_INFO("[TestScene] Bond created: " + BondableHelper::GetId(a) + " <-> " + BondableHelper::GetId(b));
        FactionManager::Get().RebuildFactions();
        LoveBondSystem::Get().RebuildLoveGroups();
    });

    CutSystem::Get().SetOnBondCut([](const BondableEntity& a, const BondableEntity& b) {
        LOG_INFO("[TestScene] Bond cut: " + BondableHelper::GetId(a) + " <-> " + BondableHelper::GetId(b));
        FactionManager::Get().RebuildFactions();
        LoveBondSystem::Get().RebuildLoveGroups();
    });

    // ラジアルメニュー初期化
    RadialMenu::Get().Initialize();

    LOG_INFO("[TestScene] A-RAS! Prototype started");
    LOG_INFO("  WASD: Move player");
    LOG_INFO("  Right Click: Bind mode (create bonds)");
    LOG_INFO("  Left Click: Cut mode (cut bonds)");
    LOG_INFO("  ESC: Cancel mode");

    // テスト: 起動時に矢を1本発射
    if (!enemyGroups_.empty()) {
        Individual* shooter = enemyGroups_[0]->GetRandomAliveIndividual();
        if (shooter && player_) {
            ArrowManager::Get().ShootAtPlayer(shooter, player_.get(), shooter->GetPosition(), 5.0f);
            LOG_INFO("[TestScene] TEST: Shot arrow at player!");
        }
    }

    // EventBus購読を設定
    SetupEventSubscriptions();
}

//----------------------------------------------------------------------------
void TestScene::OnExit()
{
    // EventBus購読解除
    EventBus::Get().Clear();
    eventSubscriptions_.clear();

    // システムクリア
    ArrowManager::Get().Clear();
    CombatSystem::Get().ClearGroups();
    GameStateManager::Get().ClearEnemyGroups();
    BondManager::Get().Clear();
    StaggerSystem::Get().Clear();
    InsulationSystem::Get().Clear();
    FactionManager::Get().ClearEntities();
    BindSystem::Get().Disable();
    CutSystem::Get().Disable();
    TimeManager::Get().Resume();

    groupAIs_.clear();
    enemyGroups_.clear();

    if (player_) {
        player_->Shutdown();
        player_.reset();
    }

    stageBackground_.Shutdown();
    cameraObj_.reset();
    whiteTexture_.reset();
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

    // ゲーム終了時は更新停止
    if (!GameStateManager::Get().IsPlaying()) {
        return;
    }

    // プレイヤー更新（時間停止中も動ける）
    if (player_ && camera_) {
        player_->Update(rawDt, *camera_);

        // マップサイズ定数
        constexpr float kMapWidth = 4000.0f;
        constexpr float kMapHeight = 4000.0f;

        // プレイヤーをマップ範囲内にクランプ
        if (Transform2D* playerTr = player_->GetTransform()) {
            Vector2 playerPos = playerTr->GetPosition();
            float clampedPX = (std::max)(0.0f, (std::min)(playerPos.x, kMapWidth));
            float clampedPY = (std::max)(0.0f, (std::min)(playerPos.y, kMapHeight));
            playerTr->SetPosition(clampedPX, clampedPY);
        }

        camera_->Follow(player_->GetTransform()->GetPosition(), 0.1f);

        // カメラをマップ範囲内にクランプ
        float halfViewW = camera_->GetViewportWidth() * 0.5f;
        float halfViewH = camera_->GetViewportHeight() * 0.5f;

        Vector2 camPos = camera_->GetPosition();
        float clampedX = (std::max)(halfViewW, (std::min)(camPos.x, kMapWidth - halfViewW));
        float clampedY = (std::max)(halfViewH, (std::min)(camPos.y, kMapHeight - halfViewH));
        camera_->SetPosition(clampedX, clampedY);
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
    for (std::unique_ptr<Group>& group : enemyGroups_) {
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
}

//----------------------------------------------------------------------------
void TestScene::HandleInput(float /*dt*/)
{
    InputManager* input = InputManager::GetInstance();
    if (!input) return;

    Keyboard& kb = input->GetKeyboard();
    Mouse& mouse = input->GetMouse();
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

    // FE消費して縁作成
    fe.Consume(BindSystem::Get().GetBindCost());
    Bond* bond = BondManager::Get().CreateBond(playerEntity, groupEntity, selectedBondType_);

    if (bond) {
        LOG_INFO("[TestScene] Player-Group bond created: " + group->GetId() +
                 " (Type: " + std::to_string(static_cast<int>(selectedBondType_)) + ")");
        FactionManager::Get().RebuildFactions();
        LoveBondSystem::Get().RebuildLoveGroups();
    }
}

//----------------------------------------------------------------------------
Group* TestScene::GetGroupUnderCursor() const
{
    InputManager* input = InputManager::GetInstance();
    if (!input || !camera_) return nullptr;

    Mouse& mouse = input->GetMouse();
    Vector2 mouseWorld = camera_->ScreenToWorld(
        Vector2(static_cast<float>(mouse.GetX()), static_cast<float>(mouse.GetY()))
    );

    // CollisionManagerでマウス位置のコライダーを検索
    std::vector<Collider2D*> hits;
    CollisionManager::Get().QueryPoint(mouseWorld, hits, CollisionLayer::Individual);

    for (Collider2D* hitCollider : hits) {
        for (const std::unique_ptr<Group>& group : enemyGroups_) {
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

    for (const std::unique_ptr<Group>& group : enemyGroups_) {
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
    for (const std::unique_ptr<Group>& group : enemyGroups_) {
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
void TestScene::DrawBonds()
{
    const std::vector<std::unique_ptr<Bond>>& bonds = BondManager::Get().GetAllBonds();

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

        // 縁タイプ別の色
        Color bondColor;
        switch (bond->GetType()) {
        case BondType::Basic:
            bondColor = Color(0.9f, 0.9f, 0.9f, 0.8f);  // 白
            break;
        case BondType::Friends:
            bondColor = Color(0.3f, 1.0f, 0.3f, 0.8f);  // 緑
            break;
        case BondType::Love:
            bondColor = Color(1.0f, 0.5f, 0.7f, 0.8f);  // ピンク
            break;
        default:
            bondColor = Color(0.8f, 0.8f, 0.2f, 0.8f);  // デフォルト（黄色）
            break;
        }
        DEBUG_LINE(posA, posB, bondColor, 3.0f);
    }

    // 個体コライダーの描画（デバッグ用）
    Color colliderColor(0.0f, 1.0f, 1.0f, 0.5f);  // シアン
    for (const std::unique_ptr<Group>& group : enemyGroups_) {
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

    for (const std::unique_ptr<Group>& group : enemyGroups_) {
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

    for (const std::unique_ptr<Group>& group : enemyGroups_) {
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

    // モード表示（背景色で判別できるため削除）

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
    for (size_t i = 0; i < groupAIs_.size() && i < enemyGroups_.size(); ++i) {
        GroupAI* ai = groupAIs_[i].get();
        Group* group = enemyGroups_[i].get();

        if (!group || group->IsDefeated()) continue;

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
        if (StaggerSystem::Get().IsStaggered(group)) {
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
            FactionManager::Get().RebuildFactions();
        })
    );

    // 縁削除
    eventSubscriptions_.push_back(
        EventBus::Get().Subscribe<BondRemovedEvent>([](const BondRemovedEvent& e) {
            LOG_INFO("[EventBus] Bond removed: " +
                     BondableHelper::GetId(e.entityA) + " <-> " +
                     BondableHelper::GetId(e.entityB));
            FactionManager::Get().RebuildFactions();
        })
    );

    // グループ全滅
    eventSubscriptions_.push_back(
        EventBus::Get().Subscribe<GroupDefeatedEvent>([](const GroupDefeatedEvent& e) {
            LOG_INFO("[EventBus] Group defeated: " + e.group->GetId());

            // 全滅したグループの縁を全て削除
            BondableEntity entity = e.group;
            BondManager::Get().RemoveAllBondsFor(entity);

            // 陣営再構築
            FactionManager::Get().RebuildFactions();
        })
    );

    LOG_INFO("[TestScene] EventBus subscriptions registered");
}
