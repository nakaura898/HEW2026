//----------------------------------------------------------------------------
//! @file   application.h
//! @brief  アプリケーション管理クラス（シングルトン）
//----------------------------------------------------------------------------
#pragma once

#include "dx11/gpu_common.h"
#include "dx11/swap_chain.h"
#include "engine/time/timer.h"
#include "window.h"
#include <memory>
#include <cstdint>

//----------------------------------------------------------------------------
//! @brief アプリケーション設定
//----------------------------------------------------------------------------
struct ApplicationDesc
{
    HINSTANCE hInstance = nullptr;        //!< アプリケーションインスタンス
    WindowDesc window;                    //!< ウィンドウ設定
    uint32_t renderWidth = 1280;          //!< レンダリング解像度幅（固定）
    uint32_t renderHeight = 720;          //!< レンダリング解像度高さ（固定）
    bool enableDebugLayer = true;         //!< D3Dデバッグレイヤー有効化
    VSyncMode vsync = VSyncMode::On;      //!< 垂直同期モード
    float maxDeltaTime = 0.25f;           //!< deltaTime上限（秒）デバッグ時異常値防止
};

//----------------------------------------------------------------------------
//! @brief アプリケーション管理クラス（シングルトン）
//!
//! Window所有、メインループ提供、時間管理を行う。
//! 描画関連はRenderer（シングルトン）が担当。
//----------------------------------------------------------------------------
class Application final : private NonCopyableNonMovable
{
public:
    //! @brief シングルトンインスタンス取得
    static Application& Get() noexcept;

    //----------------------------------------------------------
    //! @name ライフサイクル
    //----------------------------------------------------------
    //!@{

    //! @brief 初期化
    //! @param [in] desc アプリケーション設定
    //! @return 成功時true
    [[nodiscard]] bool Initialize(const ApplicationDesc& desc);

    //! @brief アプリケーション実行
    //! @tparam TGame ゲームクラス（Update(), Render()を持つ）
    //! @param [in] game ゲームインスタンス
    //!
    //! ループ内で以下を実行:
    //! 1. メッセージ処理
    //! 2. 時間更新
    //! 3. ProcessInput
    //! 4. Update
    //! 5. Render
    //! 6. Renderer::Get().Present()
    template<typename TGame>
    void Run(TGame& game);

    //! @brief 終了処理
    void Shutdown() noexcept;

    //! @brief ループ終了要求
    void Quit() noexcept;

    //!@}

    //----------------------------------------------------------
    //! @name 時間管理
    //----------------------------------------------------------
    //!@{

    //! @brief 前フレームからの経過時間（秒）
    [[nodiscard]] float GetDeltaTime() const noexcept;

    //! @brief アプリケーション開始からの経過時間（秒）
    [[nodiscard]] float GetTotalTime() const noexcept;

    //! @brief 現在のFPS
    [[nodiscard]] float GetFPS() const noexcept;

    //! @brief フレームカウント
    [[nodiscard]] uint64_t GetFrameCount() const noexcept;

    //!@}

    //----------------------------------------------------------
    //! @name サブシステムアクセス
    //----------------------------------------------------------
    //!@{

    //! @brief インスタンスハンドルを取得
    [[nodiscard]] HINSTANCE GetHInstance() const noexcept;

    //! @brief ウィンドウハンドルを取得
    [[nodiscard]] HWND GetHWND() const noexcept;

    //! @brief ウィンドウを取得
    [[nodiscard]] Window* GetWindow() noexcept;
    [[nodiscard]] const Window* GetWindow() const noexcept;

    //!@}

private:
    Application() = default;
    ~Application() = default;

    template<typename TGame>
    void MainLoop(TGame& game);
    void ProcessInput();
    void OnResize(uint32_t width, uint32_t height) noexcept;

    std::unique_ptr<Window> window_;

    ApplicationDesc desc_;
    bool initialized_ = false;
    bool running_ = false;
    bool shouldQuit_ = false;
};

// テンプレート実装
#include "application.inl"
