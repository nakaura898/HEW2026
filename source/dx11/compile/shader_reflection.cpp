//----------------------------------------------------------------------------
//! @file   shader_reflection.cpp
//! @brief  シェーダーリフレクション実装
//----------------------------------------------------------------------------
#include "shader_reflection.h"
#include "common/logging/logging.h"

//----------------------------------------------------------------------------
std::unique_ptr<ShaderReflection> ShaderReflection::Create(ID3DBlob* bytecode)
{
    if (!bytecode || bytecode->GetBufferSize() == 0) {
        LOG_ERROR("[ShaderReflection] バイトコードが空です");
        return nullptr;
    }

    auto reflection = std::unique_ptr<ShaderReflection>(new ShaderReflection());

    HRESULT hr = D3DReflect(
        bytecode->GetBufferPointer(),
        bytecode->GetBufferSize(),
        IID_ID3D11ShaderReflection,
        reinterpret_cast<void**>(reflection->reflection_.GetAddressOf()));

    if (FAILED(hr)) {
        LOG_HRESULT(hr, "[ShaderReflection] D3DReflectに失敗しました");
        return nullptr;
    }

    reflection->Parse();
    return reflection;
}

//----------------------------------------------------------------------------
void ShaderReflection::Parse()
{
    if (!reflection_) return;

    D3D11_SHADER_DESC shaderDesc{};
    reflection_->GetDesc(&shaderDesc);

    // 定数バッファを解析
    for (UINT i = 0; i < shaderDesc.ConstantBuffers; ++i) {
        auto* cb = reflection_->GetConstantBufferByIndex(i);
        if (!cb) continue;

        D3D11_SHADER_BUFFER_DESC bufferDesc{};
        cb->GetDesc(&bufferDesc);

        // バインドスロットを取得
        D3D11_SHADER_INPUT_BIND_DESC bindDesc{};
        if (SUCCEEDED(reflection_->GetResourceBindingDescByName(bufferDesc.Name, &bindDesc))) {
            ShaderResourceBindInfo info;
            info.desc = bindDesc;
            info.bufferSize = bufferDesc.Size;
            constantBuffers_.push_back(info);
        }
    }

    // バウンドリソースを解析
    for (UINT i = 0; i < shaderDesc.BoundResources; ++i) {
        D3D11_SHADER_INPUT_BIND_DESC bindDesc{};
        if (FAILED(reflection_->GetResourceBindingDesc(i, &bindDesc))) {
            continue;
        }

        switch (bindDesc.Type) {
        case D3D_SIT_TEXTURE:
        case D3D_SIT_STRUCTURED:
        case D3D_SIT_BYTEADDRESS:
            {
                ShaderResourceBindInfo info;
                info.desc = bindDesc;
                textures_.push_back(info);
            }
            break;

        case D3D_SIT_SAMPLER:
            {
                ShaderResourceBindInfo info;
                info.desc = bindDesc;
                samplers_.push_back(info);
            }
            break;

        default:
            break;
        }
    }

    // 入力シグネチャを解析（VSの場合）
    for (UINT i = 0; i < shaderDesc.InputParameters; ++i) {
        D3D11_SIGNATURE_PARAMETER_DESC paramDesc{};
        if (FAILED(reflection_->GetInputParameterDesc(i, &paramDesc))) {
            continue;
        }
        inputElements_.push_back(paramDesc);
    }
}

//----------------------------------------------------------------------------
ShaderParameterMap ShaderReflection::BuildParameterMap() const
{
    ShaderParameterMap map;

    for (const auto& cb : constantBuffers_) {
        map.AddParameter(
            ShaderParameterType::ConstantBuffer,
            static_cast<uint8_t>(cb.desc.BindPoint),
            static_cast<uint16_t>(cb.bufferSize),
            cb.desc.Name);
    }

    for (const auto& tex : textures_) {
        map.AddParameter(
            ShaderParameterType::ShaderResource,
            static_cast<uint8_t>(tex.desc.BindPoint),
            0,
            tex.desc.Name);
    }

    for (const auto& samp : samplers_) {
        map.AddParameter(
            ShaderParameterType::Sampler,
            static_cast<uint8_t>(samp.desc.BindPoint),
            0,
            samp.desc.Name);
    }

    return map;
}
