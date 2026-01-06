//----------------------------------------------------------------------------
//! @file   game.cpp
//! @brief  ゲームクラス実装
//----------------------------------------------------------------------------
#include "game.h"
#include "engine/platform/application.h"
#include "engine/fs/file_system_manager.h"
#include "engine/fs/host_file_system.h"
#include "engine/fs/path_utility.h"
#include "engine/texture/texture_manager.h"
#include "engine/shader/shader_manager.h"
#include "dx11/compile/shader_compiler.h"
#include "engine/input/input_manager.h"
#include "common/logging/logging.h"
#include "engine/scene/scene_manager.h"
#include "engine/c_systems/collision_manager.h"

#include "scenes/title_scene.h"
#include "scenes/test_scene.h"
#include "systems/system_manager.h"
#include "dx11/graphics_context.h"
#include "engine/platform/renderer.h"
#include "engine/graphics2d/render_state_manager.h"
#include "engine/c_systems/sprite_batch.h"
#include "engine/c_systems/mesh_batch.h"
#include "engine/debug/debug_draw.h"
#include "engine/debug/circle_renderer.h"
#include "engine/core/job_system.h"
#include "engine/core/service_locator.h"
#include "engine/mesh/mesh_manager.h"
#include "engine/material/material_manager.h"
#include "engine/lighting/lighting_manager.h"

// シェーダーコンパイラ（グローバルインスタンス）
static std::unique_ptr<D3DShaderCompiler> g_shaderCompiler;

//----------------------------------------------------------------------------
Game::Game() = default;

//----------------------------------------------------------------------------
bool Game::Initialize()
{
    // 0. エンジンシングルトン生成
    // Note: TextureManager, Renderer は Application層で管理
    JobSystem::Create();  // 最初に初期化（他システムが使用する可能性あり）
    Services::Provide(&JobSystem::Get());  // ServiceLocatorに登録
    InputManager::Create();
    FileSystemManager::Create();
    ShaderManager::Create();
    RenderStateManager::Create();
    SpriteBatch::Create();
    MeshBatch::Create();
    CollisionManager::Create();
    MeshManager::Create();
    MaterialManager::Create();
    LightingManager::Create();
    SceneManager::Create();
#ifdef _DEBUG
    DebugDraw::Create();
    CircleRenderer::Create();
#endif

    std::wstring projectRoot = FileSystemManager::GetProjectRoot();
    std::wstring assetsRoot = FileSystemManager::GetAssetsDirectory();

    // 1. ゲームシステム一括生成（シングルトン初期化）
    SystemManager::CreateAll();

    // 2. CollisionManager初期化（セルサイズはコライダーサイズの2倍が適切）
    CollisionManager::Get().Initialize(64);

    // 3. ファイルシステムマウント
    LOG_INFO("[Game] Project root: " + PathUtility::toNarrowString(projectRoot));
    LOG_INFO("[Game] Assets root: " + PathUtility::toNarrowString(assetsRoot));

    auto& fsManager = FileSystemManager::Get();
    fsManager.Mount("shaders", std::make_unique<HostFileSystem>(assetsRoot + L"shader/"));
    fsManager.Mount("textures", std::make_unique<HostFileSystem>(assetsRoot + L"texture/"));
    fsManager.Mount("stages", std::make_unique<HostFileSystem>(assetsRoot + L"stages/"));
    fsManager.Mount("models", std::make_unique<HostFileSystem>(assetsRoot + L"models/"));

    // 4. TextureManager初期化（Application層でCreate済み）
    auto* textureFs = fsManager.GetFileSystem("textures");
    TextureManager::Get().Initialize(textureFs);

    // 5. ShaderManager初期化
    auto* shaderFs = fsManager.GetFileSystem("shaders");
    if (shaderFs) {
        g_shaderCompiler = std::make_unique<D3DShaderCompiler>();
        ShaderManager::Get().Initialize(shaderFs, g_shaderCompiler.get());
    }

    // 6. RenderStateManager初期化
    if (!RenderStateManager::Get().Initialize()) {
        LOG_ERROR("[Game] RenderStateManagerの初期化に失敗");
        return false;
    }

    // 7. SpriteBatch初期化
    if (!SpriteBatch::Get().Initialize()) {
        LOG_ERROR("[Game] SpriteBatchの初期化に失敗");
        return false;
    }

    // 7.5. MeshBatch初期化
    if (!MeshBatch::Get().Initialize()) {
        LOG_ERROR("[Game] MeshBatchの初期化に失敗");
        return false;
    }

    // 8. MeshManager初期化
    auto* modelsFs = fsManager.GetFileSystem("models");
    MeshManager::Get().Initialize(modelsFs);

    // 9. MaterialManager初期化
    MaterialManager::Get().Initialize();

    // 10. LightingManager初期化
    LightingManager::Get().Initialize();

    LOG_INFO("[Game] サブシステム初期化完了");

    // 8. 初期シーンを設定
    SceneManager::Get().Load<Title_Scene>();
    SceneManager::Get().ApplyPendingChange(currentScene_);

    return true;
}

