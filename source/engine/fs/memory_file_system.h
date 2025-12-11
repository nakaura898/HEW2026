#pragma once

#include "file_system.h"
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>


//! メモリベースファイルシステム（テスト・埋め込みリソース用）
//! 読み取り専用。ファイル追加はaddFile/addTextFileで行う。
//!
//! @note スレッドセーフ: 読み取りと書き込みは排他制御されます。
//!       ファイルハンドルはデータのshared_ptrを保持するため、
//!       ハンドル使用中にファイルが削除/更新されても安全です。
class MemoryFileSystem : public IReadableFileSystem {
public:
    //----------------------------------------------------------
    //! @name   IFileSystem実装
    //----------------------------------------------------------

    bool exists(const std::string& path) const noexcept override;
    int64_t getFileSize(const std::string& path) const noexcept override;
    bool isFile(const std::string& path) const noexcept override;
    bool isDirectory(const std::string& path) const noexcept override;
    int64_t getFreeSpaceSize() const noexcept override;
    int64_t getLastWriteTime(const std::string& path) const noexcept override;

    //----------------------------------------------------------
    //! @name   IReadableFileSystem実装
    //----------------------------------------------------------

    std::unique_ptr<IFileHandle> open(const std::string& path) noexcept override;
    FileReadResult read(const std::string& path) noexcept override;
    std::vector<DirectoryEntry> listDirectory(const std::string& path) const noexcept override;

    //----------------------------------------------------------
    //! @name   メモリファイルシステム固有（セットアップ用）
    //----------------------------------------------------------

    //! ファイルを追加
    void addFile(const std::string& path, std::vector<std::byte> data);

    //! テキストファイルを追加
    void addTextFile(const std::string& path, const std::string& text);

    //! 全ファイルをクリア
    void clear();

private:
    //! ファイルデータをshared_ptrで保持（ハンドルとの共有所有権）
    using FileData = std::shared_ptr<std::vector<std::byte>>;

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, FileData> files_;
};

