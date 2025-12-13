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

#include "scenes/test_scene.h"
#include "dx11/graphics_context.h"
#include "engine/c_systems/sprite_batch.h"

// シェーダーコンパイラ（グローバルインスタンス）
static std::unique_ptr<D3DShaderCompiler> g_shaderCompiler;

// コンソール+ファイルログ出力（デバッグ用）
#ifdef _DEBUG
static FullLogOutput g_fullLog;
#endif

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

#ifdef _DEBUG
    // コンソール+ファイルログを有効化
    std::wstring debugDir = PathUtility::normalizeW(projectRoot + L"debug");
    FileSystemManager::CreateDirectories(debugDir);
    std::wstring logPath = debugDir + L"debug_log.txt";
    g_fullLog.openFile(logPath);
    LogSystem::setOutput(&g_fullLog);
    LOG_INFO("=== ログ出力開始 ===");
#endif

    // 1. InputManager初期化
    if (!InputManager::Initialize()) {
        LOG_ERROR("[Game] InputManagerの初期化に失敗");
        return false;
    }

    // 2. CollisionManager初期化
    CollisionManager::Get().Initialize(256);

    // 3. ファイルシステムマウント
    LOG_INFO("[Game] Project root: " + PathUtility::toNarrowString(projectRoot));
    LOG_INFO("[Game] Assets root: " + PathUtility::toNarrowString(assetsRoot));

    auto& fsManager = FileSystemManager::Get();
    fsManager.Mount("shaders", std::make_unique<HostFileSystem>(assetsRoot + L"shader/"));
    fsManager.Mount("textures", std::make_unique<HostFileSystem>(assetsRoot + L"texture/"));

    // 3. TextureManager初期化
    auto* textureFs = fsManager.GetFileSystem("textures");
    if (textureFs) {
        TextureManager::Get().Initialize(textureFs);
    }

    // 4. ShaderManager初期化
    auto* shaderFs = fsManager.GetFileSystem("shaders");
    if (shaderFs) {
        g_shaderCompiler = std::make_unique<D3DShaderCompiler>();
        ShaderManager::Get().Initialize(shaderFs, g_shaderCompiler.get());
    }

    // 5. SpriteBatch初期化
    if (!SpriteBatch::Get().Initialize()) {
        LOG_ERROR("[Game] SpriteBatchの初期化に失敗");
        return false;
    }

    LOG_INFO("[Game] サブシステム初期化完了");

    // 6. 初期シーンを設定
    sceneManager_.Load<TestScene>();
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
    SpriteBatch::Get().Shutdown();
    ShaderManager::Get().Shutdown();
    TextureManager::Get().Shutdown();
    FileSystemManager::Get().UnmountAll();
    CollisionManager::Get().Shutdown();
    InputManager::Uninit();
    g_shaderCompiler.reset();

    LOG_INFO("[Game] シャットダウン完了");
}

//----------------------------------------------------------------------------
void Game::CloseLog() noexcept
{
#ifdef _DEBUG
    LOG_INFO("=== ログ出力終了 ===");
    g_fullLog.closeFile();
#endif
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
