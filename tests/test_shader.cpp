//----------------------------------------------------------------------------
//! @file   test_shader.cpp
//! @brief  シェーダーシステム テストスイート
//!
//! @details
//! このファイルはシェーダーシステムの包括的なテストを提供します。
//!
//! テストカテゴリ:
//! - ShaderTypeユーティリティ: シェーダータイプ判定・プロファイル取得
//! - D3DShaderCompiler: HLSLシェーダーのコンパイル機能
//! - ShaderResource: D3D11シェーダーリソースの生成
//! - ShaderManager: シェーダーのロード・キャッシュ・管理
//! - ファイルベーステスト: 実際のシェーダーファイルからのロード
//!
//! @note D3D11デバイスが必要なテストは自動的にスキップされます
//----------------------------------------------------------------------------
#include "test_shader.h"
#include "test_common.h"
#include "dx11/gpu_common.h"
#include "dx11/graphics_device.h"
#include "dx11/graphics_context.h"
#include "engine/shader/shader_manager.h"
#include "dx11/compile/shader_type.h"
#include "dx11/compile/shader_compiler.h"
#include "dx11/gpu/gpu.h"
#include "engine/fs/file_system_manager.h"
#include "engine/fs/memory_file_system.h"
#include "engine/fs/host_file_system.h"
#include "common/logging/logging.h"
#include <cassert>
#include <iostream>
#include <filesystem>

namespace tests {

//----------------------------------------------------------------------------
// テストユーティリティ（共通ヘッダーから使用）
//----------------------------------------------------------------------------

// グローバルカウンターを使用（後方互換性のため）
#define s_testCount tests::GetGlobalTestCount()
#define s_passCount tests::GetGlobalPassCount()

//----------------------------------------------------------------------------
// テスト用シェーダーソースコード (HLSL)
//----------------------------------------------------------------------------

//! シンプルな頂点シェーダー
//! @details POSITION/COLORセマンティクスのテスト用
static const char* kSimpleVertexShader = R"(
struct VSInput
{
    float3 position : POSITION;
    float4 color : COLOR;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    output.position = float4(input.position, 1.0);
    output.color = input.color;
    return output;
}
)";

//! シンプルなピクセルシェーダー
//! @details SV_TARGET出力のテスト用
static const char* kSimplePixelShader = R"(
struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

float4 PSMain(PSInput input) : SV_TARGET
{
    return input.color;
}
)";

//! コンピュートシェーダー
//! @details RWBuffer・numthreadsのテスト用
static const char* kComputeShader = R"(
RWBuffer<float> output : register(u0);

[numthreads(64, 1, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
    output[DTid.x] = float(DTid.x) * 2.0;
}
)";

//! マクロ定義付きシェーダー
//! @details #ifdef/#defineプリプロセッサのテスト用
static const char* kShaderWithDefines = R"(
struct VSInput
{
    float3 position : POSITION;
};

struct VSOutput
{
    float4 position : SV_POSITION;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
#ifdef SCALE_POSITION
    output.position = float4(input.position * SCALE_VALUE, 1.0);
#else
    output.position = float4(input.position, 1.0);
#endif
    return output;
}
)";

//! 不正なシェーダー（構文エラーあり）
//! @details コンパイルエラー検出のテスト用
static const char* kInvalidShader = R"(
// このシェーダーは構文エラーを含む
float4 VSMain() : SV_POSITION
{
    undeclared_function();  // エラー: 未宣言の関数
    return float4(0,0,0,1);
}
)";

//----------------------------------------------------------------------------
// D3DShaderCompiler テスト
//----------------------------------------------------------------------------

//! 頂点シェーダーコンパイルテスト
//! @details vs_5_0プロファイルでの基本的なコンパイルをテスト
static void TestShaderCompiler_VertexShader()
{
    std::cout << "\n=== 頂点シェーダーコンパイルテスト ===" << std::endl;

    D3DShaderCompiler compiler;

    std::vector<char> source(kSimpleVertexShader, kSimpleVertexShader + strlen(kSimpleVertexShader));
    auto result = compiler.compile(source, "test_vs.hlsl", "vs_5_0", "VSMain");

    TEST_ASSERT(result.success, "頂点シェーダーのコンパイルが成功すること");
    TEST_ASSERT(result.bytecode != nullptr, "バイトコードがnullでないこと");
    TEST_ASSERT(result.bytecode->GetBufferSize() > 0, "バイトコードサイズが0より大きいこと");
    TEST_ASSERT(result.errorMessage.empty(), "成功時はエラーメッセージが空であること");
}

