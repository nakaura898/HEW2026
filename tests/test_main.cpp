//----------------------------------------------------------------------------
//! @file   test_main.cpp
//! @brief  テストランナー メインエントリーポイント
//!
//! @details
//! mutra DirectX11ラッパーライブラリのテストスイートを実行します。
//!
//! テストスイート:
//! - FileSystemテスト: ファイルシステム抽象化レイヤーのテスト
//! - Shaderテスト: シェーダーコンパイル・ロード・管理のテスト
//! - Textureテスト: テクスチャ生成・ロード・キャッシュのテスト
//! - Bufferテスト: バッファ生成・GPU Readback検証のテスト
//!
//! コマンドライン引数:
//!   --help           ヘルプ表示
//!   --no-device      D3D11デバイス初期化をスキップ
//!   --no-debug       D3D11デバッグレイヤーを無効化
//!   --fs-only        FileSystemテストのみ実行
//!   --shader-only    Shaderテストのみ実行
//!   --texture-only   Textureテストのみ実行
//!   --buffer-only    Bufferテストのみ実行
//!   --assets-dir     テストアセットディレクトリを指定
//----------------------------------------------------------------------------
#include "test_file_system.h"
#include "test_shader.h"
#include "test_texture.h"
#include "test_buffer.h"

#include "dx11/gpu_common.h"
#include "dx11/graphics_device.h"
#include "dx11/graphics_context.h"
#include "common/logging/logging.h"

#include <iostream>
#include <string>
#include <vector>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

//----------------------------------------------------------------------------
// コマンドライン引数解析
//----------------------------------------------------------------------------

//! テスト設定構造体
struct TestConfig
{
    bool runFileSystemTests = true;   //!< FileSystemテストを実行
    bool runShaderTests = true;       //!< Shaderテストを実行
    bool runTextureTests = true;      //!< Textureテストを実行
    bool runBufferTests = true;       //!< Bufferテストを実行
    bool initDevice = true;           //!< D3D11デバイスを初期化
    bool debugDevice = true;          //!< D3D11デバッグレイヤーを有効化
    std::wstring hostTestDir;         //!< HostFileSystemテスト用ディレクトリ
    std::wstring textureDir;          //!< テストテクスチャディレクトリ
    std::wstring assetsDir;           //!< テストアセットディレクトリ
};

//! 使用方法を表示
static void PrintUsage(const char* programName)
{
    std::cout << "使用方法: " << programName << " [オプション]\n"
              << "\nオプション:\n"
              << "  --help                 このヘルプを表示\n"
              << "  --no-device            D3D11デバイス初期化をスキップ\n"
              << "  --no-debug             D3D11デバッグレイヤーを無効化\n"
              << "  --fs-only              FileSystemテストのみ実行\n"
              << "  --shader-only          Shaderテストのみ実行\n"
              << "  --texture-only         Textureテストのみ実行\n"
              << "  --buffer-only          Bufferテストのみ実行\n"
              << "  --host-dir=<パス>      HostFileSystemテスト用ディレクトリ\n"
              << "  --texture-dir=<パス>   テストテクスチャを含むディレクトリ\n"
              << "  --assets-dir=<パス>    テストアセットディレクトリ\n"
              << std::endl;
}

//! コマンドライン引数を解析
static TestConfig ParseCommandLine(int argc, char* argv[])
{
    TestConfig config;

    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--help") {
            PrintUsage(argv[0]);
            exit(0);
        }
        else if (arg == "--no-device") {
            config.initDevice = false;
        }
        else if (arg == "--no-debug") {
            config.debugDevice = false;
        }
        else if (arg == "--fs-only") {
            config.runFileSystemTests = true;
            config.runShaderTests = false;
            config.runTextureTests = false;
            config.runBufferTests = false;
        }
        else if (arg == "--shader-only") {
            config.runFileSystemTests = false;
            config.runShaderTests = true;
            config.runTextureTests = false;
            config.runBufferTests = false;
        }
        else if (arg == "--texture-only") {
            config.runFileSystemTests = false;
            config.runShaderTests = false;
            config.runTextureTests = true;
            config.runBufferTests = false;
        }
        else if (arg == "--buffer-only") {
            config.runFileSystemTests = false;
            config.runShaderTests = false;
            config.runTextureTests = false;
            config.runBufferTests = true;
        }
        else if (arg.rfind("--host-dir=", 0) == 0) {
            std::string path = arg.substr(11);
            config.hostTestDir = std::wstring(path.begin(), path.end());
        }
        else if (arg.rfind("--texture-dir=", 0) == 0) {
            std::string path = arg.substr(14);
            config.textureDir = std::wstring(path.begin(), path.end());
        }
        else if (arg.rfind("--assets-dir=", 0) == 0) {
            std::string path = arg.substr(13);
            config.assetsDir = std::wstring(path.begin(), path.end());
        }
        else {
            LOG_ERROR(("不明な引数: " + arg).c_str());
            PrintUsage(argv[0]);
            exit(1);
        }
    }

    return config;
}

