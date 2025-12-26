//----------------------------------------------------------------------------
//! @file   file_system_manager.h
//! @brief  ファイルシステムマネージャー
//----------------------------------------------------------------------------
#pragma once

#include "file_system.h"
#include "common/utility/non_copyable.h"
#include <memory>
#include <optional>
#include <string>
#include <vector>

//===========================================================================
//! ファイルシステムマネージャー（シングルトン）
//!
//! マウントベースのファイルシステム抽象化を提供。
//!
//! @note 使用例:
//! @code
//!   // マウント
//!   FileSystemManager::Get().Mount("assets", std::make_unique<HostFileSystem>(L"C:/game/assets/"));
//!   FileSystemManager::Get().Mount("shaders", std::make_unique<HostFileSystem>(L"C:/game/shaders/"));
//!
//!   // ファイル読み込み
//!   auto data = FileSystemManager::Get().ReadFile("assets:/texture.png");
//!   auto text = FileSystemManager::Get().ReadFileAsText("shaders:/vs.hlsl");
//!
//!   // 終了
//!   FileSystemManager::Get().UnmountAll();
//! @endcode
//===========================================================================
class FileSystemManager final : private NonCopyableNonMovable
{
public:
    //! シングルトンインスタンスを取得
    static FileSystemManager& Get() noexcept;

    //----------------------------------------------------------
    //! @name   パスユーティリティ（static）
    //----------------------------------------------------------
    //!@{

    //! 実行ファイルのあるディレクトリを取得（末尾に/付き）
    [[nodiscard]] static std::wstring GetExecutableDirectory();

    //! プロジェクトルートを取得（末尾に/付き）
    //! @note 実行ファイルから相対パス（../../../../）で取得
    [[nodiscard]] static std::wstring GetProjectRoot();

    //! アセットディレクトリを取得（末尾に/付き）
    [[nodiscard]] static std::wstring GetAssetsDirectory();

    //! ディレクトリを作成（親ディレクトリも含めて）
    //! @return 作成成功または既に存在する場合 true
    static bool CreateDirectories(const std::wstring& path);

    //!@}

    //----------------------------------------------------------
    //! @name   マウント操作
    //----------------------------------------------------------
    //!@{

    //! ファイルシステムをマウント
    bool Mount(const std::string& name, std::unique_ptr<IReadableFileSystem> fileSystem);

    //! アンマウント
    void Unmount(const std::string& name);

    //! 全てアンマウント
    void UnmountAll();

    //! マウント済みか確認
    [[nodiscard]] bool IsMounted(const std::string& name) const;

    //!@}
    //----------------------------------------------------------
    //! @name   ファイルシステム取得
    //----------------------------------------------------------
    //!@{

    [[nodiscard]] IReadableFileSystem* GetFileSystem(const std::string& name);
    [[nodiscard]] IWritableFileSystem* GetWritableFileSystem(const std::string& name);

    //!@}
    //----------------------------------------------------------
    //! @name   パス解決
    //----------------------------------------------------------
    //!@{

    struct ResolvedPath {
        IReadableFileSystem* fileSystem;
        std::string relativePath;
    };

    [[nodiscard]] std::optional<ResolvedPath> ResolvePath(const std::string& mountPath);

    //!@}
    //----------------------------------------------------------
    //! @name   ファイル操作
    //----------------------------------------------------------
    //!@{

    [[nodiscard]] FileReadResult ReadFile(const std::string& mountPath);
    [[nodiscard]] std::string ReadFileAsText(const std::string& mountPath);
    [[nodiscard]] std::vector<char> ReadFileAsChars(const std::string& mountPath);
    [[nodiscard]] bool Exists(const std::string& mountPath);
    [[nodiscard]] int64_t GetFileSize(const std::string& mountPath);

    //!@}

private:
    FileSystemManager() = default;
    ~FileSystemManager() = default;

    struct MountPoint {
        std::string name;
        std::shared_ptr<IReadableFileSystem> fileSystem;
    };

    std::vector<MountPoint> mounts_;

    [[nodiscard]] std::shared_ptr<IReadableFileSystem> GetFileSystemSafe(const std::string& name);
};
