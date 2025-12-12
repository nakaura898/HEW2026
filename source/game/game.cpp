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

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <filesystem>

#include "scenes/test_scene.h"
#include "dx11/graphics_context.h"
#include "engine/graphics2d/sprite_batch.h"

namespace {
    //! @brief 実行ファイルのあるディレクトリを取得
    std::wstring GetExecutableDirectory() {
        wchar_t path[MAX_PATH];
        GetModuleFileNameW(nullptr, path, MAX_PATH);
        std::filesystem::path exePath(path);
        return exePath.parent_path().wstring() + L"/";
    }
}

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
    // 実行ファイルからの相対パス: build/bin/Debug-windows-x86_64/game/ → プロジェクトルート
    std::wstring exeDir = GetExecutableDirectory();

#ifdef _DEBUG
    // コンソール+ファイルログを有効化
    // プロジェクトルート/debug/ に保存
    std::filesystem::path debugDir = std::filesystem::path(exeDir) / L"../../../../debug";
    debugDir = std::filesystem::weakly_canonical(debugDir);
    std::filesystem::create_directories(debugDir);
    std::wstring logPath = debugDir.wstring() + L"/debug_log.txt";
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

    // パスを正規化
    std::filesystem::path assetsPath = std::filesystem::path(exeDir) / L"../../../../assets";
    assetsPath = std::filesystem::weakly_canonical(assetsPath);
    std::wstring assetsRoot = assetsPath.wstring() + L"/";

    LOG_INFO("[Game] Exe directory: " + std::string(exeDir.begin(), exeDir.end()));
    LOG_INFO("[Game] Assets root: " + std::string(assetsRoot.begin(), assetsRoot.end()));

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
