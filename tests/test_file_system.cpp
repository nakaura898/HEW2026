//----------------------------------------------------------------------------
//! @file   test_file_system.cpp
//! @brief  ファイルシステム テストスイート
//!
//! @details
//! このファイルはファイルシステム抽象化レイヤーの包括的なテストを提供します。
//!
//! テストカテゴリ:
//! - MemoryFileSystem: メモリ上の仮想ファイルシステム
//!   - ファイルの追加と読み取り
//!   - バイナリデータの処理
//!   - ファイルハンドル操作（シーク、部分読み取り）
//!   - クリア操作
//! - FileSystemManager: マウントベースのファイルシステム管理
//!   - マウント/アンマウント操作
//!   - パス解決（mount:/path形式）
//!   - 複数マウントポイントの管理
//! - HostFileSystem: 実際のファイルシステムへのアクセス
//!   - ファイルの読み書き
//!   - ディレクトリ操作
//!
//! @note HostFileSystemテストはテストディレクトリが指定された場合のみ実行
//----------------------------------------------------------------------------
#include "test_file_system.h"
#include "test_common.h"
#include "engine/fs/file_system.h"
#include "engine/fs/file_system_types.h"
#include "engine/fs/file_system_manager.h"
#include "engine/fs/host_file_system.h"
#include "engine/fs/memory_file_system.h"
#include "common/logging/logging.h"
#include <algorithm>
#include <cassert>
#include <iostream>