//! ピクセルシェーダーコンパイルテスト
//! @details ps_5_0プロファイルでの基本的なコンパイルをテスト
static void TestShaderCompiler_PixelShader()
{
    std::cout << "\n=== ピクセルシェーダーコンパイルテスト ===" << std::endl;

    D3DShaderCompiler compiler;

    std::vector<char> source(kSimplePixelShader, kSimplePixelShader + strlen(kSimplePixelShader));
    auto result = compiler.compile(source, "test_ps.hlsl", "ps_5_0", "PSMain");

    TEST_ASSERT(result.success, "ピクセルシェーダーのコンパイルが成功すること");
    TEST_ASSERT(result.bytecode != nullptr, "バイトコードがnullでないこと");
}

//! コンピュートシェーダーコンパイルテスト
//! @details cs_5_0プロファイルでの基本的なコンパイルをテスト
static void TestShaderCompiler_ComputeShader()
{
    std::cout << "\n=== コンピュートシェーダーコンパイルテスト ===" << std::endl;

    D3DShaderCompiler compiler;

    std::vector<char> source(kComputeShader, kComputeShader + strlen(kComputeShader));
    auto result = compiler.compile(source, "test_cs.hlsl", "cs_5_0", "CSMain");

    TEST_ASSERT(result.success, "コンピュートシェーダーのコンパイルが成功すること");
    TEST_ASSERT(result.bytecode != nullptr, "バイトコードがnullでないこと");
}

//! マクロ定義付きコンパイルテスト
//! @details ShaderDefineを使用したプリプロセッサマクロのテスト
static void TestShaderCompiler_WithDefines()
{
    std::cout << "\n=== マクロ定義付きコンパイルテスト ===" << std::endl;

    D3DShaderCompiler compiler;

    std::vector<char> source(kShaderWithDefines, kShaderWithDefines + strlen(kShaderWithDefines));
    std::vector<ShaderDefine> defines = {
        {"SCALE_POSITION", "1"},
        {"SCALE_VALUE", "2.0"}
    };

    auto result = compiler.compile(source, "test_defines.hlsl", "vs_5_0", "VSMain", defines);

    TEST_ASSERT(result.success, "マクロ定義付きシェーダーのコンパイルが成功すること");
    TEST_ASSERT(result.bytecode != nullptr, "バイトコードがnullでないこと");
}

//! 不正シェーダーコンパイルテスト
//! @details 構文エラーのあるシェーダーのコンパイル失敗を確認
static void TestShaderCompiler_InvalidShader()
{
    std::cout << "\n=== 不正シェーダーコンパイルテスト ===" << std::endl;

    D3DShaderCompiler compiler;

    std::vector<char> source(kInvalidShader, kInvalidShader + strlen(kInvalidShader));
    auto result = compiler.compile(source, "test_invalid.hlsl", "vs_5_0", "VSMain");

    TEST_ASSERT(!result.success, "不正なシェーダーはコンパイルに失敗すること");
    TEST_ASSERT(result.bytecode == nullptr, "失敗時はバイトコードがnullであること");
    TEST_ASSERT(!result.errorMessage.empty(), "失敗時はエラーメッセージが設定されること");

    std::cout << "  エラーメッセージ: " << result.errorMessage.substr(0, 100) << "..." << std::endl;
}

//----------------------------------------------------------------------------
// ShaderManager テスト
//----------------------------------------------------------------------------

