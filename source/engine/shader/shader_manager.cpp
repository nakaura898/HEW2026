//----------------------------------------------------------------------------
//! @file   shader_manager.cpp
//! @brief  シェーダーマネージャー実装
//----------------------------------------------------------------------------
#include "shader_manager.h"
#include "shader_program.h"
#include "global_shader.h"
#include "dx11/compile/shader_compiler.h"
#include "dx11/compile/shader_cache.h"
#include "engine/fs/file_system.h"
#include "dx11/graphics_device.h"
#include "common/logging/logging.h"
#include "common/utility/hash.h"

namespace
{
    //! シェーダーをバイトコードから作成
    [[nodiscard]] ShaderPtr CreateShaderFromBytecodeImpl(
        ComPtr<ID3DBlob> bytecode,
        ShaderType type)
    {
        if (!bytecode) return nullptr;

        ID3D11Device* device = GetD3D11Device();
        if (!device) return nullptr;

        ComPtr<ID3D11DeviceChild> shaderObj;
        HRESULT hr = E_FAIL;

        switch (type) {
        case ShaderType::Vertex:
            {
                ComPtr<ID3D11VertexShader> vs;
                hr = device->CreateVertexShader(
                    bytecode->GetBufferPointer(),
                    bytecode->GetBufferSize(),
                    nullptr, &vs);
                if (SUCCEEDED(hr)) {
                    hr = vs.As(&shaderObj);
                }
            }
            break;

        case ShaderType::Pixel:
            {
                ComPtr<ID3D11PixelShader> ps;
                hr = device->CreatePixelShader(
                    bytecode->GetBufferPointer(),
                    bytecode->GetBufferSize(),
                    nullptr, &ps);
                if (SUCCEEDED(hr)) {
                    hr = ps.As(&shaderObj);
                }
            }
            break;

        case ShaderType::Geometry:
            {
                ComPtr<ID3D11GeometryShader> gs;
                hr = device->CreateGeometryShader(
                    bytecode->GetBufferPointer(),
                    bytecode->GetBufferSize(),
                    nullptr, &gs);
                if (SUCCEEDED(hr)) {
                    hr = gs.As(&shaderObj);
                }
            }
            break;

        case ShaderType::Hull:
            {
                ComPtr<ID3D11HullShader> hs;
                hr = device->CreateHullShader(
                    bytecode->GetBufferPointer(),
                    bytecode->GetBufferSize(),
                    nullptr, &hs);
                if (SUCCEEDED(hr)) {
                    hr = hs.As(&shaderObj);
                }
            }
            break;

        case ShaderType::Domain:
            {
                ComPtr<ID3D11DomainShader> ds;
                hr = device->CreateDomainShader(
                    bytecode->GetBufferPointer(),
                    bytecode->GetBufferSize(),
                    nullptr, &ds);
                if (SUCCEEDED(hr)) {
                    hr = ds.As(&shaderObj);
                }
            }
            break;

        case ShaderType::Compute:
            {
                ComPtr<ID3D11ComputeShader> cs;
                hr = device->CreateComputeShader(
                    bytecode->GetBufferPointer(),
                    bytecode->GetBufferSize(),
                    nullptr, &cs);
                if (SUCCEEDED(hr)) {
                    hr = cs.As(&shaderObj);
                }
            }
            break;

        default:
            LOG_ERROR("[ShaderManager] 不明なシェーダータイプ");
            return nullptr;
        }

        RETURN_NULL_IF_FAILED(hr, "[ShaderManager] シェーダー作成失敗");

        // 頂点シェーダーの場合はバイトコードを保持（InputLayout作成用）
        ComPtr<ID3DBlob> keepBytecode = (type == ShaderType::Vertex) ? bytecode : nullptr;
        return std::make_shared<Shader>(std::move(shaderObj), std::move(keepBytecode));
    }
} // namespace

//============================================================================
// ShaderManager 実装
//============================================================================
ShaderManager& ShaderManager::Get() noexcept
{
    static ShaderManager instance;
    return instance;
}

