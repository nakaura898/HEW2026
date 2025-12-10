//----------------------------------------------------------------------------
//! @file   shader.h
//! @brief  GPUシェーダークラス（統一設計）
//----------------------------------------------------------------------------
#pragma once

#include "dx11/gpu_common.h"
#include "gpu_resource.h"
#include "dx11/compile/shader_type.h"
#include <d3dcompiler.h>

//===========================================================================
//! @brief GPUシェーダークラス（統一設計）
//! @note  Layout (16 bytes): shader_(8) + bytecode_(8)
//===========================================================================
class Shader : private NonCopyable {
public:
    //----------------------------------------------------------
    //! @name   ファクトリメソッド
    //----------------------------------------------------------
    //!@{

    //! 頂点シェーダーを作成
    [[nodiscard]] static std::shared_ptr<Shader> CreateVertexShader(ComPtr<ID3DBlob> bytecode);

    //! ピクセルシェーダーを作成
    [[nodiscard]] static std::shared_ptr<Shader> CreatePixelShader(ComPtr<ID3DBlob> bytecode);

    //! ジオメトリシェーダーを作成
    [[nodiscard]] static std::shared_ptr<Shader> CreateGeometryShader(ComPtr<ID3DBlob> bytecode);

    //! コンピュートシェーダーを作成
    [[nodiscard]] static std::shared_ptr<Shader> CreateComputeShader(ComPtr<ID3DBlob> bytecode);

    //! ハルシェーダーを作成
    [[nodiscard]] static std::shared_ptr<Shader> CreateHullShader(ComPtr<ID3DBlob> bytecode);

    //! ドメインシェーダーを作成
    [[nodiscard]] static std::shared_ptr<Shader> CreateDomainShader(ComPtr<ID3DBlob> bytecode);

    //!@}
    //----------------------------------------------------------
    //! @name   コンストラクタ
    //----------------------------------------------------------
    //!@{

    //! デフォルトコンストラクタ
    Shader() = default;

    //! コンストラクタ
    Shader(ComPtr<ID3D11DeviceChild> shader, ComPtr<ID3DBlob> bytecode = nullptr)
        : shader_(std::move(shader))
        , bytecode_(std::move(bytecode)) {}

    //!@}

    //! GPUメモリサイズを取得
    [[nodiscard]] size_t GpuSize() const noexcept {
        return bytecode_ ? bytecode_->GetBufferSize() : 0;
    }

    //! D3D11シェーダーを取得
    [[nodiscard]] ID3D11DeviceChild* Get() const noexcept { return shader_.Get(); }

    //! シェーダー種別を取得
    [[nodiscard]] ShaderType GetShaderType() const noexcept {
        if (IsVertex())   return ShaderType::Vertex;
        if (IsPixel())    return ShaderType::Pixel;
        if (IsGeometry()) return ShaderType::Geometry;
        if (IsCompute())  return ShaderType::Compute;
        if (IsHull())     return ShaderType::Hull;
        return ShaderType::Domain;
    }

    //! 頂点シェーダーとして取得
    [[nodiscard]] ID3D11VertexShader* AsVs() const noexcept {
        return static_cast<ID3D11VertexShader*>(shader_.Get());
    }
    //! ピクセルシェーダーとして取得
    [[nodiscard]] ID3D11PixelShader* AsPs() const noexcept {
        return static_cast<ID3D11PixelShader*>(shader_.Get());
    }
    //! ジオメトリシェーダーとして取得
    [[nodiscard]] ID3D11GeometryShader* AsGs() const noexcept {
        return static_cast<ID3D11GeometryShader*>(shader_.Get());
    }
    //! コンピュートシェーダーとして取得
    [[nodiscard]] ID3D11ComputeShader* AsCs() const noexcept {
        return static_cast<ID3D11ComputeShader*>(shader_.Get());
    }
    //! ハルシェーダーとして取得
    [[nodiscard]] ID3D11HullShader* AsHs() const noexcept {
        return static_cast<ID3D11HullShader*>(shader_.Get());
    }
    //! ドメインシェーダーとして取得
    [[nodiscard]] ID3D11DomainShader* AsDs() const noexcept {
        return static_cast<ID3D11DomainShader*>(shader_.Get());
    }

    //! バイトコードを取得（入力レイアウト作成用）
    [[nodiscard]] const void* Bytecode() const noexcept {
        return bytecode_ ? bytecode_->GetBufferPointer() : nullptr;
    }
    //! バイトコードサイズを取得
    [[nodiscard]] size_t BytecodeSize() const noexcept {
        return bytecode_ ? bytecode_->GetBufferSize() : 0;
    }
    //! バイトコードを持つか判定
    [[nodiscard]] bool HasBytecode() const noexcept { return bytecode_ != nullptr; }

    //! 頂点シェーダーか判定
    [[nodiscard]] bool IsVertex() const noexcept { return Check<ID3D11VertexShader>(); }
    //! ピクセルシェーダーか判定
    [[nodiscard]] bool IsPixel() const noexcept { return Check<ID3D11PixelShader>(); }
    //! ジオメトリシェーダーか判定
    [[nodiscard]] bool IsGeometry() const noexcept { return Check<ID3D11GeometryShader>(); }
    //! コンピュートシェーダーか判定
    [[nodiscard]] bool IsCompute() const noexcept { return Check<ID3D11ComputeShader>(); }
    //! ハルシェーダーか判定
    [[nodiscard]] bool IsHull() const noexcept { return Check<ID3D11HullShader>(); }
    //! ドメインシェーダーか判定
    [[nodiscard]] bool IsDomain() const noexcept { return Check<ID3D11DomainShader>(); }

private:
    //! QueryInterfaceで型チェック
    template<typename T>
    [[nodiscard]] bool Check() const noexcept {
        if (!shader_) return false;
        T* ptr = nullptr;
        HRESULT hr = shader_->QueryInterface(__uuidof(T), reinterpret_cast<void**>(&ptr));
        if (SUCCEEDED(hr) && ptr) {
            ptr->Release();
            return true;
        }
        return false;
    }

    ComPtr<ID3D11DeviceChild> shader_;
    ComPtr<ID3DBlob> bytecode_;
};

//! シェーダースマートポインタ
using ShaderPtr = std::shared_ptr<Shader>;
