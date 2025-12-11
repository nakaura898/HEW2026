//----------------------------------------------------------------------------
//! @file   file_system_manager.cpp
//! @brief  ファイルシステムマネージャー実装
//----------------------------------------------------------------------------
#include "file_system_manager.h"
#include "file_system_types.h"
#include <algorithm>

//============================================================================
// ユーティリティ関数
//============================================================================
namespace
{
    struct ParsedPath {
        std::string mountName;
        std::string relativePath;
    };

    std::optional<ParsedPath> ParseMountPath(const std::string& mountPath) {
        auto colonPos = mountPath.find(":/");
        if (colonPos == std::string::npos || colonPos == 0) {
            return std::nullopt;
        }
        return ParsedPath{
            mountPath.substr(0, colonPos),
            (colonPos + 2 <= mountPath.size()) ? mountPath.substr(colonPos + 2) : std::string{}
        };
    }
} // namespace

//============================================================================
// FileSystemManager 実装
//============================================================================
FileSystemManager& FileSystemManager::Get() noexcept
{
    static FileSystemManager instance;
    return instance;
}

bool FileSystemManager::Mount(const std::string& name, std::unique_ptr<IReadableFileSystem> fileSystem)
{
    if (!fileSystem) return false;
    if (name.empty() || name.size() > MountNameLengthMax) return false;

    bool alreadyMounted = std::any_of(mounts_.begin(), mounts_.end(),
        [&name](const MountPoint& mp) { return mp.name == name; });
    if (alreadyMounted) return false;

    // unique_ptrからshared_ptrに変換して保存
    mounts_.push_back({ name, std::shared_ptr<IReadableFileSystem>(std::move(fileSystem)) });
    return true;
}

void FileSystemManager::Unmount(const std::string& name)
{
    auto it = std::find_if(mounts_.begin(), mounts_.end(),
        [&name](const MountPoint& mp) { return mp.name == name; });
    if (it != mounts_.end()) {
        mounts_.erase(it);
    }
}

void FileSystemManager::UnmountAll()
{
    mounts_.clear();
}

bool FileSystemManager::IsMounted(const std::string& name) const
{
    return std::any_of(mounts_.begin(), mounts_.end(),
        [&name](const MountPoint& mp) { return mp.name == name; });
}

IReadableFileSystem* FileSystemManager::GetFileSystem(const std::string& name)
{
    auto it = std::find_if(mounts_.begin(), mounts_.end(),
        [&name](const MountPoint& mp) { return mp.name == name; });
    return it != mounts_.end() ? it->fileSystem.get() : nullptr;
}

IWritableFileSystem* FileSystemManager::GetWritableFileSystem(const std::string& name)
{
    auto it = std::find_if(mounts_.begin(), mounts_.end(),
        [&name](const MountPoint& mp) { return mp.name == name; });
    if (it == mounts_.end()) return nullptr;
    return dynamic_cast<IWritableFileSystem*>(it->fileSystem.get());
}

std::shared_ptr<IReadableFileSystem> FileSystemManager::GetFileSystemSafe(const std::string& name)
{
    auto it = std::find_if(mounts_.begin(), mounts_.end(),
        [&name](const MountPoint& mp) { return mp.name == name; });
    return it != mounts_.end() ? it->fileSystem : nullptr;
}

std::optional<FileSystemManager::ResolvedPath> FileSystemManager::ResolvePath(const std::string& mountPath)
{
    auto colonPos = mountPath.find(":/");
    if (colonPos == std::string::npos || colonPos == 0) {
        return std::nullopt;
    }

    std::string mountName = mountPath.substr(0, colonPos);
    std::string relativePath = (colonPos + 2 <= mountPath.size())
        ? mountPath.substr(colonPos + 2)
        : std::string{};

    auto it = std::find_if(mounts_.begin(), mounts_.end(),
        [&mountName](const MountPoint& mp) { return mp.name == mountName; });
    if (it == mounts_.end()) {
        return std::nullopt;
    }

    return ResolvedPath{ it->fileSystem.get(), relativePath };
}

FileReadResult FileSystemManager::ReadFile(const std::string& mountPath)
{
    auto parsed = ParseMountPath(mountPath);
    if (!parsed) {
        FileReadResult result;
        result.error = FileError::make(FileError::Code::InvalidMount, 0, mountPath);
        return result;
    }

    auto fs = GetFileSystemSafe(parsed->mountName);
    if (!fs) {
        FileReadResult result;
        result.error = FileError::make(FileError::Code::InvalidMount, 0, mountPath);
        return result;
    }

    return fs->read(parsed->relativePath);
}

std::string FileSystemManager::ReadFileAsText(const std::string& mountPath)
{
    auto parsed = ParseMountPath(mountPath);
    if (!parsed) return {};

    auto fs = GetFileSystemSafe(parsed->mountName);
    if (!fs) return {};

    return fs->readAsText(parsed->relativePath);
}

std::vector<char> FileSystemManager::ReadFileAsChars(const std::string& mountPath)
{
    auto parsed = ParseMountPath(mountPath);
    if (!parsed) return {};

    auto fs = GetFileSystemSafe(parsed->mountName);
    if (!fs) return {};

    return fs->readAsChars(parsed->relativePath);
}

bool FileSystemManager::Exists(const std::string& mountPath)
{
    auto parsed = ParseMountPath(mountPath);
    if (!parsed) return false;

    auto fs = GetFileSystemSafe(parsed->mountName);
    if (!fs) return false;

    return fs->exists(parsed->relativePath);
}

int64_t FileSystemManager::GetFileSize(const std::string& mountPath)
{
    auto parsed = ParseMountPath(mountPath);
    if (!parsed) return -1;

    auto fs = GetFileSystemSafe(parsed->mountName);
    if (!fs) return -1;

    return fs->getFileSize(parsed->relativePath);
}