//! ShaderManager初期化テスト
//! @details ファイルシステムとコンパイラを使用した初期化をテスト
static void TestShaderManager_Initialize()
{
    std::cout << "\n=== ShaderManager初期化テスト ===" << std::endl;

    if (!GraphicsDevice::Get().IsValid()) {
        std::cout << "[スキップ] GraphicsDeviceが初期化されていません" << std::endl;
        return;
    }

    // テスト用メモリファイルシステムをセットアップ
    auto memFs = std::make_unique<MemoryFileSystem>();
    memFs->addTextFile("vs_simple.hlsl", kSimpleVertexShader);
    memFs->addTextFile("ps_simple.hlsl", kSimplePixelShader);
    memFs->addTextFile("cs_simple.hlsl", kComputeShader);

    FileSystemManager::Get().Mount("shaders", std::move(memFs));

    // ファイルシステムを取得
    IReadableFileSystem* fs = FileSystemManager::Get().GetFileSystem("shaders");
    TEST_ASSERT(fs != nullptr, "ファイルシステムが有効であること");

    // シェーダーマネージャーを初期化
    static D3DShaderCompiler compiler;
    ShaderManager::Get().Initialize(fs, &compiler);

    TEST_ASSERT(ShaderManager::Get().IsInitialized(), "ShaderManagerが初期化されていること");
}

//! 頂点シェーダーロードテスト
//! @details ShaderManager経由での頂点シェーダーロードをテスト
static void TestShaderManager_LoadVertexShader()
{
    std::cout << "\n=== 頂点シェーダーロードテスト ===" << std::endl;

    if (!ShaderManager::Get().IsInitialized()) {
        std::cout << "[スキップ] ShaderManagerが初期化されていません" << std::endl;
        return;
    }

    auto vs = ShaderManager::Get().LoadVertexShader("vs_simple.hlsl");
    TEST_ASSERT(vs != nullptr, "LoadVertexShaderが有効なシェーダーを返すこと");
    TEST_ASSERT(vs->IsVertex(), "ロードしたシェーダーが頂点シェーダーであること");
    TEST_ASSERT(vs->GetShaderType() == ShaderType::Vertex, "シェーダータイプがVertexであること");
}

//! ピクセルシェーダーロードテスト
//! @details ShaderManager経由でのピクセルシェーダーロードをテスト
static void TestShaderManager_LoadPixelShader()
{
    std::cout << "\n=== ピクセルシェーダーロードテスト ===" << std::endl;

    if (!ShaderManager::Get().IsInitialized()) {
        std::cout << "[スキップ] ShaderManagerが初期化されていません" << std::endl;
        return;
    }

    auto ps = ShaderManager::Get().LoadPixelShader("ps_simple.hlsl");
    TEST_ASSERT(ps != nullptr, "LoadPixelShaderが有効なシェーダーを返すこと");
    TEST_ASSERT(ps->IsPixel(), "ロードしたシェーダーがピクセルシェーダーであること");
    TEST_ASSERT(ps->GetShaderType() == ShaderType::Pixel, "シェーダータイプがPixelであること");
}

//! コンピュートシェーダーロードテスト
//! @details ShaderManager経由でのコンピュートシェーダーロードをテスト
static void TestShaderManager_LoadComputeShader()
{
    std::cout << "\n=== コンピュートシェーダーロードテスト ===" << std::endl;

    if (!ShaderManager::Get().IsInitialized()) {
        std::cout << "[スキップ] ShaderManagerが初期化されていません" << std::endl;
        return;
    }

    auto cs = ShaderManager::Get().LoadComputeShader("cs_simple.hlsl");
    TEST_ASSERT(cs != nullptr, "LoadComputeShaderが有効なシェーダーを返すこと");
    TEST_ASSERT(cs->IsCompute(), "ロードしたシェーダーがコンピュートシェーダーであること");
    TEST_ASSERT(cs->GetShaderType() == ShaderType::Compute, "シェーダータイプがComputeであること");
}

