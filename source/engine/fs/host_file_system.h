#pragma once

#include "file_system.h"
#include <string>


//! ホストPCファイルシステム実装
class HostFileSystem : public IWritableFileSystem {
public:
    //! コンストラクタ
    //! @param [in] rootPath ルートパス（例: L"C:/Game/assets/"）
    explicit HostFileSystem(const std::wstring& rootPath);

    // IReadableFileSystem実装
    std::unique_ptr<IFileHandle> open(const std::string& path) noexcept override;
    FileReadResult read(const std::string& path) noexcept override;
    bool exists(const std::string& path) const noexcept override;
    int64_t getFileSize(const std::string& path) const noexcept override;
    bool isFile(const std::string& path) const noexcept override;
    bool isDirectory(const std::string& path) const noexcept override;
    int64_t getFreeSpaceSize() const noexcept override;
    int64_t getLastWriteTime(const std::string& path) const noexcept override;
    FileOperationResult createFile(const std::string& path, int64_t size) noexcept override;
    FileOperationResult deleteFile(const std::string& path) noexcept override;
    FileOperationResult renameFile(const std::string& oldPath, const std::string& newPath) noexcept override;
    FileOperationResult writeFile(const std::string& path, std::span<const std::byte> data) noexcept override;
    FileOperationResult createDirectory(const std::string& path) noexcept override;
    FileOperationResult deleteDirectory(const std::string& path) noexcept override;
    FileOperationResult deleteDirectoryRecursively(const std::string& path) noexcept override;
    FileOperationResult renameDirectory(const std::string& oldPath, const std::string& newPath) noexcept override;
    std::vector<DirectoryEntry> listDirectory(const std::string& path) const noexcept override;

private:
    std::wstring rootPath_;  //!< ルートパス

    //! 相対パスを絶対パスに変換
    [[nodiscard]] std::wstring toAbsolutePath(const std::string& relativePath) const noexcept;

    //! WindowsエラーコードをFileErrorに変換
    [[nodiscard]] static FileError makeError(FileError::Code code) noexcept;
    [[nodiscard]] static FileError makeErrorFromLastError() noexcept;
};

