//----------------------------------------------------------------------------
//! @file   test_texture.cpp
//! @brief  テクスチャシステム テストスイート
//!
//! @details
//! このファイルはテクスチャシステムの包括的なテストを提供します。
//!
//! テストカテゴリ:
//! - TextureManager: テクスチャ作成・ロード・キャッシュ・管理
//! - Texture: GPU上のテクスチャリソース
//! - ファイルベーステスト: PNG/DDSファイルの読み込み
//!
//! @note D3D11デバイスが必要なテストは自動的にスキップされます
//----------------------------------------------------------------------------
#include "test_texture.h"
#include "test_common.h"
#include "dx11/gpu_common.h"
#include "dx11/graphics_device.h"
#include "dx11/graphics_context.h"
#include "dx11/gpu/gpu.h"
#include "engine/texture/texture_manager.h"
#include "engine/fs/file_system_manager.h"
#include "engine/fs/host_file_system.h"
#include "engine/fs/memory_file_system.h"
#include "common/logging/logging.h"
#include <cassert>
#include <iostream>
#include <cmath>

namespace tests {

//----------------------------------------------------------------------------
// テストユーティリティ（共通ヘッダーから使用）
//----------------------------------------------------------------------------

// グローバルカウンターを使用（後方互換性のため）
#define s_testCount tests::GetGlobalTestCount()
#define s_passCount tests::GetGlobalPassCount()

//----------------------------------------------------------------------------
// テスト用画像データ生成
//----------------------------------------------------------------------------

//! 4x4ピクセルのRGBAテストパターンを生成
//! @details 16ピクセル、64バイトのグラデーションパターン
//! @return テストパターンのバイトデータ
static std::vector<std::byte> GenerateTestPattern4x4()
{
    std::vector<std::byte> data(4 * 4 * 4); // 4x4ピクセル、1ピクセル4バイト

    for (int y = 0; y < 4; y++) {
        for (int x = 0; x < 4; x++) {
            int idx = (y * 4 + x) * 4;
            data[idx + 0] = std::byte(x * 64);        // R: 水平グラデーション
            data[idx + 1] = std::byte(y * 64);        // G: 垂直グラデーション
            data[idx + 2] = std::byte((x + y) * 32);  // B: 斜めグラデーション
            data[idx + 3] = std::byte(255);           // A: 不透明
        }
    }

    return data;
}

//----------------------------------------------------------------------------
// TextureManager テスト
//----------------------------------------------------------------------------

//! TextureManager初期化テスト
//! @details ファイルシステムを使用した初期化をテスト
static void TestTextureManager_Initialize()
{
    std::cout << "\n=== TextureManager初期化テスト ===" << std::endl;

    if (!GraphicsDevice::Get().IsValid()) {
        std::cout << "[スキップ] GraphicsDeviceが初期化されていません" << std::endl;
        return;
    }

    // テストテクスチャ用メモリファイルシステムを作成
    auto memFs = std::make_unique<MemoryFileSystem>();

    FileSystemManager::Get().Mount("textures", std::move(memFs));
    IReadableFileSystem* fs = FileSystemManager::Get().GetFileSystem("textures");

    TEST_ASSERT(fs != nullptr, "ファイルシステムが有効であること");

    TextureManager::Get().Initialize(fs);
    TEST_ASSERT(TextureManager::Get().IsInitialized(), "TextureManagerが初期化されていること");
}

//! 2Dテクスチャ作成テスト
//! @details Create2Dでのテクスチャ作成をテスト
static void TestTextureManager_Create2D()
{
    std::cout << "\n=== 2Dテクスチャ作成テスト ===" << std::endl;

    if (!TextureManager::Get().IsInitialized()) {
        std::cout << "[スキップ] TextureManagerが初期化されていません" << std::endl;
        return;
    }

    auto pattern = GenerateTestPattern4x4();

    auto texture = TextureManager::Get().Create2D(
        4, 4,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        D3D11_BIND_SHADER_RESOURCE,
        pattern.data(),
        4 * 4);  // rowPitch

    TEST_ASSERT(texture != nullptr, "Create2Dが成功すること");
    TEST_ASSERT(texture->Get() != nullptr, "テクスチャリソースが有効であること");
    TEST_ASSERT(texture->Width() == 4, "幅が4であること");
    TEST_ASSERT(texture->Height() == 4, "高さが4であること");
    TEST_ASSERT(texture->Format() == DXGI_FORMAT_R8G8B8A8_UNORM, "フォーマットがR8G8B8A8_UNORMであること");
    TEST_ASSERT(texture->Is2D(), "2Dテクスチャであること");
}

//! レンダーターゲット作成テスト
//! @details CreateRenderTargetとRTV/SRV取得をテスト
static void TestTextureManager_CreateRenderTarget()
{
    std::cout << "\n=== レンダーターゲット作成テスト ===" << std::endl;

    if (!TextureManager::Get().IsInitialized()) {
        std::cout << "[スキップ] TextureManagerが初期化されていません" << std::endl;
        return;
    }

    auto rt = TextureManager::Get().CreateRenderTarget(512, 512, DXGI_FORMAT_R8G8B8A8_UNORM);

    TEST_ASSERT(rt != nullptr, "CreateRenderTargetが成功すること");
    TEST_ASSERT(rt->Get() != nullptr, "レンダーターゲットが有効であること");
    TEST_ASSERT(rt->Width() == 512, "幅が512であること");
    TEST_ASSERT(rt->Height() == 512, "高さが512であること");
    TEST_ASSERT(rt->HasRtv(), "RTVを持つこと");
    TEST_ASSERT(rt->HasSrv(), "SRVを持つこと");
}

//! 深度ステンシル作成テスト
//! @details CreateDepthStencilとDSV取得をテスト
static void TestTextureManager_CreateDepthStencil()
{
    std::cout << "\n=== 深度ステンシル作成テスト ===" << std::endl;

    if (!TextureManager::Get().IsInitialized()) {
        std::cout << "[スキップ] TextureManagerが初期化されていません" << std::endl;
        return;
    }

    auto ds = TextureManager::Get().CreateDepthStencil(1024, 768, DXGI_FORMAT_D24_UNORM_S8_UINT);

    TEST_ASSERT(ds != nullptr, "CreateDepthStencilが成功すること");
    TEST_ASSERT(ds->Get() != nullptr, "深度ステンシルが有効であること");
    TEST_ASSERT(ds->Width() == 1024, "幅が1024であること");
    TEST_ASSERT(ds->Height() == 768, "高さが768であること");
    TEST_ASSERT(ds->HasDsv(), "DSVを持つこと");
}

//! キャッシュ統計テスト
//! @details GetCacheStatsの動作を確認
static void TestTextureManager_CacheStats()
{
    std::cout << "\n=== キャッシュ統計テスト ===" << std::endl;

    if (!TextureManager::Get().IsInitialized()) {
        std::cout << "[スキップ] TextureManagerが初期化されていません" << std::endl;
        return;
    }

    auto stats = TextureManager::Get().GetCacheStats();

    std::cout << "  テクスチャキャッシュ数: " << stats.textureCount << std::endl;
    std::cout << "  メモリ使用量: " << stats.totalMemoryBytes << " バイト" << std::endl;

    // 統計構造体が正しく動作することを確認
    TEST_ASSERT(stats.textureCount >= 0, "テクスチャカウントが有効であること");
    TEST_ASSERT(stats.totalMemoryBytes >= 0, "メモリ使用量が有効であること");
}

//! 存在しないファイルロードテスト
//! @details 存在しないファイルをロードした場合のエラー処理をテスト
static void TestTextureManager_LoadNonExistent()
{
    std::cout << "\n=== 存在しないファイルロードテスト ===" << std::endl;

    if (!TextureManager::Get().IsInitialized()) {
        std::cout << "[スキップ] TextureManagerが初期化されていません" << std::endl;
        return;
    }

    auto texture = TextureManager::Get().LoadTexture2D("nonexistent_texture.png");
    TEST_ASSERT(texture == nullptr, "存在しないテクスチャファイルがnullptrを返すこと");

    auto cube = TextureManager::Get().LoadTextureCube("nonexistent_cubemap.dds");
    TEST_ASSERT(cube == nullptr, "存在しないキューブマップがnullptrを返すこと");
}

//! TextureManagerクリーンアップテスト
//! @details キャッシュクリアとシャットダウンをテスト
static void TestTextureManager_Cleanup()
{
    std::cout << "\n=== TextureManagerクリーンアップテスト ===" << std::endl;

    TextureManager::Get().ClearCache();
    TextureManager::Get().Shutdown();
    FileSystemManager::Get().UnmountAll();

    TEST_ASSERT(!TextureManager::Get().IsInitialized(), "TextureManagerがシャットダウンされていること");
}

//----------------------------------------------------------------------------
// ファイルベース テクスチャロード テスト
//----------------------------------------------------------------------------

//! テクスチャディレクトリを使用したTextureManager初期化
//! @param textureDir テストテクスチャディレクトリのパス
//! @return 初期化成功時true
static bool InitTextureManagerWithFileSystem(const std::wstring& textureDir)
{
    if (!GraphicsDevice::Get().IsValid()) {
        return false;
    }

    // ホストファイルシステムをマウント
    auto hostFs = std::make_unique<HostFileSystem>(textureDir);
    FileSystemManager::Get().Mount("textures", std::move(hostFs));
    IReadableFileSystem* fs = FileSystemManager::Get().GetFileSystem("textures");

    if (!fs) {
        return false;
    }

    TextureManager::Get().Initialize(fs);
    return TextureManager::Get().IsInitialized();
}

//! PNGテクスチャロードテスト
//! @details checkerboard_256.pngをロードして検証
static void TestTextureManager_LoadPNG()
{
    std::cout << "\n=== PNGテクスチャロードテスト ===" << std::endl;

    if (!TextureManager::Get().IsInitialized()) {
        std::cout << "[スキップ] TextureManagerが初期化されていません" << std::endl;
        return;
    }

    // checkerboard_256.pngをロード
    auto texture = TextureManager::Get().LoadTexture2D("checkerboard_256.png");
    TEST_ASSERT(texture != nullptr, "checkerboard_256.pngのロードが成功すること");

    if (texture) {
        TEST_ASSERT(texture->Width() == 256, "PNGテクスチャの幅が256であること");
        TEST_ASSERT(texture->Height() == 256, "PNGテクスチャの高さが256であること");
        TEST_ASSERT(texture->Is2D(), "PNGテクスチャが2Dであること");
        TEST_ASSERT(texture->HasSrv(), "PNGテクスチャがSRVを持つこと");
    }
}

//! 複数PNGテクスチャロードテスト
//! @details 異なるサイズ・形式のPNGファイルをロード
static void TestTextureManager_LoadMultiplePNG()
{
    std::cout << "\n=== 複数PNGテクスチャロードテスト ===" << std::endl;

    if (!TextureManager::Get().IsInitialized()) {
        std::cout << "[スキップ] TextureManagerが初期化されていません" << std::endl;
        return;
    }

    // gradient_256.pngをロード
    auto gradient = TextureManager::Get().LoadTexture2D("gradient_256.png");
    TEST_ASSERT(gradient != nullptr, "gradient_256.pngのロードが成功すること");

    // white_64.pngをロード（小さなサイズ）
    auto white = TextureManager::Get().LoadTexture2D("white_64.png");
    TEST_ASSERT(white != nullptr, "white_64.pngのロードが成功すること");

    if (white) {
        TEST_ASSERT(white->Width() == 64, "small PNGテクスチャの幅が64であること");
        TEST_ASSERT(white->Height() == 64, "small PNGテクスチャの高さが64であること");
    }

    // normal_flat_256.pngをロード（ノーマルマップ）
    auto normal = TextureManager::Get().LoadTexture2D("normal_flat_256.png", false);  // sRGB=false
    TEST_ASSERT(normal != nullptr, "normal_flat_256.pngのロードが成功すること");
}

//! DDSテクスチャロードテスト
//! @details checkerboard_256.ddsをロードして検証
static void TestTextureManager_LoadDDS()
{
    std::cout << "\n=== DDSテクスチャロードテスト ===" << std::endl;

    if (!TextureManager::Get().IsInitialized()) {
        std::cout << "[スキップ] TextureManagerが初期化されていません" << std::endl;
        return;
    }

    // checkerboard_256.ddsをロード
    auto texture = TextureManager::Get().LoadTexture2D("checkerboard_256.dds");
    TEST_ASSERT(texture != nullptr, "checkerboard_256.ddsのロードが成功すること");

    if (texture) {
        TEST_ASSERT(texture->Width() == 256, "DDSテクスチャの幅が256であること");
        TEST_ASSERT(texture->Height() == 256, "DDSテクスチャの高さが256であること");
        TEST_ASSERT(texture->Is2D(), "DDSテクスチャが2Dであること");
    }

    // gradient_128.ddsをロード（小さなサイズ）
    auto gradient = TextureManager::Get().LoadTexture2D("gradient_128.dds");
    TEST_ASSERT(gradient != nullptr, "gradient_128.ddsのロードが成功すること");

    if (gradient) {
        TEST_ASSERT(gradient->Width() == 128, "gradient DDSの幅が128であること");
        TEST_ASSERT(gradient->Height() == 128, "gradient DDSの高さが128であること");
    }
}

//! テクスチャキャッシュヒットテスト
//! @details 同じテクスチャを2回ロードしてキャッシュ動作を検証
static void TestTextureManager_CacheHit()
{
    std::cout << "\n=== テクスチャキャッシュヒットテスト ===" << std::endl;

    if (!TextureManager::Get().IsInitialized()) {
        std::cout << "[スキップ] TextureManagerが初期化されていません" << std::endl;
        return;
    }

    // 同じテクスチャを2回ロード
    auto tex1 = TextureManager::Get().LoadTexture2D("checkerboard_256.png");
    auto tex2 = TextureManager::Get().LoadTexture2D("checkerboard_256.png");

    TEST_ASSERT(tex1 != nullptr && tex2 != nullptr, "両方のテクスチャロードが成功すること");
    TEST_ASSERT(tex1.get() == tex2.get(), "同じテクスチャがキャッシュから返されること");

    // キャッシュ統計を確認
    auto stats = TextureManager::Get().GetCacheStats();
    TEST_ASSERT(stats.textureCount > 0, "キャッシュにテクスチャが存在すること");
}

//! カラーチャンネルテクスチャテスト
//! @details 単色テクスチャ（赤、緑、青）をロードしてチャンネル確認
static void TestTextureManager_ColorChannels()
{
    std::cout << "\n=== カラーチャンネルテクスチャテスト ===" << std::endl;

    if (!TextureManager::Get().IsInitialized()) {
        std::cout << "[スキップ] TextureManagerが初期化されていません" << std::endl;
        return;
    }

    // 各色チャンネルのテクスチャをロード
    auto red = TextureManager::Get().LoadTexture2D("red_64.png");
    TEST_ASSERT(red != nullptr, "red_64.pngのロードが成功すること");

    auto green = TextureManager::Get().LoadTexture2D("green_64.png");
    TEST_ASSERT(green != nullptr, "green_64.pngのロードが成功すること");

    auto blue = TextureManager::Get().LoadTexture2D("blue_64.png");
    TEST_ASSERT(blue != nullptr, "blue_64.pngのロードが成功すること");

    auto black = TextureManager::Get().LoadTexture2D("black_64.png");
    TEST_ASSERT(black != nullptr, "black_64.pngのロードが成功すること");
}

//! ファイルベーステスト用クリーンアップ
static void TestTextureManager_FileBasedCleanup()
{
    std::cout << "\n=== ファイルベーステストクリーンアップ ===" << std::endl;

    TextureManager::Get().ClearCache();
    TextureManager::Get().Shutdown();
    FileSystemManager::Get().UnmountAll();

    TEST_ASSERT(!TextureManager::Get().IsInitialized(), "TextureManagerがシャットダウンされていること");
}

//----------------------------------------------------------------------------
// 公開インターフェース
//----------------------------------------------------------------------------

//! テクスチャテストスイートを実行
//! @param textureDir テストテクスチャディレクトリのパス（オプション）
//! @return 全テスト成功時true、それ以外false
bool RunTextureTests(const std::wstring& textureDir)
{
    std::cout << "\n========================================" << std::endl;
    std::cout << "  テクスチャシステム テスト" << std::endl;
    std::cout << "========================================" << std::endl;

    ResetGlobalCounters();

    // TextureManagerテスト（メモリファイルシステム使用）
    TestTextureManager_Initialize();
    TestTextureManager_Create2D();
    TestTextureManager_CreateRenderTarget();
    TestTextureManager_CreateDepthStencil();
    TestTextureManager_CacheStats();
    TestTextureManager_LoadNonExistent();
    TestTextureManager_Cleanup();

    // ファイルベーステスト（テクスチャディレクトリが指定された場合のみ）
    if (!textureDir.empty()) {
        std::cout << "\n--- ファイルベーステクスチャテスト ---" << std::endl;
        std::wcout << L"テクスチャディレクトリ: " << textureDir << std::endl;

        if (InitTextureManagerWithFileSystem(textureDir)) {
            TestTextureManager_LoadPNG();
            TestTextureManager_LoadMultiplePNG();
            TestTextureManager_LoadDDS();
            TestTextureManager_CacheHit();
            TestTextureManager_ColorChannels();
            TestTextureManager_FileBasedCleanup();
        } else {
            std::cout << "[スキップ] ファイルベーステスト（初期化失敗）" << std::endl;
        }
    } else {
        std::cout << "\n[スキップ] ファイルベーステスト（テクスチャディレクトリ未指定）" << std::endl;
    }

    std::cout << "\n----------------------------------------" << std::endl;
    std::cout << "テクスチャテスト: " << s_passCount << "/" << s_testCount << " 成功" << std::endl;
    std::cout << "----------------------------------------" << std::endl;

    return s_passCount == s_testCount;
}

} // namespace tests
