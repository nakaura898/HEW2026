//----------------------------------------------------------------------------
//! @file   main.cpp
//! @brief  ゲーム エントリーポイント
//----------------------------------------------------------------------------
#include "engine/platform/application.h"
#include "game.h"

#include <Windows.h>

//! WinMainエントリーポイント
int WINAPI WinMain(
    _In_ HINSTANCE hInstance,
    _In_opt_ HINSTANCE hPrevInstance,
    _In_ LPSTR lpCmdLine,
    _In_ int nCmdShow)
{
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    // アプリケーション設定
    ApplicationDesc desc;
    desc.window.title = L"HEW2026 Game";
    desc.window.width = 1920;
    desc.window.height = 1080;
    desc.enableDebugLayer = true;
    desc.vsync = VSyncMode::On;

    // エンジンシングルトン生成
    Application::Create();

    // 初期化
    if (!Application::Get().Initialize(desc)) {
        Application::Destroy();
        return -1;
    }

    // ゲーム初期化
    Game game;
    if (!game.Initialize()) {
        Application::Get().Shutdown();
        return -1;
    }

    // ゲーム実行
    Application::Get().Run(game);

    // 終了
    game.Shutdown();
    Application::Get().Shutdown();
    Application::Destroy();

    return 0;
}