void ShaderManager::Initialize(
    IReadableFileSystem* fileSystem,
    IShaderCompiler* compiler,
    IShaderCache* bytecodeCache,
    IShaderResourceCache* resourceCache)
{
    if (!GetD3D11Device()) {
        LOG_ERROR("[ShaderManager] D3D11Deviceが初期化されていません");
        return;
    }

    if (!fileSystem) {
        LOG_ERROR("[ShaderManager] FileSystemがnullです");
        return;
    }

    if (!compiler) {
        LOG_ERROR("[ShaderManager] ShaderCompilerがnullです");
        return;
    }

    initialized_ = true;
    fileSystem_ = fileSystem;
    compiler_ = compiler;
    bytecodeCache_ = bytecodeCache;

    // リソースキャッシュ：外部指定があればそれを使う、なければデフォルト作成
    if (resourceCache) {
        resourceCache_ = resourceCache;
    } else {
        ownedResourceCache_ = std::make_unique<ShaderResourceCache>();
        resourceCache_ = ownedResourceCache_.get();
    }
}

void ShaderManager::Shutdown()
{
    globalShaders_.clear();
    if (resourceCache_) {
        resourceCache_->Clear();
    }
    ownedResourceCache_.reset();
    initialized_ = false;
    fileSystem_ = nullptr;
    compiler_ = nullptr;
    bytecodeCache_ = nullptr;
    resourceCache_ = nullptr;
}

//----------------------------------------------------------------------------
// シェーダーロード（統一API）
//----------------------------------------------------------------------------
ShaderPtr ShaderManager::LoadShader(
    const std::string& path,
    ShaderType type,
    const std::vector<ShaderDefine>& defines)
{
    if (!initialized_) {
        LOG_ERROR("[ShaderManager] 初期化されていません");
        return nullptr;
    }

    // キャッシュキー計算
    uint64_t key = ComputeCacheKey(path, type, defines);

    // リソースキャッシュ検索
    if (auto cached = resourceCache_->Get(key)) {
        return cached;
    }

    // バイトコードコンパイル
    auto bytecode = CompileBytecode(path, type, defines);
    if (!bytecode) return nullptr;

    // シェーダー作成
    auto shader = CreateShaderFromBytecode(std::move(bytecode), type);
    if (!shader) return nullptr;

    // キャッシュ登録
    resourceCache_->Put(key, shader);

    return shader;
}

//----------------------------------------------------------------------------
// シェーダーロード（個別API）
//----------------------------------------------------------------------------
ShaderPtr ShaderManager::LoadVertexShader(
    const std::string& path,
    const std::vector<ShaderDefine>& defines)
{
    return LoadShader(path, ShaderType::Vertex, defines);
}

ShaderPtr ShaderManager::LoadPixelShader(
    const std::string& path,
    const std::vector<ShaderDefine>& defines)
{
    return LoadShader(path, ShaderType::Pixel, defines);
}

ShaderPtr ShaderManager::LoadGeometryShader(
    const std::string& path,
    const std::vector<ShaderDefine>& defines)
{
    return LoadShader(path, ShaderType::Geometry, defines);
}

ShaderPtr ShaderManager::LoadHullShader(
    const std::string& path,
    const std::vector<ShaderDefine>& defines)
{
    return LoadShader(path, ShaderType::Hull, defines);
}

ShaderPtr ShaderManager::LoadDomainShader(
    const std::string& path,
    const std::vector<ShaderDefine>& defines)
{
    return LoadShader(path, ShaderType::Domain, defines);
}

ShaderPtr ShaderManager::LoadComputeShader(
    const std::string& path,
    const std::vector<ShaderDefine>& defines)
{
    return LoadShader(path, ShaderType::Compute, defines);
}

//----------------------------------------------------------------------------
// ShaderProgram作成
//----------------------------------------------------------------------------
std::unique_ptr<ShaderProgram> ShaderManager::CreateProgram(
    const std::string& vsPath,
    const std::string& psPath)
{
    auto vs = LoadVertexShader(vsPath);
    if (!vs) return nullptr;

    auto ps = LoadPixelShader(psPath);
    if (!ps) return nullptr;

    return ShaderProgram::Create(std::move(vs), std::move(ps));
}

std::unique_ptr<ShaderProgram> ShaderManager::CreateProgram(
    const std::string& vsPath,
    const std::string& psPath,
    const std::string& gsPath)
{
    auto vs = LoadVertexShader(vsPath);
    if (!vs) return nullptr;

    auto ps = LoadPixelShader(psPath);
    if (!ps) return nullptr;

    auto gs = LoadGeometryShader(gsPath);
    if (!gs) return nullptr;

    return ShaderProgram::Create(std::move(vs), std::move(ps), std::move(gs));
}

std::unique_ptr<ShaderProgram> ShaderManager::CreateProgram(
    ShaderPtr vs,
    ShaderPtr ps,
    ShaderPtr gs,
    ShaderPtr hs,
    ShaderPtr ds)
{
    return ShaderProgram::Create(
        std::move(vs), std::move(ps), std::move(gs),
        std::move(hs), std::move(ds));
}

