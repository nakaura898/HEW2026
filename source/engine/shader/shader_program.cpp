//----------------------------------------------------------------------------
//! @file   shader_program.cpp
//! @brief  シェーダープログラム実装
//----------------------------------------------------------------------------
#include "shader_program.h"
#include "dx11/graphics_context.h"
#include "dx11/graphics_device.h"
#include "common/utility/hash.h"
#include "common/logging/logging.h"

//============================================================================
// ShaderProgram 実装
//============================================================================
std::unique_ptr<ShaderProgram> ShaderProgram::Create(
    ShaderPtr vs,
    ShaderPtr ps)
{
    return Create(std::move(vs), std::move(ps), nullptr, nullptr, nullptr);
}

std::unique_ptr<ShaderProgram> ShaderProgram::Create(
    ShaderPtr vs,
    ShaderPtr ps,
    ShaderPtr gs,
    ShaderPtr hs,
    ShaderPtr ds)
{
    if (!vs || !vs->IsVertex()) {
        LOG_ERROR("[ShaderProgram] 頂点シェーダーが無効です");
        return nullptr;
    }

    if (!ps || !ps->IsPixel()) {
        LOG_ERROR("[ShaderProgram] ピクセルシェーダーが無効です");
        return nullptr;
    }

    if (gs && !gs->IsGeometry()) {
        LOG_ERROR("[ShaderProgram] ジオメトリシェーダーが無効です");
        return nullptr;
    }

    if (hs && !hs->IsHull()) {
        LOG_ERROR("[ShaderProgram] ハルシェーダーが無効です");
        return nullptr;
    }

    if (ds && !ds->IsDomain()) {
        LOG_ERROR("[ShaderProgram] ドメインシェーダーが無効です");
        return nullptr;
    }

    auto program = std::unique_ptr<ShaderProgram>(new ShaderProgram());
    program->vs_ = std::move(vs);
    program->ps_ = std::move(ps);
    program->gs_ = std::move(gs);
    program->hs_ = std::move(hs);
    program->ds_ = std::move(ds);

    return program;
}

void ShaderProgram::Bind() const
{
    auto& ctx = GraphicsContext::Get();

    ctx.SetVertexShader(vs_.get());
    ctx.SetPixelShader(ps_.get());
    ctx.SetGeometryShader(gs_.get());
    ctx.SetHullShader(hs_.get());
    ctx.SetDomainShader(ds_.get());

    if (inputLayout_) {
        ctx.SetInputLayout(inputLayout_.Get());
    }
}

void ShaderProgram::Unbind() const
{
    auto& ctx = GraphicsContext::Get();

    ctx.SetVertexShader(nullptr);
    ctx.SetPixelShader(nullptr);
    ctx.SetGeometryShader(nullptr);
    ctx.SetHullShader(nullptr);
    ctx.SetDomainShader(nullptr);
    ctx.SetInputLayout(nullptr);
}

ID3D11InputLayout* ShaderProgram::GetOrCreateInputLayout(
    const D3D11_INPUT_ELEMENT_DESC* elements,
    uint32_t numElements)
{
    if (!elements || numElements == 0) {
        LOG_ERROR("[ShaderProgram] 入力要素が無効です");
        return nullptr;
    }

    if (!vs_ || !vs_->HasBytecode()) {
        LOG_ERROR("[ShaderProgram] 頂点シェーダーのバイトコードがありません");
        return nullptr;
    }

    // ハッシュ計算（入力レイアウトが同じならキャッシュを使用）
    uint64_t hash = 0;
    for (uint32_t i = 0; i < numElements; ++i) {
        const auto& e = elements[i];
        hash = HashUtil::Fnv1aString(e.SemanticName, hash);
        hash = HashUtil::Fnv1a(&e.SemanticIndex, sizeof(e.SemanticIndex), hash);
        hash = HashUtil::Fnv1a(&e.Format, sizeof(e.Format), hash);
        hash = HashUtil::Fnv1a(&e.InputSlot, sizeof(e.InputSlot), hash);
        hash = HashUtil::Fnv1a(&e.AlignedByteOffset, sizeof(e.AlignedByteOffset), hash);
        hash = HashUtil::Fnv1a(&e.InputSlotClass, sizeof(e.InputSlotClass), hash);
        hash = HashUtil::Fnv1a(&e.InstanceDataStepRate, sizeof(e.InstanceDataStepRate), hash);
    }

    // キャッシュヒット
    if (inputLayout_ && inputLayoutHash_ == hash) {
        return inputLayout_.Get();
    }

    // 新規作成
    ID3D11Device* device = GetD3D11Device();
    if (!device) return nullptr;

    ComPtr<ID3D11InputLayout> layout;
    HRESULT hr = device->CreateInputLayout(
        elements,
        numElements,
        vs_->Bytecode(),
        vs_->BytecodeSize(),
        &layout);

    RETURN_NULL_IF_FAILED(hr, "[ShaderProgram] InputLayout作成失敗");

    inputLayout_ = std::move(layout);
    inputLayoutHash_ = hash;

    return inputLayout_.Get();
}
