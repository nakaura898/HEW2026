//----------------------------------------------------------------------------
//! @file   graphics_device.h
//! @brief  D3D11グラフィクスデバイスマネージャー
//!
//! @note スレッドセーフ性:
//!       - Initialize()/Shutdown(): メインスレッドからのみ呼び出し可能
//!       - Device(): スレッドセーフ（ID3D11Deviceはマルチスレッド対応）
//!       - Get(): スレッドセーフ（初期化後）
//!
//! @warning リソース作成（CreateBuffer, CreateTexture等）は
//!          IDXGIDeviceSubObject経由で内部的にシリアライズされるが、
//!          パフォーマンス上、同時大量作成は避けることを推奨。
//----------------------------------------------------------------------------
#pragma once

#include <d3d11_4.h>
#include <wrl/client.h>

//===========================================================================
//! D3D11デバイスマネージャー（シングルトン）
//! @brief Device を一元管理（Context は GraphicsContext が管理）
//===========================================================================
class GraphicsDevice final
{
public:
    //! シングルトンインスタンスを取得
    static GraphicsDevice& Get() noexcept;

    //! デバイスを作成・初期化
    //! @param [in] enableDebug デバッグレイヤーを有効にするか
    //! @return 成功したらtrue
    [[nodiscard]] bool Initialize(bool enableDebug = false);

    //! 終了処理
    void Shutdown() noexcept;

    //! D3D11デバイス5を取得
    [[nodiscard]] ID3D11Device5* Device() const noexcept { return device_.Get(); }

    //! 有効性チェック
    [[nodiscard]] bool IsValid() const noexcept { return device_ != nullptr; }

    // コピー・ムーブ禁止
    GraphicsDevice(const GraphicsDevice&) = delete;
    GraphicsDevice& operator=(const GraphicsDevice&) = delete;
    GraphicsDevice(GraphicsDevice&&) = delete;
    GraphicsDevice& operator=(GraphicsDevice&&) = delete;

private:
    GraphicsDevice() = default;
    ~GraphicsDevice() = default;

    Microsoft::WRL::ComPtr<ID3D11Device5> device_;
};

//! D3D11デバイス5を取得（ショートカット）
ID3D11Device5* GetD3D11Device() noexcept;
