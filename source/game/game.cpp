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
#include "dx11/graphics_context.h"
#include "engine/platform/renderer.h"
#include "engine/graphics2d/render_state_manager.h"
#include "engine/c_systems/sprite_batch.h"
#ifdef _DEBUG
#include "engine/debug/debug_draw.h"
#include "engine/debug/circle_renderer.h"
#endif

// シェーダーコンパイラ（グローバルインスタンス）
static std::unique_ptr<D3DShaderCompiler> g_shaderCompiler;

//----------------------------------------------------------------------------
Game::Game()
    : sceneManager_(SceneManager::Get())
{
}

//----------------------------------------------------------------------------
bool Game::Initialize()
{
    std::wstring projectRoot = FileSystemManager::GetProjectRoot();
    std::wstring assetsRoot = FileSystemManager::GetAssetsDirectory();

    // 1. InputManager初期化
    if (!InputManager::Initialize()) {
        LOG_ERROR("[Game] InputManagerの初期化に失敗");
        return false;
    }

    // 2. CollisionManager初期化（セルサイズはコライダーサイズの2倍が適切）
    CollisionManager::Get().Initialize(64);

    // 3. ファイルシステムマウント
    LOG_INFO("[Game] Project root: " + PathUtility::toNarrowString(projectRoot));
    LOG_INFO("[Game] Assets root: " + PathUtility::toNarrowString(assetsRoot));

    auto& fsManager = FileSystemManager::Get();
    fsManager.Mount("shaders", std::make_unique<HostFileSystem>(assetsRoot + L"shader/"));
    fsManager.Mount("textures", std::make_unique<HostFileSystem>(assetsRoot + L"texture/"));
    fsManager.Mount("stages", std::make_unique<HostFileSystem>(assetsRoot + L"stages/"));

    // 4. TextureManager初期化
    auto* textureFs = fsManager.GetFileSystem("textures");
    if (textureFs) {
        TextureManager::Get().Initialize(textureFs);
    }

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

    LOG_INFO("[Game] サブシステム初期化完了");

    // 8. 初期シーンを設定
    sceneManager_.Load<Title_Scene>();
    sceneManager_.ApplyPendingChange(currentScene_);

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

    // 逆順でシャットダウン
#ifdef _DEBUG
    CircleRenderer::Get().Shutdown();
    DebugDraw::Get().Shutdown();
#endif
    SpriteBatch::Get().Shutdown();
    RenderStateManager::Get().Shutdown();
    ShaderManager::Get().Shutdown();
    Renderer::Get().Shutdown();  // TextureManagerより先に解放（colorBuffer_/depthBuffer_がTexturePtr）
    TextureManager::Get().Shutdown();
    FileSystemManager::Get().UnmountAll();
    CollisionManager::Get().Shutdown();
    InputManager::Uninit();
    g_shaderCompiler.reset();

    LOG_INFO("[Game] シャットダウン完了");
}

//----------------------------------------------------------------------------
void Game::Update()
{
    if (currentScene_) {
        currentScene_->Update();
    }
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
    sceneManager_.ApplyPendingChange(currentScene_);
}