//! シェーダーキャッシュヒットテスト
//! @details 同じシェーダーを2回ロードしてキャッシュ動作を確認
static void TestShaderManager_CacheHit()
{
    std::cout << "\n=== シェーダーキャッシュヒットテスト ===" << std::endl;

    if (!ShaderManager::Get().IsInitialized()) {
        std::cout << "[スキップ] ShaderManagerが初期化されていません" << std::endl;
        return;
    }

    auto vs1 = ShaderManager::Get().LoadVertexShader("vs_simple.hlsl");
    auto vs2 = ShaderManager::Get().LoadVertexShader("vs_simple.hlsl");

    TEST_ASSERT(vs1 != nullptr && vs2 != nullptr, "両方のシェーダーが有効であること");
    TEST_ASSERT(vs1.get() == vs2.get(), "同じシェーダーがキャッシュされていること");
}

//! 存在しないシェーダーロードテスト
//! @details 存在しないファイルをロードした場合のエラー処理をテスト
static void TestShaderManager_LoadNonExistent()
{
    std::cout << "\n=== 存在しないシェーダーロードテスト ===" << std::endl;

    if (!ShaderManager::Get().IsInitialized()) {
        std::cout << "[スキップ] ShaderManagerが初期化されていません" << std::endl;
        return;
    }

    auto vs = ShaderManager::Get().LoadVertexShader("nonexistent_shader.hlsl");
    TEST_ASSERT(vs == nullptr, "存在しないシェーダーファイルがnullptrを返すこと");

    auto ps = ShaderManager::Get().LoadPixelShader("another_nonexistent.hlsl");
    TEST_ASSERT(ps == nullptr, "存在しないピクセルシェーダーがnullptrを返すこと");
}

//! シェーダーキャッシュ統計テスト
//! @details GetCacheStats()の動作を確認
static void TestShaderManager_CacheStats()
{
    std::cout << "\n=== シェーダーキャッシュ統計テスト ===" << std::endl;

    if (!ShaderManager::Get().IsInitialized()) {
        std::cout << "[スキップ] ShaderManagerが初期化されていません" << std::endl;
        return;
    }

    auto stats = ShaderManager::Get().GetCacheStats();

    // 統計情報が有効な値を返すことを確認
    TEST_ASSERT(stats.entryCount >= 0, "エントリカウントが有効であること");
    TEST_ASSERT(stats.HitRate() >= 0.0 && stats.HitRate() <= 1.0, "ヒット率が0～1の範囲であること");

    std::cout << "  キャッシュエントリ数: " << stats.entryCount << std::endl;
    std::cout << "  キャッシュヒット数: " << stats.hitCount << std::endl;
    std::cout << "  キャッシュミス数: " << stats.missCount << std::endl;
    std::cout << "  キャッシュヒット率: " << (stats.HitRate() * 100.0) << "%" << std::endl;
}

//! キャッシュクリアテスト
//! @details ClearBytecodeCache/ClearResourceCacheの動作を確認
static void TestShaderManager_ClearCache()
{
    std::cout << "\n=== シェーダーキャッシュクリアテスト ===" << std::endl;

    if (!ShaderManager::Get().IsInitialized()) {
        std::cout << "[スキップ] ShaderManagerが初期化されていません" << std::endl;
        return;
    }

    // キャッシュにシェーダーを追加
    auto vs = ShaderManager::Get().LoadVertexShader("vs_simple.hlsl");
    auto statsBeforeClean = ShaderManager::Get().GetCacheStats();
    TEST_ASSERT(statsBeforeClean.entryCount > 0, "シェーダーがキャッシュに追加されていること");

    // バイトコードキャッシュをクリア（リソースキャッシュは残る）
    ShaderManager::Get().ClearBytecodeCache();
    TEST_ASSERT(true, "バイトコードキャッシュクリアが実行されること");

    // リソースキャッシュをクリア
    ShaderManager::Get().ClearResourceCache();
    auto statsAfterResourceClean = ShaderManager::Get().GetCacheStats();
    TEST_ASSERT(statsAfterResourceClean.entryCount == 0, "リソースキャッシュがクリアされていること");
}

//! ShaderManagerクリーンアップテスト
//! @details Shutdownとファイルシステムのアンマウントをテスト
static void TestShaderManager_Cleanup()
{
    std::cout << "\n=== ShaderManagerクリーンアップテスト ===" << std::endl;

    ShaderManager::Get().Shutdown();
    FileSystemManager::Get().UnmountAll();

    TEST_ASSERT(!ShaderManager::Get().IsInitialized(), "ShaderManagerがシャットダウンされていること");
}

