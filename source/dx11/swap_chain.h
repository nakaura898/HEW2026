//----------------------------------------------------------------------------
//! @file   swap_chain.h
//! @brief  スワップチェーン管理
//----------------------------------------------------------------------------
#pragma once

#include "dx11/gpu_common.h"
#include "dx11/gpu/gpu.h"
#include <cstdint>

//===========================================================================
//! 垂直同期モード
//===========================================================================
enum class VSyncMode : uint32_t
{
    Off = 0,      //!< 垂直同期なし（ティアリング許可時は即時表示）
    On = 1,       //!< 垂直同期あり（60Hz）
    Half = 2,     //!< 半分のリフレッシュレート（30Hz）
};

//===========================================================================
//! スワップチェーン管理クラス
//! @brief スワップチェーンの作成・リサイズ・表示を管理
//===========================================================================
class SwapChain final : private NonCopyable
{
public:
    //! コンストラクタ
    //! @param [in] hwnd ウィンドウハンドル
    //! @param [in] desc DXGI_SWAP_CHAIN_DESC1記述子
    //! @param [in] fullscreenDesc フルスクリーン記述子（nullptrでウィンドウモード）
    SwapChain(
        HWND hwnd,
        const DXGI_SWAP_CHAIN_DESC1& desc,
        const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fullscreenDesc = nullptr);

    ~SwapChain();
    SwapChain(SwapChain&&) noexcept;
    SwapChain& operator=(SwapChain&&) noexcept;

    //! バックバッファを画面に表示
    //! @param [in] mode 垂直同期モード
    //! @return 成功時true
    bool Present(VSyncMode mode = VSyncMode::On);

    //! バックバッファをリサイズ
    //! @param [in] width 新しい幅
    //! @param [in] height 新しい高さ
    //! @return 成功時true
    bool Resize(uint32_t width, uint32_t height);

    //! フルスクリーンモードを設定
    bool SetFullscreen(bool fullscreen);

    //! フルスクリーン状態を取得
    [[nodiscard]] bool IsFullscreen() const;

    //! 有効性チェック
    [[nodiscard]] bool IsValid() const noexcept { return swapChain_ != nullptr; }

    //! バックバッファを取得
    [[nodiscard]] Texture* GetBackBuffer() const noexcept { return backBuffer_.get(); }
    
    //! DXGIスワップチェーンを取得
    [[nodiscard]] IDXGISwapChain3* GetSwapChain() const noexcept { return swapChain_.Get(); }

private:
    ComPtr<IDXGISwapChain3> swapChain_;                          //!< DXGIスワップチェーン
    TexturePtr backBuffer_;                               //!< バックバッファテクスチャ
    HANDLE waitableObject_ = nullptr;                            //!< フレーム遅延待機オブジェクト
};
