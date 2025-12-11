//----------------------------------------------------------------------------
//! @file   shader_compiler.h
//! @brief  シェーダーコンパイラ
//----------------------------------------------------------------------------
#pragma once

#include "dx11/gpu_common.h"
#include "dx11/compile/shader_types_fwd.h"
#include <string>
#include <vector>

//----------------------------------------------------------------------------
//! シェーダーコンパイル結果
//----------------------------------------------------------------------------
struct ShaderCompileResult {
    bool success = false;               //!< 成功フラグ
    ComPtr<ID3DBlob> bytecode;          //!< コンパイル済みバイトコード
    std::string errorMessage;           //!< エラーメッセージ（失敗時）
    std::string warningMessage;         //!< 警告メッセージ
};

//----------------------------------------------------------------------------
//! シェーダーコンパイラインターフェース
//----------------------------------------------------------------------------
class IShaderCompiler {
public:
    virtual ~IShaderCompiler() = default;

    //! シェーダーソースをコンパイル
    //! @param [in] source シェーダーソースコード
    //! @param [in] sourceName ソース名（エラーメッセージ用）
    //! @param [in] profile シェーダープロファイル（例: "vs_5_0"）
    //! @param [in] entryPoint エントリーポイント関数名
    //! @param [in] defines マクロ定義リスト
    //! @return コンパイル結果
    [[nodiscard]] virtual ShaderCompileResult compile(
        const std::vector<char>& source,
        const std::string& sourceName,
        const std::string& profile,
        const std::string& entryPoint,
        const std::vector<ShaderDefine>& defines = {}) noexcept = 0;
};

//----------------------------------------------------------------------------
//! D3DCompileを使用したシェーダーコンパイラ
//! @note デバッグ/リリースは _DEBUG マクロで自動判定
//----------------------------------------------------------------------------
class D3DShaderCompiler : public IShaderCompiler {
public:
    D3DShaderCompiler() = default;
    ~D3DShaderCompiler() override = default;

    [[nodiscard]] ShaderCompileResult compile(
        const std::vector<char>& source,
        const std::string& sourceName,
        const std::string& profile,
        const std::string& entryPoint,
        const std::vector<ShaderDefine>& defines = {}) noexcept override;

private:
    [[nodiscard]] static unsigned int getCompileFlags() noexcept;
};
