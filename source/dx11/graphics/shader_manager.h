//----------------------------------------------------------------------------
//! @file   shader_manager.h
//! @brief  シェーダーマネージャー
//----------------------------------------------------------------------------
#pragma once

#include "dx11/gpu_common.h"
#include "dx11/gpu/gpu.h"
#include "shader_type.h"
#include "shader_types_fwd.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <memory>
#include <typeindex>

// 前方宣言
class ShaderProgram;
class GlobalShader;

//===========================================================================
//! シェーダーマネージャー（シングルトン）
//!
//! @code
//!   // 初期化
//!   D3DShaderCompiler compiler;
//!   ShaderManager::Get().Initialize(fs, &compiler);
//!
//!   // 個別シェーダーをロード
//!   auto vs = ShaderManager::Get().LoadVertexShader("shaders:/vs.hlsl");
//!   auto ps = ShaderManager::Get().LoadPixelShader("shaders:/ps.hlsl");
//!
//!   // または統一API
//!   auto vs = ShaderManager::Get().LoadShader("shaders:/vs.hlsl", ShaderType::Vertex);
//!
//!   // ShaderProgram作成
//!   auto program = ShaderManager::Get().CreateProgram("shaders:/vs.hlsl", "shaders:/ps.hlsl");
//!   program->Bind();
//!
//!   // GlobalShader使用
//!   auto* globalVs = ShaderManager::Get().GetGlobalShader<MyVertexShader>();
//!
//!   // InputLayout作成
//!   auto layout = ShaderManager::Get().CreateInputLayout(vs.get(), elements, count);
//!
//!   // 終了
//!   ShaderManager::Get().Shutdown();
//! @endcode
//===========================================================================
class ShaderManager final : private NonCopyableNonMovable
{
public:
    static ShaderManager& Get() noexcept;

    //----------------------------------------------------------
    //! @name   初期化・終了
    //----------------------------------------------------------
    //!@{

    void Initialize(
        IReadableFileSystem* fileSystem,
        IShaderCompiler* compiler,
        IShaderCache* bytecodeCache = nullptr,
        IShaderResourceCache* resourceCache = nullptr);

    void Shutdown();

    [[nodiscard]] bool IsInitialized() const noexcept { return initialized_; }

    //!@}
    //----------------------------------------------------------
    //! @name   シェーダーロード（統一API）
    //----------------------------------------------------------
    //!@{

    //! シェーダーをロード
    //! @param path ファイルパス（マウントパス）
    //! @param type シェーダータイプ
    //! @param defines マクロ定義
    //! @return シェーダー（失敗時nullptr）
    [[nodiscard]] ShaderPtr LoadShader(
        const std::string& path,
        ShaderType type,
        const std::vector<ShaderDefine>& defines = {});

    //!@}
    //----------------------------------------------------------
    //! @name   シェーダーロード（個別API）
    //----------------------------------------------------------
    //!@{

    [[nodiscard]] ShaderPtr LoadVertexShader(
        const std::string& path,
        const std::vector<ShaderDefine>& defines = {});

    [[nodiscard]] ShaderPtr LoadPixelShader(
        const std::string& path,
        const std::vector<ShaderDefine>& defines = {});

    [[nodiscard]] ShaderPtr LoadGeometryShader(
        const std::string& path,
        const std::vector<ShaderDefine>& defines = {});

    [[nodiscard]] ShaderPtr LoadHullShader(
        const std::string& path,
        const std::vector<ShaderDefine>& defines = {});

    [[nodiscard]] ShaderPtr LoadDomainShader(
        const std::string& path,
        const std::vector<ShaderDefine>& defines = {});

    [[nodiscard]] ShaderPtr LoadComputeShader(
        const std::string& path,
        const std::vector<ShaderDefine>& defines = {});

    //!@}
    //----------------------------------------------------------
    //! @name   ShaderProgram作成
    //----------------------------------------------------------
    //!@{

    //! VS/PSパスからプログラムを作成
    [[nodiscard]] std::unique_ptr<ShaderProgram> CreateProgram(
        const std::string& vsPath,
        const std::string& psPath);

