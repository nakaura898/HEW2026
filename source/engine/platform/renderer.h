//----------------------------------------------------------------------------
//! @file   renderer.h
//! @brief  レンダラー管理クラス（シングルトン）
//----------------------------------------------------------------------------
#pragma once

#include "dx11/gpu_common.h"
#include "dx11/swap_chain.h"
#include "dx11/gpu/gpu.h"
#include <memory>
#include <cstdint>

//----------------------------------------------------------------------------
//! @brief レンダラー管理クラス（シングルトン）
//!
//! SwapChainを所有し、描画の最終出力を管理する。
//! 固定解像度のカラーバッファと深度バッファを持ち、
//! Present時にスワップチェーンへスケーリングコピーを行う。
//----------------------------------------------------------------------------
class Renderer final : private NonCopyableNonMovable
{
public:
    //! @brief シングルトンインスタンス取得
    static Renderer& Get() noexcept;

    //----------------------------------------------------------
    //! @name ライフサイクル
    //----------------------------------------------------------
    //!@{

    //! @brief 初期化
    //! @param [in] hwnd ウィンドウハンドル
    //! @param [in] windowWidth ウィンドウ幅
    //! @param [in] windowHeight ウィンドウ高さ
    //! @param [in] renderWidth レンダリング解像度幅（固定）
    //! @param [in] renderHeight レンダリング解像度高さ（固定）
    //! @param [in] vsync 垂直同期モード
    //! @return 成功時true
    [[nodiscard]] bool Initialize(
        HWND hwnd,
        uint32_t windowWidth,
        uint32_t windowHeight,
        uint32_t renderWidth,
        uint32_t renderHeight,
        VSyncMode vsync = VSyncMode::On);

    //! @brief 終了処理
    void Shutdown() noexcept;

    //! @brief 初期化成功確認
    [[nodiscard]] bool IsValid() const noexcept;

    //!@}

    //----------------------------------------------------------
    //! @name 描画操作
    //----------------------------------------------------------
    //!@{

    //! @brief 画面に表示（カラーバッファをスワップチェーンにコピー）
    void Present();

    //! @brief ウィンドウリサイズ（スワップチェーンのみリサイズ）
    //! @param [in] width 新しいウィンドウ幅
    //! @param [in] height 新しいウィンドウ高さ
    void Resize(uint32_t width, uint32_t height);

    //!@}

    //----------------------------------------------------------
    //! @name リソースアクセス
    //----------------------------------------------------------
    //!@{

    //! @brief カラーバッファを取得（固定解像度レンダーターゲット）
    [[nodiscard]] Texture* GetColorBuffer() noexcept;

    //! @brief 深度バッファを取得（固定解像度）
    [[nodiscard]] Texture* GetDepthBuffer() noexcept;

    //! @brief スワップチェーンのバックバッファを取得
    [[nodiscard]] Texture* GetBackBuffer() noexcept;

    //! @brief スワップチェーンを取得
    [[nodiscard]] SwapChain* GetSwapChain() noexcept;

    //! @brief レンダリング解像度幅を取得
    [[nodiscard]] uint32_t GetRenderWidth() const noexcept;

    //! @brief レンダリング解像度高さを取得
    [[nodiscard]] uint32_t GetRenderHeight() const noexcept;

    //!@}

private:
    Renderer() = default;
    ~Renderer() = default;

    bool CreateRenderTargets(uint32_t width, uint32_t height);

    std::unique_ptr<SwapChain> swapChain_;
    TexturePtr colorBuffer_;   //!< 固定解像度カラーバッファ
    TexturePtr depthBuffer_;   //!< 固定解像度深度バッファ

    uint32_t renderWidth_ = 0;   //!< レンダリング解像度幅
    uint32_t renderHeight_ = 0;  //!< レンダリング解像度高さ

    VSyncMode vsync_ = VSyncMode::On;
    bool initialized_ = false;
};