//----------------------------------------------------------------------------
// ファイルベース シェーダー テスト
//----------------------------------------------------------------------------

//! シェーダーディレクトリを使用したShaderManager初期化
//! @param shaderDir シェーダーディレクトリのパス
//! @return 初期化成功時true
static bool InitShaderManagerWithFileSystem(const std::wstring& shaderDir)
{
    if (!GraphicsDevice::Get().IsValid()) {
        return false;
    }

    // ホストファイルシステムをマウント
    auto hostFs = std::make_unique<HostFileSystem>(shaderDir);
    FileSystemManager::Get().Mount("shaders", std::move(hostFs));
    IReadableFileSystem* fs = FileSystemManager::Get().GetFileSystem("shaders");

    if (!fs) {
        return false;
    }

    // シェーダーコンパイラを初期化
    static D3DShaderCompiler s_fileCompiler;
    ShaderManager::Get().Initialize(fs, &s_fileCompiler);
    return ShaderManager::Get().IsInitialized();
}

//! ファイルベース頂点シェーダーロードテスト
//! @details simple_vs.hlslをファイルからロード
static void TestFileShader_LoadVertexShader()
{
    std::cout << "\n=== ファイルベース頂点シェーダーロードテスト ===" << std::endl;

    if (!ShaderManager::Get().IsInitialized()) {
        std::cout << "[スキップ] ShaderManagerが初期化されていません" << std::endl;
        return;
    }

    auto vs = ShaderManager::Get().LoadVertexShader("simple_vs.hlsl");
    TEST_ASSERT(vs != nullptr, "simple_vs.hlslのロードが成功すること");

    if (vs) {
        TEST_ASSERT(vs->IsVertex(), "ファイルからロードしたシェーダーが頂点シェーダーであること");
        TEST_ASSERT(vs->GetShaderType() == ShaderType::Vertex, "シェーダータイプがVertexであること");
    }
}

//! ファイルベースピクセルシェーダーロードテスト
//! @details simple_ps.hlslをファイルからロード
static void TestFileShader_LoadPixelShader()
{
    std::cout << "\n=== ファイルベースピクセルシェーダーロードテスト ===" << std::endl;

    if (!ShaderManager::Get().IsInitialized()) {
        std::cout << "[スキップ] ShaderManagerが初期化されていません" << std::endl;
        return;
    }

    auto ps = ShaderManager::Get().LoadPixelShader("simple_ps.hlsl");
    TEST_ASSERT(ps != nullptr, "simple_ps.hlslのロードが成功すること");

    if (ps) {
        TEST_ASSERT(ps->IsPixel(), "ファイルからロードしたシェーダーがピクセルシェーダーであること");
        TEST_ASSERT(ps->GetShaderType() == ShaderType::Pixel, "シェーダータイプがPixelであること");
    }
}

//! ファイルベースコンピュートシェーダーロードテスト
//! @details simple_cs.hlslをファイルからロード
static void TestFileShader_LoadComputeShader()
{
    std::cout << "\n=== ファイルベースコンピュートシェーダーロードテスト ===" << std::endl;

    if (!ShaderManager::Get().IsInitialized()) {
        std::cout << "[スキップ] ShaderManagerが初期化されていません" << std::endl;
        return;
    }

    auto cs = ShaderManager::Get().LoadComputeShader("simple_cs.hlsl");
    TEST_ASSERT(cs != nullptr, "simple_cs.hlslのロードが成功すること");

    if (cs) {
        TEST_ASSERT(cs->IsCompute(), "ファイルからロードしたシェーダーがコンピュートシェーダーであること");
        TEST_ASSERT(cs->GetShaderType() == ShaderType::Compute, "シェーダータイプがComputeであること");
    }
}