namespace tests {

//----------------------------------------------------------------------------
// テストユーティリティ（共通ヘッダーから使用）
//----------------------------------------------------------------------------

// グローバルカウンターを使用（後方互換性のため）
#define s_testCount tests::GetGlobalTestCount()
#define s_passCount tests::GetGlobalPassCount()

//----------------------------------------------------------------------------
// MemoryFileSystem テスト
//----------------------------------------------------------------------------

//! テキストファイルの追加と読み取りテスト
//! @details addTextFileとread/readAsTextの基本動作をテスト
static void TestMemoryFileSystem_AddAndRead()
{
    std::cout << "\n=== テキストファイル追加・読み取りテスト ===" << std::endl;

    MemoryFileSystem fs;

    // テキストファイルを追加
    fs.addTextFile("test.txt", "Hello, World!");

    // 存在確認
    TEST_ASSERT(fs.exists("test.txt"), "追加後にファイルが存在すること");
    TEST_ASSERT(!fs.exists("nonexistent.txt"), "存在しないファイルはexists()がfalseを返すこと");

    // ファイル/ディレクトリ判定
    TEST_ASSERT(fs.isFile("test.txt"), "test.txtがファイルであること");
    TEST_ASSERT(!fs.isDirectory("test.txt"), "test.txtがディレクトリでないこと");

    // ファイル読み取り
    auto result = fs.read("test.txt");
    TEST_ASSERT(result.success, "test.txtの読み取りが成功すること");
    TEST_ASSERT(result.bytes.size() == 13, "ファイルサイズが13バイトであること");

    // テキストとして読み取り
    std::string text = fs.readAsText("test.txt");
    TEST_ASSERT(text == "Hello, World!", "テキスト内容が一致すること");

    // ファイルサイズ取得
    int64_t size = fs.getFileSize("test.txt");
    TEST_ASSERT(size == 13, "getFileSizeが13を返すこと");
}

//! バイナリデータの追加と読み取りテスト
//! @details 非テキストデータ（バイナリ）の処理をテスト
static void TestMemoryFileSystem_BinaryData()
{
    std::cout << "\n=== バイナリデータテスト ===" << std::endl;

    MemoryFileSystem fs;

    // バイナリデータを追加
    std::vector<std::byte> binaryData = {
        std::byte{0x00}, std::byte{0x01}, std::byte{0x02}, std::byte{0x03},
        std::byte{0xFF}, std::byte{0xFE}, std::byte{0xFD}, std::byte{0xFC}
    };
    fs.addFile("binary.dat", binaryData);

    // 読み取りと検証
    auto result = fs.read("binary.dat");
    TEST_ASSERT(result.success, "binary.datの読み取りが成功すること");
    TEST_ASSERT(result.bytes.size() == 8, "バイナリファイルが8バイトであること");
    TEST_ASSERT(result.bytes[0] == std::byte{0x00}, "先頭バイトが0x00であること");
    TEST_ASSERT(result.bytes[4] == std::byte{0xFF}, "5番目のバイトが0xFFであること");
}

//! ファイルハンドル操作テスト
//! @details open()で取得したハンドルのシーク・部分読み取り・EOF判定をテスト
static void TestMemoryFileSystem_FileHandle()
{
    std::cout << "\n=== ファイルハンドル操作テスト ===" << std::endl;

    MemoryFileSystem fs;
    fs.addTextFile("handle_test.txt", "ABCDEFGHIJ");

    // ファイルハンドルを開く
    auto handle = fs.open("handle_test.txt");
    TEST_ASSERT(handle != nullptr, "ハンドルが有効であること");
    TEST_ASSERT(handle->isValid(), "ハンドルがisValid()=trueを返すこと");
    TEST_ASSERT(handle->size() == 10, "ハンドルサイズが10であること");
    TEST_ASSERT(handle->tell() == 0, "初期位置が0であること");
    TEST_ASSERT(!handle->isEof(), "初期状態でEOFでないこと");

    // 部分読み取り
    auto partialResult = handle->read(5);
    TEST_ASSERT(partialResult.success, "部分読み取りが成功すること");
    TEST_ASSERT(partialResult.bytes.size() == 5, "5バイト読み取れること");
    TEST_ASSERT(handle->tell() == 5, "読み取り後の位置が5であること");

    // シーク操作
    TEST_ASSERT(handle->seek(0, SeekOrigin::Begin), "先頭へのシークが成功すること");
    TEST_ASSERT(handle->tell() == 0, "シーク後の位置が0であること");

    TEST_ASSERT(handle->seek(0, SeekOrigin::End), "末尾へのシークが成功すること");
    TEST_ASSERT(handle->tell() == 10, "末尾での位置が10であること");
    TEST_ASSERT(handle->isEof(), "末尾でEOFであること");
}

//! クリア操作テスト
//! @details clear()による全ファイル削除をテスト
static void TestMemoryFileSystem_Clear()
{
    std::cout << "\n=== クリア操作テスト ===" << std::endl;

    MemoryFileSystem fs;
    fs.addTextFile("file1.txt", "content1");
    fs.addTextFile("file2.txt", "content2");

    TEST_ASSERT(fs.exists("file1.txt"), "file1.txtが存在すること");
    TEST_ASSERT(fs.exists("file2.txt"), "file2.txtが存在すること");

    fs.clear();

    TEST_ASSERT(!fs.exists("file1.txt"), "クリア後にfile1.txtが存在しないこと");
    TEST_ASSERT(!fs.exists("file2.txt"), "クリア後にfile2.txtが存在しないこと");
}

//----------------------------------------------------------------------------
// FileSystemManager テスト
//----------------------------------------------------------------------------

//! マウント/アンマウント操作テスト
//! @details Mount(), Unmount(), IsMounted()の基本動作をテスト
static void TestFileSystemManager_MountUnmount()
{
    std::cout << "\n=== マウント/アンマウント操作テスト ===" << std::endl;

    auto& manager = FileSystemManager::Get();

    // メモリファイルシステムを作成してマウント
    auto memFs = std::make_unique<MemoryFileSystem>();
    memFs->addTextFile("test.txt", "Manager test content");

    bool mounted = manager.Mount("test", std::move(memFs));
    TEST_ASSERT(mounted, "マウントが成功すること");
    TEST_ASSERT(manager.IsMounted("test"), "testがマウントされていること");
    TEST_ASSERT(!manager.IsMounted("nonexistent"), "存在しないマウントポイントはIsMounted()がfalseを返すこと");

    // マネージャー経由で読み取り
    auto result = manager.ReadFile("test:/test.txt");
    TEST_ASSERT(result.success, "マネージャー経由の読み取りが成功すること");

    std::string text = manager.ReadFileAsText("test:/test.txt");
    TEST_ASSERT(text == "Manager test content", "内容が一致すること");

    // アンマウント
    manager.Unmount("test");
    TEST_ASSERT(!manager.IsMounted("test"), "アンマウント後にtestがマウントされていないこと");
}

//! パス解決テスト
//! @details mount:/path形式のパス解決とネストしたファイルへのアクセスをテスト
static void TestFileSystemManager_PathResolution()
{
    std::cout << "\n=== パス解決テスト ===" << std::endl;

    auto& manager = FileSystemManager::Get();

    auto memFs = std::make_unique<MemoryFileSystem>();
    memFs->addTextFile("subdir/file.txt", "Nested content");
    manager.Mount("assets", std::move(memFs));

    // マネージャー経由で存在確認
    TEST_ASSERT(manager.Exists("assets:/subdir/file.txt"), "ネストしたファイルが存在すること");
    TEST_ASSERT(!manager.Exists("assets:/nonexistent.txt"), "存在しないファイルはExists()がfalseを返すこと");

    // ネストしたファイルを読み取り
    std::string content = manager.ReadFileAsText("assets:/subdir/file.txt");
    TEST_ASSERT(content == "Nested content", "ネストしたファイルの内容が一致すること");

    manager.Unmount("assets");
}

//! 複数マウントテスト
//! @details 複数のファイルシステムを同時にマウントした場合の動作をテスト
static void TestFileSystemManager_MultipleMounts()
{
    std::cout << "\n=== 複数マウントテスト ===" << std::endl;

    auto& manager = FileSystemManager::Get();

    auto fs1 = std::make_unique<MemoryFileSystem>();
    fs1->addTextFile("file.txt", "From FS1");

    auto fs2 = std::make_unique<MemoryFileSystem>();
    fs2->addTextFile("file.txt", "From FS2");

    manager.Mount("fs1", std::move(fs1));
    manager.Mount("fs2", std::move(fs2));

    std::string content1 = manager.ReadFileAsText("fs1:/file.txt");
    std::string content2 = manager.ReadFileAsText("fs2:/file.txt");

    TEST_ASSERT(content1 == "From FS1", "fs1の内容が'From FS1'であること");
    TEST_ASSERT(content2 == "From FS2", "fs2の内容が'From FS2'であること");

    manager.UnmountAll();
    TEST_ASSERT(!manager.IsMounted("fs1"), "UnmountAll後にfs1がマウントされていないこと");
    TEST_ASSERT(!manager.IsMounted("fs2"), "UnmountAll後にfs2がマウントされていないこと");
}

//----------------------------------------------------------------------------
// MemoryFileSystem エラーハンドリング・境界値テスト
//----------------------------------------------------------------------------

//! エラーハンドリングテスト
//! @details 存在しないファイル等のエラーケースをテスト
static void TestMemoryFileSystem_ErrorHandling()
{
    std::cout << "\n=== エラーハンドリングテスト ===" << std::endl;

    MemoryFileSystem fs;
    fs.addTextFile("exists.txt", "content");

    // 存在しないファイルの読み取り
    auto result = fs.read("nonexistent.txt");
    TEST_ASSERT(!result.success, "存在しないファイルの読み取りが失敗すること");

    // 存在しないファイルのテキスト読み取り
    std::string text = fs.readAsText("nonexistent.txt");
    TEST_ASSERT(text.empty(), "存在しないファイルのreadAsTextが空文字を返すこと");

    // 存在しないファイルのサイズ取得
    int64_t size = fs.getFileSize("nonexistent.txt");
    TEST_ASSERT(size == -1, "存在しないファイルのgetFileSizeが-1を返すこと");

    // 存在しないファイルのハンドル取得
    auto handle = fs.open("nonexistent.txt");
    TEST_ASSERT(handle == nullptr, "存在しないファイルのopenがnullptrを返すこと");

    // isFile/isDirectory on nonexistent
    TEST_ASSERT(!fs.isFile("nonexistent.txt"), "存在しないファイルのisFileがfalseを返すこと");
    TEST_ASSERT(!fs.isDirectory("nonexistent.txt"), "存在しないファイルのisDirectoryがfalseを返すこと");
}

//! ファイルハンドル境界値テスト
//! @details シーク・読み取りの境界条件をテスト
static void TestMemoryFileSystem_HandleBoundary()
{
    std::cout << "\n=== ファイルハンドル境界値テスト ===" << std::endl;

    MemoryFileSystem fs;
    fs.addTextFile("boundary.txt", "0123456789"); // 10バイト

    auto handle = fs.open("boundary.txt");
    TEST_ASSERT(handle != nullptr, "ハンドルが有効であること");

    // SeekOrigin::Current テスト
    handle->seek(0, SeekOrigin::Begin);
    TEST_ASSERT(handle->seek(3, SeekOrigin::Current), "相対シーク(+3)が成功すること");
    TEST_ASSERT(handle->tell() == 3, "相対シーク後の位置が3であること");

    TEST_ASSERT(handle->seek(2, SeekOrigin::Current), "相対シーク(+2)が成功すること");
    TEST_ASSERT(handle->tell() == 5, "相対シーク後の位置が5であること");

    TEST_ASSERT(handle->seek(-2, SeekOrigin::Current), "相対シーク(-2)が成功すること");
    TEST_ASSERT(handle->tell() == 3, "相対シーク後の位置が3であること");

    // 末尾からの相対シーク
    TEST_ASSERT(handle->seek(-3, SeekOrigin::End), "末尾からの相対シーク(-3)が成功すること");
    TEST_ASSERT(handle->tell() == 7, "末尾からの相対シーク後の位置が7であること");

    // 0バイト読み取り
    handle->seek(0, SeekOrigin::Begin);
    auto result = handle->read(0);
    TEST_ASSERT(result.success, "0バイト読み取りが成功すること");
    TEST_ASSERT(result.bytes.size() == 0, "0バイト読み取り結果が0バイトであること");

    // ファイルサイズを超える読み取り要求
    handle->seek(0, SeekOrigin::Begin);
    auto largeResult = handle->read(1000);
    TEST_ASSERT(largeResult.success, "サイズ超過読み取りが成功すること");
    TEST_ASSERT(largeResult.bytes.size() == 10, "サイズ超過読み取りが実際のサイズを返すこと");

    // EOF位置での読み取り
    handle->seek(0, SeekOrigin::End);
    auto eofResult = handle->read(10);
    TEST_ASSERT(eofResult.success, "EOF位置での読み取りが成功すること");
    TEST_ASSERT(eofResult.bytes.size() == 0, "EOF位置での読み取りが0バイトを返すこと");

    // 部分読み取り（残りが要求より少ない場合）
    handle->seek(8, SeekOrigin::Begin);
    auto partialResult = handle->read(10);
    TEST_ASSERT(partialResult.success, "部分読み取りが成功すること");
    TEST_ASSERT(partialResult.bytes.size() == 2, "部分読み取りが残り2バイトを返すこと");
}

//! 空ファイルテスト
//! @details 空ファイルの処理をテスト
static void TestMemoryFileSystem_EmptyFile()
{
    std::cout << "\n=== 空ファイルテスト ===" << std::endl;

    MemoryFileSystem fs;
    fs.addTextFile("empty.txt", "");

    TEST_ASSERT(fs.exists("empty.txt"), "空ファイルが存在すること");
    TEST_ASSERT(fs.isFile("empty.txt"), "空ファイルがファイルであること");
    TEST_ASSERT(fs.getFileSize("empty.txt") == 0, "空ファイルのサイズが0であること");

    auto result = fs.read("empty.txt");
    TEST_ASSERT(result.success, "空ファイルの読み取りが成功すること");
    TEST_ASSERT(result.bytes.size() == 0, "空ファイルの内容が0バイトであること");

    auto handle = fs.open("empty.txt");
    TEST_ASSERT(handle != nullptr, "空ファイルのハンドルが有効であること");
    TEST_ASSERT(handle->size() == 0, "空ファイルハンドルのサイズが0であること");
    TEST_ASSERT(handle->isEof(), "空ファイルは最初からEOFであること");
}

//! ネストしたパステスト
//! @details パス内のスラッシュを含むファイル名のテスト
//! @note MemoryFileSystemはディレクトリ構造をサポートしない（listDirectoryは空を返す）
static void TestMemoryFileSystem_NestedPaths()
{
    std::cout << "\n=== ネストしたパステスト ===" << std::endl;

    MemoryFileSystem fs;
    fs.addTextFile("root.txt", "root file");
    fs.addTextFile("dir1/file1.txt", "file in dir1");
    fs.addTextFile("dir1/file2.txt", "another file in dir1");
    fs.addTextFile("dir1/subdir/deep.txt", "deep file");
    fs.addTextFile("dir2/other.txt", "file in dir2");

    // ネストしたファイルの存在確認
    TEST_ASSERT(fs.exists("dir1/subdir/deep.txt"), "深くネストしたファイルが存在すること");
    TEST_ASSERT(fs.isFile("dir1/subdir/deep.txt"), "深くネストしたファイルがファイルであること");

    // 各ファイルの読み取りテスト
    TEST_ASSERT(fs.readAsText("root.txt") == "root file", "ルートファイルの内容が一致すること");
    TEST_ASSERT(fs.readAsText("dir1/file1.txt") == "file in dir1", "dir1/file1.txtの内容が一致すること");
    TEST_ASSERT(fs.readAsText("dir1/subdir/deep.txt") == "deep file", "深くネストしたファイルの内容が一致すること");

    // MemoryFileSystemはディレクトリをサポートしない（仕様確認）
    TEST_ASSERT(!fs.isDirectory("dir1"), "MemoryFileSystemのisDirectoryは常にfalseを返すこと");
    auto entries = fs.listDirectory("dir1");
    TEST_ASSERT(entries.empty(), "MemoryFileSystemのlistDirectoryは空を返すこと（仕様）");
}

//----------------------------------------------------------------------------
// FileSystemManager エラーハンドリングテスト
//----------------------------------------------------------------------------

//! FileSystemManager エラーハンドリングテスト
//! @details 無効なパス、未マウントポイントへのアクセスをテスト
static void TestFileSystemManager_ErrorHandling()
{
    std::cout << "\n=== FileSystemManager エラーハンドリングテスト ===" << std::endl;

    auto& manager = FileSystemManager::Get();
    manager.UnmountAll();

    // マウントされていないポイントへのアクセス
    auto result = manager.ReadFile("unmounted:/test.txt");
    TEST_ASSERT(!result.success, "未マウントポイントの読み取りが失敗すること");

    // 無効なパス形式
    auto invalidResult = manager.ReadFile("invalid_path_without_colon");
    TEST_ASSERT(!invalidResult.success, "無効なパス形式の読み取りが失敗すること");

    // Exists on unmounted
    TEST_ASSERT(!manager.Exists("unmounted:/test.txt"), "未マウントポイントのExistsがfalseを返すこと");

    // 空のマウント名
    auto emptyFs = std::make_unique<MemoryFileSystem>();
    bool emptyMounted = manager.Mount("", std::move(emptyFs));
    // 空のマウント名が許可されるかは実装依存だが、動作を確認
    if (emptyMounted) {
        manager.Unmount("");
    }

    // 重複マウント
    auto fs1 = std::make_unique<MemoryFileSystem>();
    auto fs2 = std::make_unique<MemoryFileSystem>();
    manager.Mount("duplicate", std::move(fs1));
    bool duplicateMounted = manager.Mount("duplicate", std::move(fs2));
    TEST_ASSERT(!duplicateMounted, "重複マウントが失敗すること");
    manager.Unmount("duplicate");
}

//----------------------------------------------------------------------------
// HostFileSystem テスト (実際のファイルが必要)
//----------------------------------------------------------------------------

//! ホストファイルシステム基本テスト
//! @details 実際のファイルシステムへの読み書き操作をテスト
//! @param testDir テスト用ディレクトリのパス
static void TestHostFileSystem_Basic(const std::wstring& testDir)
{
    std::cout << "\n=== ホストファイルシステム基本テスト ===" << std::endl;

    HostFileSystem fs(testDir);

    // テストファイルを作成
    std::string testContent = "Host filesystem test content\nLine 2\nLine 3";
    std::vector<std::byte> data(testContent.size());
    std::transform(testContent.begin(), testContent.end(), data.begin(),
        [](char c) { return static_cast<std::byte>(c); });

    auto createResult = fs.writeFile("host_test.txt", data);
    TEST_ASSERT(createResult.success, "テストファイルの作成が成功すること");

    // 存在確認
    TEST_ASSERT(fs.exists("host_test.txt"), "host_test.txtが存在すること");
    TEST_ASSERT(fs.isFile("host_test.txt"), "host_test.txtがファイルであること");

    // 読み取り
    std::string readContent = fs.readAsText("host_test.txt");
    TEST_ASSERT(readContent == testContent, "読み取り内容が書き込み内容と一致すること");

    // ファイルサイズ取得
    int64_t size = fs.getFileSize("host_test.txt");
    TEST_ASSERT(size == static_cast<int64_t>(testContent.size()), "ファイルサイズが一致すること");

    // ファイル削除
    auto deleteResult = fs.deleteFile("host_test.txt");
    TEST_ASSERT(deleteResult.success, "テストファイルの削除が成功すること");
    TEST_ASSERT(!fs.exists("host_test.txt"), "削除後にhost_test.txtが存在しないこと");
}

//! ディレクトリ操作テスト
//! @details ディレクトリの作成・一覧・再帰削除をテスト
//! @param testDir テスト用ディレクトリのパス
static void TestHostFileSystem_Directory(const std::wstring& testDir)
{
    std::cout << "\n=== ディレクトリ操作テスト ===" << std::endl;

    HostFileSystem fs(testDir);

    // ディレクトリ作成
    auto createDirResult = fs.createDirectory("test_subdir");
    TEST_ASSERT(createDirResult.success, "ディレクトリ作成が成功すること");
    TEST_ASSERT(fs.exists("test_subdir"), "test_subdirが存在すること");
    TEST_ASSERT(fs.isDirectory("test_subdir"), "test_subdirがディレクトリであること");

    // ディレクトリ内にファイル作成
    std::string content = "File in subdirectory";
    std::vector<std::byte> data(content.size());
    std::transform(content.begin(), content.end(), data.begin(),
        [](char c) { return static_cast<std::byte>(c); });
    fs.writeFile("test_subdir/nested.txt", data);

    TEST_ASSERT(fs.exists("test_subdir/nested.txt"), "ネストしたファイルが存在すること");

    // ディレクトリ一覧
    auto entries = fs.listDirectory("test_subdir");
    bool foundNested = false;
    for (const auto& entry : entries) {
        if (entry.name == "nested.txt") {
            foundNested = true;
            TEST_ASSERT(entry.type == FileEntryType::File, "nested.txtがファイルであること");
        }
    }
    TEST_ASSERT(foundNested, "ディレクトリ一覧にnested.txtが含まれること");

    // クリーンアップ
    fs.deleteDirectoryRecursively("test_subdir");
    TEST_ASSERT(!fs.exists("test_subdir"), "再帰削除後にtest_subdirが存在しないこと");
}

//----------------------------------------------------------------------------
// 公開インターフェース
//----------------------------------------------------------------------------

//! ファイルシステムテストスイートを実行
//! @param hostTestDir HostFileSystemテスト用ディレクトリ（オプション）
//! @return 全テスト成功時true、それ以外false
bool RunFileSystemTests(const std::wstring& hostTestDir)
{
    std::cout << "\n========================================" << std::endl;
    std::cout << "  ファイルシステム テスト" << std::endl;
    std::cout << "========================================" << std::endl;

    ResetGlobalCounters();

    // MemoryFileSystemテスト
    TestMemoryFileSystem_AddAndRead();
    TestMemoryFileSystem_BinaryData();
    TestMemoryFileSystem_FileHandle();
    TestMemoryFileSystem_Clear();
    TestMemoryFileSystem_ErrorHandling();
    TestMemoryFileSystem_HandleBoundary();
    TestMemoryFileSystem_EmptyFile();
    TestMemoryFileSystem_NestedPaths();

    // FileSystemManagerテスト
    TestFileSystemManager_MountUnmount();
    TestFileSystemManager_PathResolution();
    TestFileSystemManager_MultipleMounts();
    TestFileSystemManager_ErrorHandling();

    // HostFileSystemテスト（テストディレクトリが指定された場合のみ）
    if (!hostTestDir.empty()) {
        TestHostFileSystem_Basic(hostTestDir);
        TestHostFileSystem_Directory(hostTestDir);
    } else {
        std::cout << "\n[スキップ] HostFileSystemテスト（テストディレクトリ未指定）" << std::endl;
    }

    std::cout << "\n----------------------------------------" << std::endl;
    std::cout << "ファイルシステムテスト: " << s_passCount << "/" << s_testCount << " 成功" << std::endl;
    std::cout << "----------------------------------------" << std::endl;

    return s_passCount == s_testCount;
}

} // namespace tests