//----------------------------------------------------------------------------
void Game::Shutdown() noexcept
{
    // パイプラインから全リソースをアンバインド（テクスチャ解放前に必須）
    if (auto* ctx = GraphicsContext::Get().GetContext()) {
        ctx->ClearState();
        ctx->Flush();
    }

    if (currentScene_) {
        currentScene_->OnExit();
        currentScene_.reset();
    }

    // パイプラインからすべてのステートをアンバインド（リソース解放前に必須）
    auto* d3dCtx = GraphicsContext::Get().GetContext();
    if (d3dCtx) {
        d3dCtx->ClearState();
        d3dCtx->Flush();
    }

    // 逆順でシャットダウン
#ifdef _DEBUG
    CircleRenderer::Get().Shutdown();
    DebugDraw::Get().Shutdown();
#endif
    LightingManager::Get().Shutdown();
    MeshBatch::Get().Shutdown();
    SpriteBatch::Get().Shutdown();
    RenderStateManager::Get().Shutdown();
    ShaderManager::Get().Shutdown();
    MaterialManager::Get().Shutdown();  // TextureManagerより先に解放（テクスチャを参照）
    MeshManager::Get().Shutdown();
    Renderer::Get().Shutdown();  // TextureManagerより先に解放（colorBuffer_/depthBuffer_がTexturePtr）
    TextureManager::Get().Shutdown();
    FileSystemManager::Get().UnmountAll();
    CollisionManager::Get().Shutdown();
    g_shaderCompiler.reset();

    // ゲームシステム一括破棄
    SystemManager::DestroyAll();

    // エンジンシングルトン破棄（逆順）
    // Note: TextureManager, Renderer は Application層で管理
#ifdef _DEBUG
    CircleRenderer::Destroy();
    DebugDraw::Destroy();
#endif
    SceneManager::Destroy();
    LightingManager::Destroy();
    MaterialManager::Destroy();
    MeshManager::Destroy();
    CollisionManager::Destroy();
    MeshBatch::Destroy();
    SpriteBatch::Destroy();
    RenderStateManager::Destroy();
    ShaderManager::Destroy();
    FileSystemManager::Destroy();
    InputManager::Destroy();
    JobSystem::Destroy();  // 最後に破棄（他システムが使用している可能性あり）

    LOG_INFO("[Game] シャットダウン完了");
}

//----------------------------------------------------------------------------
void Game::Update()
{
    // フレーム開始（ジョブカウンターリセット）
    JobSystem::Get().BeginFrame();

    if (currentScene_) {
        currentScene_->Update();
    }

    // メインスレッドジョブを処理
    JobSystem::Get().ProcessMainThreadJobs();
}

//----------------------------------------------------------------------------
void Game::Render()
{
    if (currentScene_) {
        currentScene_->Render();
    }
}

//----------------------------------------------------------------------------
void Game::EndFrame()
{
    // フレーム内ジョブの完了を待機
    JobSystem::Get().EndFrame();

    SceneManager::Get().ApplyPendingChange(currentScene_);
}
