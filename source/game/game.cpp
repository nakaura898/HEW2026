//----------------------------------------------------------------------------
//! @file   game.cpp
//! @brief  ゲームクラス実装
//----------------------------------------------------------------------------
#include "game.h"
#include "engine/platform/application.h"
#include "engine/fs/file_system_manager.h"
#include "engine/fs/host_file_system.h"
#include "engine/texture/texture_manager.h"
#include "engine/shader/shader_manager.h"
#include "dx11/compile/shader_compiler.h"
#include "engine/input/input_manager.h"
#include "common/logging/logging.h"
#include "engine/scene/scene_manager.h"
#include "engine/collision/collision_manager.h"

#include "scenes/test_scene.h"

// シェーダーコンパイラ（グローバルインスタンス）
static std::unique_ptr<D3DShaderCompiler> g_shaderCompiler;

// コンソールログ出力（デバッグ用）
#ifdef _DEBUG
static MultiLogOutput g_consoleLog;
#endif

//----------------------------------------------------------------------------
Game::Game()
    : sceneManager_(SceneManager::Get())
{
}

//----------------------------------------------------------------------------
bool Game::Initialize()
{
#ifdef _DEBUG
    // コンソールログを有効化
    LogSystem::setOutput(&g_consoleLog);
#endif

    // 1. InputManager初期化
    if (!InputManager::Initialize()) {
        LOG_ERROR("[Game] InputManagerの初期化に失敗");
        return false;
    }

    // 2. CollisionManager初期化
    CollisionManager::Get().Initialize(256);

    // 3. ファイルシステムマウント
    auto& fsManager = FileSystemManager::Get();
    fsManager.Mount("shaders", std::make_unique<HostFileSystem>(L"assets/shader/"));
    fsManager.Mount("textures", std::make_unique<HostFileSystem>(L"assets/texture/"));

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

    LOG_INFO("[Game] サブシステム初期化完了");

    // 5. 初期シーンを設定
    sceneManager_.Load<TestScene>();
    sceneManager_.ApplyPendingChange(currentScene_);

    return true;
}

//----------------------------------------------------------------------------
void Game::Shutdown() noexcept
{
    if (currentScene_) {
        currentScene_->OnExit();
        currentScene_.reset();
    }

    // 逆順でシャットダウン
    ShaderManager::Get().Shutdown();
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
