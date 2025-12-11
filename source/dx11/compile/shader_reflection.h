//----------------------------------------------------------------------------
//! @file   shader_reflection.h
//! @brief  シェーダーリフレクション
//----------------------------------------------------------------------------
#pragma once

#include "dx11/gpu_common.h"
#include "dx11/graphics/shader/shader_parameter.h"
#include <memory>
#include <vector>
#include <string>

//===========================================================================
//! リソースバインド情報（スロット情報付き）
//===========================================================================
struct ShaderResourceBindInfo
{
    D3D11_SHADER_INPUT_BIND_DESC desc;  //!< DirectX11バインド記述子
    uint32_t bufferSize = 0;            //!< 定数バッファの場合のサイズ
};

//===========================================================================
//! シェーダーリフレクション
//! @note コンパイル済みシェーダーからパラメータ情報を抽出
//!       ShaderResourceとは独立したユーティリティクラス
//===========================================================================
class ShaderReflection final : private NonCopyable
{
public:
    //! バイトコードからリフレクション情報を生成
    //! @param [in] bytecode コンパイル済みバイトコード
    //! @return 成功時は有効なunique_ptr、失敗時はnullptr
    [[nodiscard]] static std::unique_ptr<ShaderReflection> Create(ID3DBlob* bytecode);

    //----------------------------------------------------------
    //! @name   情報取得
    //----------------------------------------------------------
    //!@{

    //! 定数バッファ情報を取得
    [[nodiscard]] std::span<const ShaderResourceBindInfo> GetConstantBuffers() const noexcept
    {
        return constantBuffers_;
    }

    //! テクスチャバインド情報を取得
    [[nodiscard]] std::span<const ShaderResourceBindInfo> GetTextures() const noexcept
    {
        return textures_;
    }

    //! サンプラーバインド情報を取得
    [[nodiscard]] std::span<const ShaderResourceBindInfo> GetSamplers() const noexcept
    {
        return samplers_;
    }

    //! 入力レイアウト情報を取得（VSのみ有効）
    [[nodiscard]] std::span<const D3D11_SIGNATURE_PARAMETER_DESC> GetInputElements() const noexcept
    {
        return inputElements_;
    }

    //!@}
    //----------------------------------------------------------
    //! @name   ユーティリティ
    //----------------------------------------------------------
    //!@{

    //! ShaderParameterMapを構築
    [[nodiscard]] ShaderParameterMap BuildParameterMap() const;

    //! 有効かどうか
    [[nodiscard]] bool IsValid() const noexcept { return reflection_ != nullptr; }

    //!@}

private:
    ShaderReflection() = default;

    //! リフレクション情報を解析
    void Parse();

    ComPtr<ID3D11ShaderReflection> reflection_;

    std::vector<ShaderResourceBindInfo> constantBuffers_;
    std::vector<ShaderResourceBindInfo> textures_;
    std::vector<ShaderResourceBindInfo> samplers_;
    std::vector<D3D11_SIGNATURE_PARAMETER_DESC> inputElements_;
};