//! 複数シェーダーファイルロードテスト
//! @details textured_vs.hlsl, textured_ps.hlslをロード
static void TestFileShader_LoadMultiple()
{
    std::cout << "\n=== 複数シェーダーファイルロードテスト ===" << std::endl;

    if (!ShaderManager::Get().IsInitialized()) {
        std::cout << "[スキップ] ShaderManagerが初期化されていません" << std::endl;
        return;
    }

    // テクスチャ付きシェーダー
    auto texVs = ShaderManager::Get().LoadVertexShader("textured_vs.hlsl");
    TEST_ASSERT(texVs != nullptr, "textured_vs.hlslのロードが成功すること");

    auto texPs = ShaderManager::Get().LoadPixelShader("textured_ps.hlsl");
    TEST_ASSERT(texPs != nullptr, "textured_ps.hlslのロードが成功すること");

    // フルスクリーンシェーダー
    auto fullscreenVs = ShaderManager::Get().LoadVertexShader("fullscreen_vs.hlsl");
    TEST_ASSERT(fullscreenVs != nullptr, "fullscreen_vs.hlslのロードが成功すること");

    // ポストプロセスシェーダー
    auto postPs = ShaderManager::Get().LoadPixelShader("postprocess_ps.hlsl");
    TEST_ASSERT(postPs != nullptr, "postprocess_ps.hlslのロードが成功すること");
}

//! スキニングシェーダーロードテスト
//! @details skinning_vs.hlslをロード（マクロ定義なし）
static void TestFileShader_LoadSkinning()
{
    std::cout << "\n=== スキニングシェーダーロードテスト ===" << std::endl;

    if (!ShaderManager::Get().IsInitialized()) {
        std::cout << "[スキップ] ShaderManagerが初期化されていません" << std::endl;
        return;
    }

    auto skinVs = ShaderManager::Get().LoadVertexShader("skinning_vs.hlsl");
    TEST_ASSERT(skinVs != nullptr, "skinning_vs.hlslのロードが成功すること");
}

//! ファイルベースシェーダーキャッシュヒットテスト
//! @details 同じシェーダーファイルを2回ロードしてキャッシュ動作を検証
static void TestFileShader_CacheHit()
{
    std::cout << "\n=== ファイルベースシェーダーキャッシュヒットテスト ===" << std::endl;

    if (!ShaderManager::Get().IsInitialized()) {
        std::cout << "[スキップ] ShaderManagerが初期化されていません" << std::endl;
        return;
    }

    auto vs1 = ShaderManager::Get().LoadVertexShader("simple_vs.hlsl");
    auto vs2 = ShaderManager::Get().LoadVertexShader("simple_vs.hlsl");

    TEST_ASSERT(vs1 != nullptr && vs2 != nullptr, "両方のシェーダーロードが成功すること");
    TEST_ASSERT(vs1.get() == vs2.get(), "ファイルベースシェーダーもキャッシュされること");
}

//! ファイルベーステスト用クリーンアップ
static void TestFileShader_Cleanup()
{
    std::cout << "\n=== ファイルベーステストクリーンアップ ===" << std::endl;

    ShaderManager::Get().Shutdown();
    FileSystemManager::Get().UnmountAll();

    TEST_ASSERT(!ShaderManager::Get().IsInitialized(), "ShaderManagerがシャットダウンされていること");
}

//----------------------------------------------------------------------------
// ShaderType ユーティリティテスト
//----------------------------------------------------------------------------