//----------------------------------------------------------------------------
// アセットディレクトリ自動検出
//----------------------------------------------------------------------------

//! テストアセットディレクトリを自動検出
//! @return 見つかった場合はパス、見つからない場合は空文字列
static std::wstring FindAssetsDirectory()
{
    // カレントディレクトリからの相対パスで検索
    std::vector<fs::path> searchPaths = {
        fs::current_path() / "tests" / "assets",                          // プロジェクトルートから実行
        fs::current_path().parent_path() / "tests" / "assets",            // buildディレクトリから実行
        fs::current_path().parent_path().parent_path() / "tests" / "assets",  // build/Debugから実行
    };

    for (const auto& path : searchPaths) {
        if (fs::exists(path) && fs::is_directory(path)) {
            return path.wstring();
        }
    }

    return L"";
}

//----------------------------------------------------------------------------
// メインエントリーポイント
//----------------------------------------------------------------------------

int main(int argc, char* argv[])
{
#ifdef _WIN32
    // コンソール出力をUTF-8に設定
    SetConsoleOutputCP(CP_UTF8);
#endif

    std::cout << "========================================\n";
    std::cout << "  mutra DirectX11 ラッパー テスト\n";
    std::cout << "========================================\n" << std::endl;

    TestConfig config = ParseCommandLine(argc, argv);

    // アセットディレクトリが指定されていない場合は自動検出
    if (config.assetsDir.empty()) {
        config.assetsDir = FindAssetsDirectory();
        if (!config.assetsDir.empty()) {
            std::cout << "アセットディレクトリを自動検出: ";
            std::wcout << config.assetsDir << std::endl;
        }
    }

    // テクスチャディレクトリが指定されていない場合はassets/texturesを使用
    if (config.textureDir.empty() && !config.assetsDir.empty()) {
        fs::path texturePath = fs::path(config.assetsDir) / "textures";
        if (fs::exists(texturePath)) {
            config.textureDir = texturePath.wstring();
        }
    }

    int totalTests = 0;
    int passedTests = 0;

    // D3D11デバイスの初期化
    if (config.initDevice) {
        std::cout << "D3D11デバイスを初期化中..." << std::endl;

        if (!GraphicsDevice::Get().Initialize(config.debugDevice)) {
            LOG_ERROR("GraphicsDeviceの初期化に失敗しました！一部のテストはスキップされます。");
        }
        else {
            LOG_INFO("GraphicsDevice: 初期化成功");

            if (!GraphicsContext::Get().Initialize()) {
                LOG_ERROR("GraphicsContextの初期化に失敗しました！");
            }
            else {
                LOG_INFO("GraphicsContext: 初期化成功");
            }
        }
    }
    else {
        std::cout << "D3D11デバイス初期化をスキップ (--no-device)" << std::endl;
    }

    // FileSystemテストの実行
    if (config.runFileSystemTests) {
        bool passed = tests::RunFileSystemTests(config.hostTestDir);
        totalTests++;
        if (passed) passedTests++;
    }

    // Shaderテストの実行
    if (config.runShaderTests) {
        bool passed = tests::RunShaderTests(config.assetsDir);
        totalTests++;
        if (passed) passedTests++;
    }

    // Textureテストの実行
    if (config.runTextureTests) {
        bool passed = tests::RunTextureTests(config.textureDir);
        totalTests++;
        if (passed) passedTests++;
    }

    // Bufferテストの実行
    if (config.runBufferTests) {
        bool passed = tests::RunBufferTests();
        totalTests++;
        if (passed) passedTests++;
    }

    // クリーンアップ
    if (config.initDevice && GraphicsDevice::Get().IsValid()) {
        GraphicsContext::Get().Shutdown();
        GraphicsDevice::Get().Shutdown();
    }

    // テスト結果サマリー
    std::cout << "\n========================================" << std::endl;
    std::cout << "  テスト結果サマリー" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "テストスイート: " << passedTests << "/" << totalTests << " 成功" << std::endl;

    if (passedTests == totalTests) {
        std::cout << "\n全てのテストが成功しました！" << std::endl;
        return 0;
    }
    else {
        std::cout << "\n一部のテストが失敗しました。" << std::endl;
        return 1;
    }
}