//----------------------------------------------------------------------------
// バイトコードコンパイル
//----------------------------------------------------------------------------
ComPtr<ID3DBlob> ShaderManager::CompileBytecode(
    const std::string& path,
    ShaderType type,
    const std::vector<ShaderDefine>& defines)
{
    const char* profile = GetShaderProfile(type);
    if (!profile) {
        LOG_ERROR("[ShaderManager] シェーダータイプが無効です");
        return nullptr;
    }

    if (!fileSystem_ || !compiler_) {
        LOG_ERROR("[ShaderManager] 初期化されていません");
        return nullptr;
    }

    // キャッシュキー生成
    uint64_t key = ComputeCacheKey(path, type, defines);

    // バイトコードキャッシュヒット確認
    if (bytecodeCache_) {
        if (auto* cached = bytecodeCache_->find(key)) {
            return cached;
        }
    }

    // ファイル読み込み
    auto source = fileSystem_->readAsChars(path);
    if (source.empty()) {
        LOG_ERROR("[ShaderManager] ファイルの読み込みに失敗しました: " + path);
        return nullptr;
    }

    // コンパイル
    const char* entryPoint = GetShaderEntryPoint(type);
    auto compileResult = compiler_->compile(source, path, profile, entryPoint, defines);

    if (!compileResult.success) {
        LOG_ERROR("[ShaderManager] コンパイルエラー (" + path + "):\n" + compileResult.errorMessage);
        return nullptr;
    }

    if (!compileResult.warningMessage.empty()) {
        LOG_WARN("[ShaderManager] 警告 (" + path + "):\n" + compileResult.warningMessage);
    }

    // バイトコードキャッシュに保存
    if (bytecodeCache_) {
        bytecodeCache_->store(key, compileResult.bytecode.Get());
    }

    return compileResult.bytecode;
}

//----------------------------------------------------------------------------
// InputLayout作成
//----------------------------------------------------------------------------
ComPtr<ID3D11InputLayout> ShaderManager::CreateInputLayout(
    Shader* vertexShader,
    const D3D11_INPUT_ELEMENT_DESC* elements,
    uint32_t numElements)
{
    if (!vertexShader || !vertexShader->HasBytecode()) {
        LOG_ERROR("[ShaderManager] 頂点シェーダーまたはバイトコードが無効です");
        return nullptr;
    }

    if (!elements || numElements == 0) {
        LOG_ERROR("[ShaderManager] 入力要素が無効です");
        return nullptr;
    }

    ID3D11Device* device = GetD3D11Device();
    if (!device) return nullptr;

    ComPtr<ID3D11InputLayout> layout;
    HRESULT hr = device->CreateInputLayout(
        elements,
        numElements,
        vertexShader->Bytecode(),
        vertexShader->BytecodeSize(),
        &layout);

    RETURN_NULL_IF_FAILED(hr, "[ShaderManager] InputLayout作成失敗");

    return layout;
}

//----------------------------------------------------------------------------
// キャッシュ管理
//----------------------------------------------------------------------------
void ShaderManager::ClearCache()
{
    ClearBytecodeCache();
    ClearResourceCache();
    ClearGlobalShaderCache();
}

void ShaderManager::ClearBytecodeCache()
{
    if (bytecodeCache_) {
        bytecodeCache_->clear();
    }
}

void ShaderManager::ClearResourceCache()
{
    resourceCache_->Clear();
}

void ShaderManager::ClearGlobalShaderCache()
{
    globalShaders_.clear();
}

ShaderCacheStats ShaderManager::GetCacheStats() const
{
    return resourceCache_->GetStats();
}

//----------------------------------------------------------------------------
// 内部関数
//----------------------------------------------------------------------------
ShaderPtr ShaderManager::CreateShaderFromBytecode(
    ComPtr<ID3DBlob> bytecode,
    ShaderType type)
{
    return CreateShaderFromBytecodeImpl(std::move(bytecode), type);
}

uint64_t ShaderManager::ComputeCacheKey(
    const std::string& path,
    ShaderType type,
    const std::vector<ShaderDefine>& defines) const
{
    uint64_t hash = HashUtil::Fnv1aString(path);
    hash = HashUtil::Fnv1aString(GetShaderProfile(type), hash);

    for (const auto& def : defines) {
        hash = HashUtil::Fnv1aString(def.name, hash);
        hash = HashUtil::Fnv1aString(def.value, hash);
    }

    return hash;
}