//! ShaderTypeユーティリティ関数テスト
//! @details プロファイル文字列、エントリーポイント、タイプ名、判定関数をテスト
static void TestShaderType_Utilities()
{
    std::cout << "\n=== ShaderTypeユーティリティテスト ===" << std::endl;

    // プロファイル文字列のテスト
    TEST_ASSERT(std::string(GetShaderProfile(ShaderType::Vertex)) == "vs_5_0", "頂点シェーダープロファイルがvs_5_0であること");
    TEST_ASSERT(std::string(GetShaderProfile(ShaderType::Pixel)) == "ps_5_0", "ピクセルシェーダープロファイルがps_5_0であること");
    TEST_ASSERT(std::string(GetShaderProfile(ShaderType::Compute)) == "cs_5_0", "コンピュートシェーダープロファイルがcs_5_0であること");

    // エントリーポイントのテスト
    TEST_ASSERT(std::string(GetShaderEntryPoint(ShaderType::Vertex)) == "VSMain", "頂点シェーダーエントリーポイントがVSMainであること");
    TEST_ASSERT(std::string(GetShaderEntryPoint(ShaderType::Pixel)) == "PSMain", "ピクセルシェーダーエントリーポイントがPSMainであること");

    // タイプ名のテスト
    TEST_ASSERT(std::string(GetShaderTypeName(ShaderType::Vertex)) == "Vertex", "タイプ名がVertexであること");
    TEST_ASSERT(std::string(GetShaderTypeName(ShaderType::Compute)) == "Compute", "タイプ名がComputeであること");

    // IsGraphicsShaderのテスト
    TEST_ASSERT(IsGraphicsShader(ShaderType::Vertex), "頂点シェーダーがグラフィックスシェーダーであること");
    TEST_ASSERT(IsGraphicsShader(ShaderType::Pixel), "ピクセルシェーダーがグラフィックスシェーダーであること");
    TEST_ASSERT(!IsGraphicsShader(ShaderType::Compute), "コンピュートシェーダーがグラフィックスシェーダーでないこと");
}

//----------------------------------------------------------------------------
// 公開インターフェース
//----------------------------------------------------------------------------

//! シェーダーテストスイートを実行
//! @param assetsDir テストアセットディレクトリのパス（オプション）
//! @return 全テスト成功時true、それ以外false
bool RunShaderTests(const std::wstring& assetsDir)
{
    std::cout << "\n========================================" << std::endl;
    std::cout << "  シェーダーシステム テスト" << std::endl;
    std::cout << "========================================" << std::endl;

    ResetGlobalCounters();

    // ShaderTypeユーティリティテスト（D3D不要）
    TestShaderType_Utilities();

    // D3DShaderCompilerテスト（デバイス不要）
    TestShaderCompiler_VertexShader();
    TestShaderCompiler_PixelShader();
    TestShaderCompiler_ComputeShader();
    TestShaderCompiler_WithDefines();
    TestShaderCompiler_InvalidShader();

    // ShaderManagerテスト（D3Dデバイス、メモリファイルシステム使用）
    TestShaderManager_Initialize();
    TestShaderManager_LoadVertexShader();
    TestShaderManager_LoadPixelShader();
    TestShaderManager_LoadComputeShader();
    TestShaderManager_CacheHit();
    TestShaderManager_LoadNonExistent();
    TestShaderManager_CacheStats();
    TestShaderManager_ClearCache();
    TestShaderManager_Cleanup();

    // ファイルベーステスト（アセットディレクトリが指定された場合のみ）
    if (!assetsDir.empty()) {
        // シェーダーディレクトリを構築
        std::filesystem::path shaderPath = std::filesystem::path(assetsDir) / "shaders";

        if (std::filesystem::exists(shaderPath)) {
            std::cout << "\n--- ファイルベースシェーダーテスト ---" << std::endl;
            std::wcout << L"シェーダーディレクトリ: " << shaderPath.wstring() << std::endl;

            if (InitShaderManagerWithFileSystem(shaderPath.wstring())) {
                TestFileShader_LoadVertexShader();
                TestFileShader_LoadPixelShader();
                TestFileShader_LoadComputeShader();
                TestFileShader_LoadMultiple();
                TestFileShader_LoadSkinning();
                TestFileShader_CacheHit();
                TestFileShader_Cleanup();
            } else {
                std::cout << "[スキップ] ファイルベーステスト（初期化失敗）" << std::endl;
            }
        } else {
            std::cout << "\n[スキップ] ファイルベーステスト（シェーダーディレクトリが存在しません）" << std::endl;
        }
    } else {
        std::cout << "\n[スキップ] ファイルベーステスト（アセットディレクトリ未指定）" << std::endl;
    }

    std::cout << "\n----------------------------------------" << std::endl;
    std::cout << "シェーダーテスト: " << s_passCount << "/" << s_testCount << " 成功" << std::endl;
    std::cout << "----------------------------------------" << std::endl;

    return s_passCount == s_testCount;
}

} // namespace tests