    //! VS/PS/GSパスからプログラムを作成
    [[nodiscard]] std::unique_ptr<ShaderProgram> CreateProgram(
        const std::string& vsPath,
        const std::string& psPath,
        const std::string& gsPath);

    //! シェーダーオブジェクトからプログラムを作成
    [[nodiscard]] std::unique_ptr<ShaderProgram> CreateProgram(
        ShaderPtr vs,
        ShaderPtr ps,
        ShaderPtr gs = nullptr,
        ShaderPtr hs = nullptr,
        ShaderPtr ds = nullptr);

    //!@}
    //----------------------------------------------------------
    //! @name   GlobalShader
    //----------------------------------------------------------
    //!@{

    //! グローバルシェーダーを取得（遅延初期化）
    template<typename T>
    [[nodiscard]] T* GetGlobalShader() {
        static_assert(std::is_base_of_v<GlobalShader, T>, "T must derive from GlobalShader");

        auto typeIndex = std::type_index(typeid(T));
        auto it = globalShaders_.find(typeIndex);

        if (it != globalShaders_.end()) {
            return static_cast<T*>(it->second.get());
        }

        // 新規作成
        auto shader = std::make_unique<T>();
        T* ptr = shader.get();

        // シェーダーをロード
        auto loaded = LoadShader(
            shader->GetSourcePath(),
            shader->GetShaderType(),
            shader->GetDefines());

        if (loaded) {
            shader->SetShader(std::move(loaded));
            globalShaders_[typeIndex] = std::move(shader);
            return ptr;
        }

        return nullptr;
    }

    //!@}
    //----------------------------------------------------------
    //! @name   バイトコードコンパイル
    //----------------------------------------------------------
    //!@{

    //! バイトコードをコンパイル
    [[nodiscard]] ComPtr<ID3DBlob> CompileBytecode(
        const std::string& path,
        ShaderType type,
        const std::vector<ShaderDefine>& defines = {});

    //!@}
    //----------------------------------------------------------
    //! @name   InputLayout作成
    //----------------------------------------------------------
    //!@{

    //! 頂点シェーダーからInputLayoutを作成
    [[nodiscard]] ComPtr<ID3D11InputLayout> CreateInputLayout(
        Shader* vertexShader,
        const D3D11_INPUT_ELEMENT_DESC* elements,
        uint32_t numElements);

    //!@}
    //----------------------------------------------------------
    //! @name   キャッシュ管理
    //----------------------------------------------------------
    //!@{

    //! 全キャッシュをクリア
    void ClearCache();

    //! バイトコードキャッシュをクリア
    void ClearBytecodeCache();

    //! シェーダーリソースキャッシュをクリア
    void ClearResourceCache();

    //! グローバルシェーダーキャッシュをクリア
    void ClearGlobalShaderCache();

    //! キャッシュ統計を取得
    [[nodiscard]] ShaderCacheStats GetCacheStats() const;

    //!@}

private:
    ShaderManager() = default;
    ~ShaderManager() = default;

    //! シェーダーを作成
    [[nodiscard]] ShaderPtr CreateShaderFromBytecode(
        ComPtr<ID3DBlob> bytecode,
        ShaderType type);

    //! キャッシュキーを計算
    [[nodiscard]] uint64_t ComputeCacheKey(
        const std::string& path,
        ShaderType type,
        const std::vector<ShaderDefine>& defines) const;

    bool initialized_ = false;
    IReadableFileSystem* fileSystem_ = nullptr;
    IShaderCompiler* compiler_ = nullptr;
    IShaderCache* bytecodeCache_ = nullptr;

    // シェーダーリソースキャッシュ
    std::unique_ptr<IShaderResourceCache> ownedResourceCache_;  //!< 内部所有（外部指定なしの場合）
    IShaderResourceCache* resourceCache_ = nullptr;             //!< 使用するキャッシュ

    // グローバルシェーダーキャッシュ
    std::unordered_map<std::type_index, std::unique_ptr<GlobalShader>> globalShaders_;
};
