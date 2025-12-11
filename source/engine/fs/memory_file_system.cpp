//----------------------------------------------------------------------------
//! @file   memory_file_system.cpp
//! @brief  メモリベースファイルシステム実装
//----------------------------------------------------------------------------
#include "memory_file_system.h"
#include "path_utility.h"
#include <algorithm>
#include <cstring>


//==============================================================================
// MemoryFileHandle
//==============================================================================
class MemoryFileHandle : public IFileHandle {
public:
    //! コンストラクタ
    //! @param [in] data ファイルデータ（shared_ptrで共有所有）
    explicit MemoryFileHandle(std::shared_ptr<std::vector<std::byte>> data) noexcept
        : data_(std::move(data)) {}

    FileReadResult read(size_t size) noexcept override {
        FileReadResult result;

        if (!data_) {
            result.error = FileError::make(FileError::Code::InvalidPath, 0, "Invalid file handle");
            return result;
        }

        size_t available = data_->size() - static_cast<size_t>(position_);
        size_t toRead = std::min(size, available);

        result.bytes.resize(toRead);
        if (toRead > 0) {
            std::memcpy(result.bytes.data(), data_->data() + position_, toRead);
            position_ += static_cast<int64_t>(toRead);
        }

        result.success = true;
        return result;
    }

    bool seek(int64_t offset, SeekOrigin origin) noexcept override {
        if (!data_) return false;

        int64_t newPos;
        switch (origin) {
        case SeekOrigin::Begin:
            newPos = offset;
            break;
        case SeekOrigin::Current:
            newPos = position_ + offset;
            break;
        case SeekOrigin::End:
            newPos = static_cast<int64_t>(data_->size()) + offset;
            break;
        default:
            return false;
        }

        if (newPos < 0 || newPos > static_cast<int64_t>(data_->size())) {
            return false;
        }

        position_ = newPos;
        return true;
    }

    int64_t tell() const noexcept override {
        return position_;
    }

    int64_t size() const noexcept override {
        return data_ ? static_cast<int64_t>(data_->size()) : 0;
    }

    bool isEof() const noexcept override {
        return !data_ || position_ >= static_cast<int64_t>(data_->size());
    }

    bool isValid() const noexcept override {
        return data_ != nullptr;
    }

private:
    std::shared_ptr<std::vector<std::byte>> data_;  //!< 共有所有権でデータ保持
    int64_t position_ = 0;
};

//==============================================================================
// MemoryFileSystem
//==============================================================================

std::unique_ptr<IFileHandle> MemoryFileSystem::open(const std::string& path) noexcept {
    std::shared_lock lock(mutex_);

    auto normalizedPath = PathUtility::normalize(path);
    auto it = files_.find(normalizedPath);
    if (it == files_.end()) {
        return nullptr;
    }
    // shared_ptrをコピーしてハンドルに渡す（共有所有権）
    return std::make_unique<MemoryFileHandle>(it->second);
}

FileReadResult MemoryFileSystem::read(const std::string& path) noexcept {
    std::shared_lock lock(mutex_);

    FileReadResult result;
    auto normalizedPath = PathUtility::normalize(path);

    auto it = files_.find(normalizedPath);
    if (it == files_.end()) {
        result.error = FileError::make(FileError::Code::NotFound, 0, path);
        return result;
    }

    // データをコピー（スレッドセーフ）
    result.bytes = *it->second;
    result.success = true;
    return result;
}

bool MemoryFileSystem::exists(const std::string& path) const noexcept {
    std::shared_lock lock(mutex_);

    auto normalizedPath = PathUtility::normalize(path);
    return files_.find(normalizedPath) != files_.end();
}

int64_t MemoryFileSystem::getFileSize(const std::string& path) const noexcept {
    std::shared_lock lock(mutex_);

    auto normalizedPath = PathUtility::normalize(path);
    auto it = files_.find(normalizedPath);
    if (it == files_.end()) return -1;
    return static_cast<int64_t>(it->second->size());
}

bool MemoryFileSystem::isFile(const std::string& path) const noexcept {
    std::shared_lock lock(mutex_);

    auto normalizedPath = PathUtility::normalize(path);
    return files_.find(normalizedPath) != files_.end();
}

bool MemoryFileSystem::isDirectory([[maybe_unused]] const std::string& path) const noexcept {
    // メモリファイルシステムはディレクトリをサポートしない
    return false;
}

int64_t MemoryFileSystem::getFreeSpaceSize() const noexcept {
    // メモリ制限なし（実際はシステムメモリに依存）
    return INT64_MAX;
}

int64_t MemoryFileSystem::getLastWriteTime([[maybe_unused]] const std::string& path) const noexcept {
    // タイムスタンプ未サポート
    return -1;
}

std::vector<DirectoryEntry> MemoryFileSystem::listDirectory([[maybe_unused]] const std::string& path) const noexcept {
    // メモリファイルシステムはディレクトリをサポートしない
    return {};
}

void MemoryFileSystem::addFile(const std::string& path, std::vector<std::byte> data) {
    std::unique_lock lock(mutex_);

    auto normalizedPath = PathUtility::normalize(path);
    files_[normalizedPath] = std::make_shared<std::vector<std::byte>>(std::move(data));
}

void MemoryFileSystem::addTextFile(const std::string& path, const std::string& text) {
    std::unique_lock lock(mutex_);

    auto normalizedPath = PathUtility::normalize(path);
    auto data = std::make_shared<std::vector<std::byte>>(text.size());
    std::memcpy(data->data(), text.data(), text.size());
    files_[normalizedPath] = std::move(data);
}

void MemoryFileSystem::clear() {
    std::unique_lock lock(mutex_);
    files_.clear();
}
