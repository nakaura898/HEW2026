//----------------------------------------------------------------------------
//! @file   format.h
//! @brief  GPUフォーマット
//----------------------------------------------------------------------------
#pragma once

#include "dx11/gpu_common.h"

//===========================================================================
//! GPUフォーマット ユーティリティクラス
//!
//! @brief DXGI_FORMATのラッパー。フォーマット変換と情報取得を提供します。
//!
//! @details
//! DirectX11ではテクスチャやビューの作成時にフォーマット変換が頻繁に必要です。
//! 例えば：
//! - Typelessテクスチャから型付きビューを作成する
//! - sRGB/Linear間の変換（テクスチャ読み込み時）
//! - 深度テクスチャをSRVとDSVで異なるフォーマットで参照する
//!
//! このクラスはこれらの変換ロジックを一元管理し、コードの重複を防ぎます。
//!
//! @code
//!   // sRGB変換
//!   DXGI_FORMAT srgb = Format(DXGI_FORMAT_R8G8B8A8_UNORM).addSrgb();
//!
//!   // Typeless変換（テクスチャ作成用）
//!   DXGI_FORMAT typeless = Format(DXGI_FORMAT_D24_UNORM_S8_UINT).typeless();
//!
//!   // ビット深度取得
//!   int32_t bits = Format(DXGI_FORMAT_R16G16B16A16_FLOAT).bpp();  // 64
//! @endcode
//===========================================================================
class Format
{
public:
    //----------------------------------------------------------
    //! @name 初期化
    //----------------------------------------------------------
    //!@{

    //! デフォルトコンストラクタ
    Format() = default;

    //! コンストラクタ
    explicit Format(DXGI_FORMAT dxgiFormat)
        : dxgiFormat_(dxgiFormat)
    {
    }

    //!@}
    //----------------------------------------------------------
    //! @name 変換
//----------------------------------------------------------
    //!@{

    //! DXGIフォーマットへキャストします
    operator DXGI_FORMAT() const noexcept { return dxgiFormat_; }

    //! 型なしフォーマットに変換します
    DXGI_FORMAT typeless() const noexcept;

    //! カラーフォーマットに変換します
    DXGI_FORMAT toColor() const noexcept;

    //! 深度フォーマットに変換します
    DXGI_FORMAT toDepth() const noexcept;

    //! sRGB属性を付与します
    DXGI_FORMAT addSrgb() const noexcept;

    //! sRGB属性を除去します
    DXGI_FORMAT removeSrgb() const noexcept;

    //!@}
    //----------------------------------------------------------
    //! @name 取得
    //----------------------------------------------------------
    //!@{

    //! フォーマットのビット数を取得します
    int32_t bpp() const noexcept;

    //! 深度ステンシルフォーマットかどうかを取得します
    bool isDepthStencil() const noexcept;

    //!@}
private:
    DXGI_FORMAT dxgiFormat_ = DXGI_FORMAT_UNKNOWN;    //!< DXGIフォーマット
};
