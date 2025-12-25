//----------------------------------------------------------------------------
//! @file   application.inl
//! @brief  アプリケーションテンプレート実装
//----------------------------------------------------------------------------
#pragma once

#include "renderer.h"
#include "common/logging/logging.h"

//----------------------------------------------------------------------------
template<typename TGame>
void Application::Run(TGame& game)
{
    if (running_) {
        LOG_WARN("[Application] 既に実行中です");
        return;
    }
    running_ = true;
    shouldQuit_ = false;

    MainLoop(game);

    running_ = false;
}

//----------------------------------------------------------------------------
template<typename TGame>
void Application::MainLoop(TGame& game)
{
    Renderer& renderer = Renderer::Get();
    while (!shouldQuit_) {
        // メッセージ処理
        if (!window_->ProcessMessages()) {
            break;  // WM_QUIT
        }

        if (window_->ShouldClose()) {
            break;
        }

        // 最小化中はスリープ
        if (window_->IsMinimized()) {
            Sleep(10);
            continue;
        }

        // 時間更新
        Timer::Update(desc_.maxDeltaTime);

        // 入力処理
        ProcessInput();

        // 通常更新
        game.Update();

        // 描画
        game.Render();

        // Present
        renderer.Present();

        // フレーム末処理
        game.EndFrame();
    }
}
