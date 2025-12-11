//----------------------------------------------------------------------------
//! @file   global_shader.h
//! @brief  グローバルシェーダー基底クラス
//----------------------------------------------------------------------------
#pragma once

#include "dx11/gpu_common.h"
#include "dx11/gpu/gpu.h"
#include "dx11/compile/shader_type.h"
#include "dx11/compile/shader_types_fwd.h"
#include <string>
#include <vector>
#include <typeindex>

//===========================================================================
//! グローバルシェーダー基底クラス
//!
//! @brief 静的に定義されるシェーダーの基底クラス
//!        派生クラスで GetSourcePath(), GetShaderType() を実装する
//!
//! @code
//!   // 派生クラス定義
//!   class MyVertexShader : public GlobalShader {
//!   public:
//!       static constexpr const char* SourcePath = "shaders:/my_vs.hlsl";
//!       static constexpr ShaderType Type = ShaderType::Vertex;
//!
//!       const char* GetSourcePath() const override { return SourcePath; }
//!       ShaderType GetShaderType() const override { return Type; }
//!   };
//!
//!   // 使用（ShaderManager経由）
//!   auto* shader = ShaderManager::Get().GetGlobalShader<MyVertexShader>();
//!   GraphicsContext::Get().SetVertexShader(shader->GetShader());
//! @endcode
//===========================================================================
class GlobalShader : private NonCopyable
{
public:
    virtual ~GlobalShader() = default;

    //----------------------------------------------------------
    //! @name   派生クラスで実装
    //----------------------------------------------------------
    //!@{

    //! シェーダーソースパスを取得
    [[nodiscard]] virtual const char* GetSourcePath() const = 0;

    //! シェーダータイプを取得
    [[nodiscard]] virtual ShaderType GetShaderType() const = 0;

    //! マクロ定義を取得（オプション）
    [[nodiscard]] virtual std::vector<ShaderDefine> GetDefines() const { return {}; }

    //! エントリーポイント名を取得（オプション、デフォルトはタイプ別）
    [[nodiscard]] virtual const char* GetEntryPoint() const { return nullptr; }

    //!@}
    //----------------------------------------------------------
    //! @name   シェーダーアクセス
    //----------------------------------------------------------
    //!@{

    //! コンパイル済みシェーダーを取得
    [[nodiscard]] Shader* GetShader() const noexcept { return shader_.get(); }

    //! シェーダーが有効か
    [[nodiscard]] bool IsValid() const noexcept { return shader_ != nullptr; }

    //! バイトコードを取得（VSのInputLayout作成用）
    [[nodiscard]] const void* GetBytecode() const noexcept {
        return shader_ ? shader_->Bytecode() : nullptr;
    }

    //! バイトコードサイズを取得
    [[nodiscard]] size_t GetBytecodeSize() const noexcept {
        return shader_ ? shader_->BytecodeSize() : 0;
    }

    //!@}

protected:
    GlobalShader() = default;

    //! シェーダーを設定（ShaderManagerから呼び出し）
    void SetShader(ShaderPtr shader) { shader_ = std::move(shader); }

private:
    friend class ShaderManager;
    ShaderPtr shader_;
};

//===========================================================================
//! グローバルシェーダー型情報
//!
//! @brief 派生クラスの型IDを取得するためのヘルパー
//===========================================================================
class GlobalShaderTypeInfo
{
public:
    //! 型のインデックスを取得
    template<typename T>
    [[nodiscard]] static std::type_index GetTypeIndex() {
        static_assert(std::is_base_of_v<GlobalShader, T>, "T must derive from GlobalShader");
        return std::type_index(typeid(T));
    }

    //! 新しいインスタンスを作成
    template<typename T>
    [[nodiscard]] static std::unique_ptr<GlobalShader> CreateInstance() {
        static_assert(std::is_base_of_v<GlobalShader, T>, "T must derive from GlobalShader");
        return std::make_unique<T>();
    }
};

//===========================================================================
//! グローバルシェーダー定義マクロ
//!
//! @code
//!   // ヘッダーで宣言
//!   DECLARE_GLOBAL_SHADER(MyVertexShader, ShaderType::Vertex, "shaders:/my_vs.hlsl")
//!
//!   // または手動で定義
//!   class MyVertexShader : public GlobalShader {
//!       IMPLEMENT_GLOBAL_SHADER(MyVertexShader, ShaderType::Vertex, "shaders:/my_vs.hlsl")
//!   };
//! @endcode
//===========================================================================

//! シンプルなグローバルシェーダー宣言
#define DECLARE_GLOBAL_SHADER(ClassName, ShaderTypeValue, SourcePathStr) \
    class ClassName : public GlobalShader { \
    public: \
        [[nodiscard]] const char* GetSourcePath() const override { return SourcePathStr; } \
        [[nodiscard]] ShaderType GetShaderType() const override { return ShaderTypeValue; } \
    }

//! グローバルシェーダー実装ヘルパー（クラス内で使用）
#define IMPLEMENT_GLOBAL_SHADER(ClassName, ShaderTypeValue, SourcePathStr) \
    public: \
        [[nodiscard]] const char* GetSourcePath() const override { return SourcePathStr; } \
        [[nodiscard]] ShaderType GetShaderType() const override { return ShaderTypeValue; }
