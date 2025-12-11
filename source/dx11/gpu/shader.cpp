//----------------------------------------------------------------------------
//! @file   shader.cpp
//! @brief  GPUシェーダークラス実装
//----------------------------------------------------------------------------
#include "shader.h"
#include "dx11/gpu_common.h"
#include "dx11/graphics_device.h"
#include "common/logging/logging.h"

//----------------------------------------------------------------------------
// Shader静的ファクトリメソッド
//----------------------------------------------------------------------------

//! 頂点シェーダーを作成
std::shared_ptr<Shader> Shader::CreateVertexShader(ComPtr<ID3DBlob> bytecode)
{
    RETURN_NULL_IF_NULL(bytecode.Get(), "[Shader] bytecodeがnullです");

    auto* device = GetD3D11Device();
    RETURN_NULL_IF_NULL(device, "[Shader] D3D11Deviceがnullです");

    ComPtr<ID3D11VertexShader> shader;
    HRESULT hr = device->CreateVertexShader(
        bytecode->GetBufferPointer(),
        bytecode->GetBufferSize(),
        nullptr,
        shader.GetAddressOf());

    RETURN_NULL_IF_FAILED(hr, "[Shader] 頂点シェーダー作成失敗");

    ComPtr<ID3D11DeviceChild> deviceChild;
    shader.As(&deviceChild);

    return std::make_shared<Shader>(std::move(deviceChild), std::move(bytecode));
}

//! ピクセルシェーダーを作成
std::shared_ptr<Shader> Shader::CreatePixelShader(ComPtr<ID3DBlob> bytecode)
{
    RETURN_NULL_IF_NULL(bytecode.Get(), "[Shader] bytecodeがnullです");

    auto* device = GetD3D11Device();
    RETURN_NULL_IF_NULL(device, "[Shader] D3D11Deviceがnullです");

    ComPtr<ID3D11PixelShader> shader;
    HRESULT hr = device->CreatePixelShader(
        bytecode->GetBufferPointer(),
        bytecode->GetBufferSize(),
        nullptr,
        shader.GetAddressOf());

    RETURN_NULL_IF_FAILED(hr, "[Shader] ピクセルシェーダー作成失敗");

    ComPtr<ID3D11DeviceChild> deviceChild;
    shader.As(&deviceChild);

    return std::make_shared<Shader>(std::move(deviceChild), std::move(bytecode));
}

//! ジオメトリシェーダーを作成
std::shared_ptr<Shader> Shader::CreateGeometryShader(ComPtr<ID3DBlob> bytecode)
{
    RETURN_NULL_IF_NULL(bytecode.Get(), "[Shader] bytecodeがnullです");

    auto* device = GetD3D11Device();
    RETURN_NULL_IF_NULL(device, "[Shader] D3D11Deviceがnullです");

    ComPtr<ID3D11GeometryShader> shader;
    HRESULT hr = device->CreateGeometryShader(
        bytecode->GetBufferPointer(),
        bytecode->GetBufferSize(),
        nullptr,
        shader.GetAddressOf());

    RETURN_NULL_IF_FAILED(hr, "[Shader] ジオメトリシェーダー作成失敗");

    ComPtr<ID3D11DeviceChild> deviceChild;
    shader.As(&deviceChild);

    return std::make_shared<Shader>(std::move(deviceChild), std::move(bytecode));
}

//! コンピュートシェーダーを作成
std::shared_ptr<Shader> Shader::CreateComputeShader(ComPtr<ID3DBlob> bytecode)
{
    RETURN_NULL_IF_NULL(bytecode.Get(), "[Shader] bytecodeがnullです");

    auto* device = GetD3D11Device();
    RETURN_NULL_IF_NULL(device, "[Shader] D3D11Deviceがnullです");

    ComPtr<ID3D11ComputeShader> shader;
    HRESULT hr = device->CreateComputeShader(
        bytecode->GetBufferPointer(),
        bytecode->GetBufferSize(),
        nullptr,
        shader.GetAddressOf());

    RETURN_NULL_IF_FAILED(hr, "[Shader] コンピュートシェーダー作成失敗");

    ComPtr<ID3D11DeviceChild> deviceChild;
    shader.As(&deviceChild);

    return std::make_shared<Shader>(std::move(deviceChild), std::move(bytecode));
}

//! ハルシェーダーを作成
std::shared_ptr<Shader> Shader::CreateHullShader(ComPtr<ID3DBlob> bytecode)
{
    RETURN_NULL_IF_NULL(bytecode.Get(), "[Shader] bytecodeがnullです");

    auto* device = GetD3D11Device();
    RETURN_NULL_IF_NULL(device, "[Shader] D3D11Deviceがnullです");

    ComPtr<ID3D11HullShader> shader;
    HRESULT hr = device->CreateHullShader(
        bytecode->GetBufferPointer(),
        bytecode->GetBufferSize(),
        nullptr,
        shader.GetAddressOf());

    RETURN_NULL_IF_FAILED(hr, "[Shader] ハルシェーダー作成失敗");

    ComPtr<ID3D11DeviceChild> deviceChild;
    shader.As(&deviceChild);

    return std::make_shared<Shader>(std::move(deviceChild), std::move(bytecode));
}

//! ドメインシェーダーを作成
std::shared_ptr<Shader> Shader::CreateDomainShader(ComPtr<ID3DBlob> bytecode)
{
    RETURN_NULL_IF_NULL(bytecode.Get(), "[Shader] bytecodeがnullです");

    auto* device = GetD3D11Device();
    RETURN_NULL_IF_NULL(device, "[Shader] D3D11Deviceがnullです");

    ComPtr<ID3D11DomainShader> shader;
    HRESULT hr = device->CreateDomainShader(
        bytecode->GetBufferPointer(),
        bytecode->GetBufferSize(),
        nullptr,
        shader.GetAddressOf());

    RETURN_NULL_IF_FAILED(hr, "[Shader] ドメインシェーダー作成失敗");

    ComPtr<ID3D11DeviceChild> deviceChild;
    shader.As(&deviceChild);

    return std::make_shared<Shader>(std::move(deviceChild), std::move(bytecode));
}
