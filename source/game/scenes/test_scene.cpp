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

//----------------------------------------------------------------------------
void TestScene::OnEnter()
{
    Application& app = Application::Get();
    Window* window = app.GetWindow();
    screenWidth_ = static_cast<float>(window->GetWidth());
    screenHeight_ = static_cast<float>(window->GetHeight());

    // カメラ作成
    cameraObj_ = std::make_unique<GameObject>("MainCamera");
    cameraObj_->AddComponent<Transform2D>(Vector2(screenWidth_ * 0.5f, screenHeight_ * 0.5f));
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

    // 背景テクスチャをロード
    backgroundTexture_ = TextureManager::Get().LoadTexture2D("background.png");

    // 背景作成
    background_ = std::make_unique<GameObject>("Background");
    bgTransform_ = background_->AddComponent<Transform2D>();
    bgTransform_->SetPosition(Vector2(screenWidth_ * 0.5f, screenHeight_ * 0.5f));
    bgSprite_ = background_->AddComponent<SpriteRenderer>();
    bgSprite_->SetTexture(backgroundTexture_.get());
    bgSprite_->SetSortingLayer(-100);
    if (backgroundTexture_) {
        float texW = static_cast<float>(backgroundTexture_->Width());
        float texH = static_cast<float>(backgroundTexture_->Height());
        bgTransform_->SetPivot(Vector2(texW * 0.5f, texH * 0.5f));
        float scaleX = screenWidth_ / texW;
        float scaleY = screenHeight_ / texH;
        float scale = (scaleX > scaleY) ? scaleX : scaleY;
        bgTransform_->SetScale(Vector2(scale, scale));
    }

    // プレイヤー作成（画面中央）
    player_ = std::make_unique<Player>();
    player_->Initialize(Vector2(screenWidth_ * 0.5f, screenHeight_ * 0.5f));

    // 敵グループ1: Elfグループ（左上）
    {
        std::unique_ptr<Group> group = std::make_unique<Group>("ElfGroup1");
        group->SetBaseThreat(80.0f);
        group->SetDetectionRange(300.0f);  // デバッグ用に小さめ

        // Elf個体を3体追加
        for (int i = 0; i < 3; ++i) {
            std::unique_ptr<Elf> elf = std::make_unique<Elf>("Elf1_" + std::to_string(i));
            elf->Initialize(Vector2(180.0f + i * 40.0f, 130.0f + i * 20.0f));
            group->AddIndividual(std::move(elf));
        }

        // 個体追加後にFormation初期化
        group->Initialize(Vector2(200.0f, 150.0f));

        // AI作成
        std::unique_ptr<GroupAI> ai = std::make_unique<GroupAI>(group.get());
        ai->SetPlayer(player_.get());
        ai->SetCamera(camera_);
        ai->SetDetectionRange(300.0f);  // デバッグ用に小さめ
        group->SetAI(ai.get());  // GroupにAI参照をセット
        groupAIs_.push_back(std::move(ai));

        // システムに登録
        CombatSystem::Get().RegisterGroup(group.get());
        GameStateManager::Get().RegisterEnemyGroup(group.get());

        enemyGroups_.push_back(std::move(group));
    }

    // 敵グループ2: Knightグループ（右上）
    {
        std::unique_ptr<Group> group = std::make_unique<Group>("KnightGroup1");
        group->SetBaseThreat(120.0f);
        group->SetDetectionRange(300.0f);  // デバッグ用に小さめ

        // Knight個体を2体追加
        for (int i = 0; i < 2; ++i) {
            std::unique_ptr<Knight> knight = std::make_unique<Knight>("Knight1_" + std::to_string(i));
            knight->Initialize(Vector2(screenWidth_ - 220.0f + i * 40.0f, 130.0f + i * 30.0f));
            knight->SetColor(Color(1.0f, 0.3f, 0.3f, 1.0f)); // 赤
            group->AddIndividual(std::move(knight));
        }

        // 個体追加後にFormation初期化
        group->Initialize(Vector2(screenWidth_ - 200.0f, 150.0f));

        std::unique_ptr<GroupAI> ai = std::make_unique<GroupAI>(group.get());
        ai->SetPlayer(player_.get());
        ai->SetCamera(camera_);
        ai->SetDetectionRange(300.0f);  // デバッグ用に小さめ
        group->SetAI(ai.get());  // GroupにAI参照をセット
        groupAIs_.push_back(std::move(ai));

        CombatSystem::Get().RegisterGroup(group.get());
        GameStateManager::Get().RegisterEnemyGroup(group.get());

        enemyGroups_.push_back(std::move(group));
    }

    // 敵グループ3: Elfグループ（左下）
    {
        std::unique_ptr<Group> group = std::make_unique<Group>("ElfGroup2");
        group->SetBaseThreat(60.0f);
        group->SetDetectionRange(300.0f);  // デバッグ用に小さめ

        for (int i = 0; i < 4; ++i) {
            std::unique_ptr<Elf> elf = std::make_unique<Elf>("Elf2_" + std::to_string(i));
            elf->Initialize(Vector2(160.0f + i * 30.0f, screenHeight_ - 170.0f + (i % 2) * 40.0f));
            group->AddIndividual(std::move(elf));
        }

        // 個体追加後にFormation初期化
        group->Initialize(Vector2(200.0f, screenHeight_ - 150.0f));

        std::unique_ptr<GroupAI> ai = std::make_unique<GroupAI>(group.get());
        ai->SetPlayer(player_.get());
        ai->SetCamera(camera_);
        ai->SetDetectionRange(300.0f);  // デバッグ用に小さめ
        group->SetAI(ai.get());  // GroupにAI参照をセット
        groupAIs_.push_back(std::move(ai));

        CombatSystem::Get().RegisterGroup(group.get());
        GameStateManager::Get().RegisterEnemyGroup(group.get());

        enemyGroups_.push_back(std::move(group));
    }

    // 敵グループ4: Knightグループ（右下）
    {
        std::unique_ptr<Group> group = std::make_unique<Group>("KnightGroup2");
        group->SetBaseThreat(100.0f);
        group->SetDetectionRange(300.0f);  // デバッグ用に小さめ

        for (int i = 0; i < 3; ++i) {
            std::unique_ptr<Knight> knight = std::make_unique<Knight>("Knight2_" + std::to_string(i));
            knight->Initialize(Vector2(screenWidth_ - 240.0f + i * 40.0f, screenHeight_ - 160.0f + i * 20.0f));
            knight->SetColor(Color(0.3f, 0.5f, 1.0f, 1.0f)); // 青
            group->AddIndividual(std::move(knight));
        }

        // 個体追加後にFormation初期化
        group->Initialize(Vector2(screenWidth_ - 200.0f, screenHeight_ - 150.0f));

        std::unique_ptr<GroupAI> ai = std::make_unique<GroupAI>(group.get());
        ai->SetPlayer(player_.get());
        ai->SetCamera(camera_);
        ai->SetDetectionRange(300.0f);  // デバッグ用に小さめ
        group->SetAI(ai.get());  // GroupにAI参照をセット
        groupAIs_.push_back(std::move(ai));

        CombatSystem::Get().RegisterGroup(group.get());
        GameStateManager::Get().RegisterEnemyGroup(group.get());

        enemyGroups_.push_back(std::move(group));
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

    // 初期縁を作成（騎士同士のみ接続、エルフは未接続）
    // KnightGroup1(1) <-> KnightGroup2(3)
    LOG_INFO("[TestScene] Creating initial bonds...");
    {
        // 騎士同士
        BondableEntity knight1 = enemyGroups_[1].get();  // KnightGroup1
        BondableEntity knight2 = enemyGroups_[3].get();  // KnightGroup2
        Bond* knightBond = BondManager::Get().CreateBond(knight1, knight2, BondType::Basic);
        if (knightBond) {
            LOG_INFO("  Bond: KnightGroup1 <-> KnightGroup2 (Knights)");
        }
    }
    LOG_INFO("[TestScene] Initial bonds created: " + std::to_string(BondManager::Get().GetAllBonds().size()));

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
    });

    CutSystem::Get().SetOnBondCut([](const BondableEntity& a, const BondableEntity& b) {
        LOG_INFO("[TestScene] Bond cut: " + BondableHelper::GetId(a) + " <-> " + BondableHelper::GetId(b));
        FactionManager::Get().RebuildFactions();
    });

    LOG_INFO("[TestScene] A-RAS! Prototype started");
    LOG_INFO("  WASD: Move player");
    LOG_INFO("  B: Toggle Bind mode (create bonds)");
    LOG_INFO("  C: Toggle Cut mode (cut bonds)");
    LOG_INFO("  Left Click: Select entity / Confirm");
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

    background_.reset();
    cameraObj_.reset();
    whiteTexture_.reset();
    backgroundTexture_.reset();
}

