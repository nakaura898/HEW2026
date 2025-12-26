//----------------------------------------------------------------------------
//! @file   sampler_state.h
//! @brief  サンプラーステート
//----------------------------------------------------------------------------
#pragma once

#include "dx11/gpu_common.h"
#include <memory>

//===========================================================================
//! サンプラーステート
//! @brief テクスチャサンプリング設定をカプセル化
//===========================================================================
class SamplerState final : private NonCopyable
{
public:
    //----------------------------------------------------------
    //! @name   ファクトリメソッド
    //----------------------------------------------------------
    //!@{

    //! サンプラーステートを作成
    //! @param [in] desc サンプラー記述子
    //! @return 成功時は有効なunique_ptr、失敗時はnullptr
    [[nodiscard]] static std::unique_ptr<SamplerState> Create(const D3D11_SAMPLER_DESC& desc);

    //! デフォルトのサンプラーステートを作成（リニアフィルタ、ラップ）
    [[nodiscard]] static std::unique_ptr<SamplerState> CreateDefault();

    //! ポイントサンプリングのサンプラーステートを作成
    [[nodiscard]] static std::unique_ptr<SamplerState> CreatePoint();

    //! 異方性フィルタリングのサンプラーステートを作成
    //! @param [in] maxAnisotropy 最大異方性（1-16）
    [[nodiscard]] static std::unique_ptr<SamplerState> CreateAnisotropic(uint32_t maxAnisotropy = 16);

    //! 比較サンプラー（シャドウマップ用）を作成
    [[nodiscard]] static std::unique_ptr<SamplerState> CreateComparison();

    //! クランプサンプラーを作成（テクスチャ境界で端のピクセルを繰り返す）
    [[nodiscard]] static std::unique_ptr<SamplerState> CreateClamp();

    //!@}
    //----------------------------------------------------------
    //! @name   アクセサ
    //----------------------------------------------------------
    //!@{

    //! D3D11サンプラーステートを取得
    [[nodiscard]] ID3D11SamplerState* GetD3DSamplerState() const noexcept { return sampler_.Get(); }

    //! 有効性チェック
    [[nodiscard]] bool IsValid() const noexcept { return sampler_ != nullptr; }

    //!@}

private:
    SamplerState() = default;

    ComPtr<ID3D11SamplerState> sampler_;
};
