//----------------------------------------------------------------------------
//! @file   shader_program.h
//! @brief  シェーダープログラム
//----------------------------------------------------------------------------
#pragma once

#include "dx11/gpu_common.h"
#include "dx11/gpu/gpu.h"
#include <memory>

//===========================================================================
//! シェーダープログラム
//!
//! @brief VS/PS/GS/HS/DSをまとめて管理するクラス
//!
//! @code
//!   auto program = ShaderProgram::Create(vs, ps);
//!   program->Bind();  // 全シェーダーをパイプラインに設定
//! @endcode
//===========================================================================
class ShaderProgram final : private NonCopyable
{
public:
    //! VS/PSからプログラムを作成
    [[nodiscard]] static std::unique_ptr<ShaderProgram> Create(
        ShaderPtr vs,
        ShaderPtr ps);

    //! 全シェーダー指定で作成
    [[nodiscard]] static std::unique_ptr<ShaderProgram> Create(
        ShaderPtr vs,
        ShaderPtr ps,
        ShaderPtr gs,
        ShaderPtr hs = nullptr,
        ShaderPtr ds = nullptr);

    ~ShaderProgram() = default;

    // ムーブ可能
    ShaderProgram(ShaderProgram&&) noexcept = default;
    ShaderProgram& operator=(ShaderProgram&&) noexcept = default;

    //----------------------------------------------------------
    //! @name   パイプライン操作
    //----------------------------------------------------------
    //!@{

    //! 全シェーダーをパイプラインにバインド
    void Bind() const;

    //! 全シェーダーをアンバインド
    void Unbind() const;

    //!@}
    //----------------------------------------------------------
    //! @name   InputLayout
    //----------------------------------------------------------
    //!@{

    //! InputLayoutを作成・キャッシュして返す
    [[nodiscard]] ID3D11InputLayout* GetOrCreateInputLayout(
        const D3D11_INPUT_ELEMENT_DESC* elements,
        uint32_t numElements);

    //! 現在のInputLayoutを取得
    [[nodiscard]] ID3D11InputLayout* GetInputLayout() const noexcept { return inputLayout_.Get(); }

    //!@}
    //----------------------------------------------------------
    //! @name   シェーダー取得
    //----------------------------------------------------------
    //!@{

    [[nodiscard]] Shader* GetVertexShader() const noexcept { return vs_.get(); }
    [[nodiscard]] Shader* GetPixelShader() const noexcept { return ps_.get(); }
    [[nodiscard]] Shader* GetGeometryShader() const noexcept { return gs_.get(); }
    [[nodiscard]] Shader* GetHullShader() const noexcept { return hs_.get(); }
    [[nodiscard]] Shader* GetDomainShader() const noexcept { return ds_.get(); }

    //! 有効かどうか（最低限VS/PSが設定されている）
    [[nodiscard]] bool IsValid() const noexcept { return vs_ && ps_; }

    //!@}

private:
    ShaderProgram() = default;

    ShaderPtr vs_;
    ShaderPtr ps_;
    ShaderPtr gs_;
    ShaderPtr hs_;
    ShaderPtr ds_;

    ComPtr<ID3D11InputLayout> inputLayout_;
    uint64_t inputLayoutHash_ = 0;
};
