//----------------------------------------------------------------------------
//! @file   blend_state.h
//! @brief  ブレンドステート
//----------------------------------------------------------------------------
#pragma once

#include "dx11/gpu_common.h"
#include <memory>
#include <array>

//===========================================================================
//! ブレンドステート
//! @brief ブレンド設定をカプセル化
//===========================================================================
class BlendState final : private NonCopyable
{
public:
    //----------------------------------------------------------
    //! @name   ファクトリメソッド
    //----------------------------------------------------------
    //!@{

    //! ブレンドステートを作成
    //! @param [in] desc ブレンド記述子
    //! @return 成功時は有効なunique_ptr、失敗時はnullptr
    [[nodiscard]] static std::unique_ptr<BlendState> Create(const D3D11_BLEND_DESC& desc);

    //! ブレンド無効（デフォルト）
    [[nodiscard]] static std::unique_ptr<BlendState> CreateOpaque();

    //! アルファブレンド（半透明）
    [[nodiscard]] static std::unique_ptr<BlendState> CreateAlphaBlend();

    //! 加算ブレンド
    [[nodiscard]] static std::unique_ptr<BlendState> CreateAdditive();

    //! 乗算ブレンド
    [[nodiscard]] static std::unique_ptr<BlendState> CreateMultiply();

    //! プリマルチプライドアルファブレンド
    [[nodiscard]] static std::unique_ptr<BlendState> CreatePremultipliedAlpha();

    //! MAXブレンド（アルファ累積防止用）
    //! @details 重なり部分で色が明るくなる問題を防ぐ
    [[nodiscard]] static std::unique_ptr<BlendState> CreateMaxBlend();

    //!@}
    //----------------------------------------------------------
    //! @name   アクセサ
    //----------------------------------------------------------
    //!@{

    //! D3D11ブレンドステートを取得
    [[nodiscard]] ID3D11BlendState* GetD3DBlendState() const noexcept { return blend_.Get(); }

    //! 有効性チェック
    [[nodiscard]] bool IsValid() const noexcept { return blend_ != nullptr; }

    //!@}

private:
    BlendState() = default;

    ComPtr<ID3D11BlendState> blend_;
};