//----------------------------------------------------------------------------
void TestScene::Update()
{
    float rawDt = Application::Get().GetDeltaTime();
    float dt = TimeManager::Get().GetScaledDeltaTime(rawDt);
    time_ += dt;

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
        camera_->Follow(player_->GetTransform()->GetPosition(), 0.1f);
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

    // Bキー: 結モード切り替え
    if (kb.IsKeyDown(Key::B)) {
        if (CutSystem::Get().IsEnabled()) {
            CutSystem::Get().Disable();
        }
        BindSystem::Get().Toggle();

        if (BindSystem::Get().IsEnabled()) {
            TimeManager::Get().Freeze();
            LOG_INFO("[TestScene] Bind mode ON");
        } else {
            TimeManager::Get().Resume();
            LOG_INFO("[TestScene] Bind mode OFF");
        }
    }

    // Cキー: 切モード切り替え
    if (kb.IsKeyDown(Key::C)) {
        if (BindSystem::Get().IsEnabled()) {
            BindSystem::Get().Disable();
        }
        CutSystem::Get().Toggle();

        if (CutSystem::Get().IsEnabled()) {
            TimeManager::Get().Freeze();
            LOG_INFO("[TestScene] Cut mode ON - Time frozen");
        } else {
            TimeManager::Get().Resume();
            LOG_INFO("[TestScene] Cut mode OFF - Time resumed");
        }
    }

    // ESCキー: モードキャンセル
    if (kb.IsKeyDown(Key::Escape)) {
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

    // 結モード中: CollisionManagerのコールバックで自動処理
    // （Individual::SetupCollider()で設定済み）

    // 切モード中: プレイヤーが縁を通過したら切断
    // ※縁はコライダーを持たないためQueryLineSegmentを使用
    if (CutSystem::Get().IsEnabled() && player_ != nullptr && player_->GetCollider() != nullptr) {
        Collider2D* playerCollider = player_->GetCollider();
        Bond* bondToCut = nullptr;

        // 切断対象の縁を探す（反復中に削除しない）
        const std::vector<std::unique_ptr<Bond>>& bonds = BondManager::Get().GetAllBonds();
        for (const std::unique_ptr<Bond>& bond : bonds) {
            Vector2 posA = BondableHelper::GetPosition(bond->GetEntityA());
            Vector2 posB = BondableHelper::GetPosition(bond->GetEntityB());

            // CollisionManagerで線分と交差するコライダーを検索
            std::vector<Collider2D*> hits;
            CollisionManager::Get().QueryLineSegment(posA, posB, hits, CollisionLayer::Player);

            // プレイヤーのコライダーが含まれているか確認
            for (Collider2D* hitCollider : hits) {
                if (hitCollider == playerCollider) {
                    bondToCut = bond.get();
                    break;
                }
            }
            if (bondToCut != nullptr) break;
        }

        // 反復完了後に切断（1フレームに1つまで）
        if (bondToCut != nullptr) {
            if (CutSystem::Get().CutBond(bondToCut)) {
                LOG_INFO("[TestScene] Bond cut!");
            }
        }
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

    // 背景色（モードによって変更）
    float clearColor[4] = { 0.1f, 0.1f, 0.2f, 1.0f };
    if (BindSystem::Get().IsEnabled()) {
        clearColor[0] = 0.1f; clearColor[1] = 0.2f; clearColor[2] = 0.1f; // 緑がかった色
    } else if (CutSystem::Get().IsEnabled()) {
        clearColor[0] = 0.2f; clearColor[1] = 0.1f; clearColor[2] = 0.1f; // 赤がかった色
    }
    ctx.ClearRenderTarget(backBuffer, clearColor);
    ctx.ClearDepthStencil(depthBuffer, 1.0f, 0);

    SpriteBatch& spriteBatch = SpriteBatch::Get();
    spriteBatch.SetCamera(*camera_);
    spriteBatch.Begin();

    // 背景描画
    if (bgTransform_ && bgSprite_) {
        spriteBatch.Draw(*bgSprite_, *bgTransform_);
    }

    // 縁の描画
    DrawBonds();

    // 敵グループ描画
    for (const std::unique_ptr<Group>& group : enemyGroups_) {
        group->Render(spriteBatch);
    }

#ifdef _DEBUG
    // 個体コライダー描画
    DrawIndividualColliders();

    // プレイヤーコライダー描画
    if (player_ && player_->GetCollider()) {
        AABB playerAABB = player_->GetCollider()->GetAABB();
        Color playerColliderColor(0.0f, 1.0f, 0.0f, 0.8f);  // 緑
        DEBUG_RECT(playerAABB.GetCenter(), playerAABB.GetSize(), playerColliderColor, 2.0f);
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

    spriteBatch.End();

#ifdef _DEBUG
    // 円描画（別シェーダー）
    CircleRenderer::Get().Begin(*camera_);
    DrawDetectionRanges();
    CircleRenderer::Get().End();
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

        // 縁の色（黄色）
        Color bondColor(0.8f, 0.8f, 0.2f, 0.8f);
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
    // HP/FEバー表示（画面左上）
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
