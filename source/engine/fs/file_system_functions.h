//----------------------------------------------------------------------------
//! @file   file_system_functions.h
//! @brief  ファイルシステム便利関数
//----------------------------------------------------------------------------
#pragma once

#include "file_system_manager.h"
#include "host_file_system.h"
#include "memory_file_system.h"

//----------------------------------------------------------
//! @name   マウント便利関数
//----------------------------------------------------------

inline bool MountHostFileSystem(const char* name, const std::wstring& rootPath) {
    return FileSystemManager::Get().Mount(name, std::make_unique<HostFileSystem>(rootPath));
}

inline bool MountMemoryFileSystem(const char* name) {
    return FileSystemManager::Get().Mount(name, std::make_unique<MemoryFileSystem>());
}

inline void UnmountFileSystem(const char* name) {
    FileSystemManager::Get().Unmount(name);
}

inline void UnmountAllFileSystems() {
    FileSystemManager::Get().UnmountAll();
}

//----------------------------------------------------------
//! @name   ファイル操作便利関数
//----------------------------------------------------------

inline FileReadResult ReadFile(const char* mountPath) {
    return FileSystemManager::Get().ReadFile(mountPath);
}

inline std::string ReadFileAsText(const char* mountPath) {
    return FileSystemManager::Get().ReadFileAsText(mountPath);
}

inline std::vector<char> ReadFileAsChars(const char* mountPath) {
    return FileSystemManager::Get().ReadFileAsChars(mountPath);
}

inline bool FileExists(const char* mountPath) {
    return FileSystemManager::Get().Exists(mountPath);
}

inline int64_t GetFileSize(const char* mountPath) {
    return FileSystemManager::Get().GetFileSize(mountPath);
}
