//----------------------------------------------------------------------------
//! @file   shader_compiler.cpp
//! @brief  シェーダーコンパイラ実装
//----------------------------------------------------------------------------
#include "shader_compiler.h"

#pragma comment(lib, "d3dcompiler.lib")

//----------------------------------------------------------------------------
ShaderCompileResult D3DShaderCompiler::compile(
    const std::vector<char>& source,
    const std::string& sourceName,
    const std::string& profile,
    const std::string& entryPoint,
    const std::vector<ShaderDefine>& defines) noexcept
{
    ShaderCompileResult result;

    if (source.empty()) {
        result.errorMessage = "シェーダーソースが空です";
        return result;
    }

    if (entryPoint.empty()) {
        result.errorMessage = "エントリーポイント名が空です";
        return result;
    }

    if (profile.empty()) {
        result.errorMessage = "シェーダープロファイルが空です";
        return result;
    }

    // D3D_SHADER_MACROの配列を構築
    std::vector<D3D_SHADER_MACRO> macros;
    macros.reserve(defines.size() + 1);

    for (const auto& def : defines) {
        macros.push_back({ def.name.c_str(), def.value.c_str() });
    }
    macros.push_back({ nullptr, nullptr }); // 終端

    // コンパイル
    ComPtr<ID3DBlob> shaderBlob;
    ComPtr<ID3DBlob> errorBlob;

    HRESULT hr = D3DCompile(
        source.data(),
        source.size(),
        sourceName.empty() ? nullptr : sourceName.c_str(),
        macros.data(),
        D3D_COMPILE_STANDARD_FILE_INCLUDE,
        entryPoint.c_str(),
        profile.c_str(),
        getCompileFlags(),
        0,
        &shaderBlob,
        &errorBlob);

    // エラー/警告メッセージを取得
    if (errorBlob) {
        std::string message(
            static_cast<const char*>(errorBlob->GetBufferPointer()),
            errorBlob->GetBufferSize());

        // 末尾のnull文字と空白を除去
        while (!message.empty() && (message.back() == '\0' || message.back() == '\n' || message.back() == '\r')) {
            message.pop_back();
        }

        if (FAILED(hr)) {
            result.errorMessage = std::move(message);
        } else {
            result.warningMessage = std::move(message);
        }
    }

    if (FAILED(hr)) {
        if (result.errorMessage.empty()) {
            result.errorMessage = "D3DCompileに失敗しました (HRESULT: " + std::to_string(hr) + ")";
        }
        return result;
    }

    // バイトコードを移譲
    result.success = true;
    result.bytecode = std::move(shaderBlob);

    return result;
}

//----------------------------------------------------------------------------
unsigned int D3DShaderCompiler::getCompileFlags() noexcept
{
    unsigned int flags = 0;

#ifdef _DEBUG
    // デバッグビルド: デバッグ情報付き、最適化なし
    flags |= D3DCOMPILE_DEBUG;
    flags |= D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    // リリースビルド: 最大最適化
    flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

    // 厳格モード
    flags |= D3DCOMPILE_ENABLE_STRICTNESS;

    // 行列の列優先パッキング（DirectXMath互換）
    flags |= D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR;

    return flags;
}
