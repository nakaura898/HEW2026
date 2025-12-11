#include "host_file_system.h"

#include <Windows.h>

#include <algorithm>
#include <filesystem>
#include <fstream>


namespace {

//! UTF-8 std::stringをstd::wstringに変換
std::wstring string_to_wstring(const std::string& str) {
    if (str.empty()) {
        return L"";
    }
    int size_needed = ::MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    ::MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

//! std::wstringをUTF-8 std::stringに変換
std::string wstring_to_string(const std::wstring& wstr) {
    if (wstr.empty()) {
        return "";
    }
    int size_needed = ::WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
    std::string strTo(size_needed, 0);
    ::WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
    return strTo;
}

} // namespace


//==============================================================================
// HostFileHandle
//==============================================================================
class HostFileHandle : public IFileHandle {
public:
    explicit HostFileHandle(HANDLE hFile, int64_t fileSize) noexcept
        : hFile_(hFile), fileSize_(fileSize) {}

    ~HostFileHandle() override {
        if (hFile_ != INVALID_HANDLE_VALUE) {
            ::CloseHandle(hFile_);
        }
    }

    // コピー禁止
    HostFileHandle(const HostFileHandle&) = delete;
    HostFileHandle& operator=(const HostFileHandle&) = delete;

    FileReadResult read(size_t size) noexcept override {
        FileReadResult result;

        if (hFile_ == INVALID_HANDLE_VALUE) {
            result.error = FileError::make(FileError::Code::InvalidPath, 0, "Invalid file handle");
            return result;
        }

        result.bytes.resize(size);
        size_t totalRead = 0;
        std::byte* dest = result.bytes.data();

        constexpr DWORD chunkSize = 0x40000000; // 1GB
        while (totalRead < size) {
            DWORD toRead = static_cast<DWORD>(std::min<size_t>(size - totalRead, chunkSize));
            DWORD bytesRead = 0;
            if (!::ReadFile(hFile_, dest, toRead, &bytesRead, nullptr)) {
                result.error = FileError::make(FileError::Code::Unknown, static_cast<int32_t>(::GetLastError()), "Failed to read file");
                result.bytes.clear();
                return result;
            }
            if (bytesRead == 0) break; // EOF
            dest += bytesRead;
            totalRead += bytesRead;
        }

        result.bytes.resize(totalRead);
        result.success = true;
        return result;
    }

    bool seek(int64_t offset, SeekOrigin origin) noexcept override {
        if (hFile_ == INVALID_HANDLE_VALUE) return false;

        DWORD moveMethod;
        switch (origin) {
        case SeekOrigin::Begin:   moveMethod = FILE_BEGIN; break;
        case SeekOrigin::Current: moveMethod = FILE_CURRENT; break;
        case SeekOrigin::End:     moveMethod = FILE_END; break;
        default: return false;
        }

        LARGE_INTEGER li;
        li.QuadPart = offset;
        return ::SetFilePointerEx(hFile_, li, nullptr, moveMethod) != 0;
    }

    int64_t tell() const noexcept override {
        if (hFile_ == INVALID_HANDLE_VALUE) return -1;

        LARGE_INTEGER zero = {};
        LARGE_INTEGER pos;
        if (!::SetFilePointerEx(hFile_, zero, &pos, FILE_CURRENT)) {
            return -1;
        }
        return pos.QuadPart;
    }

    int64_t size() const noexcept override {
        return fileSize_;
    }

    bool isEof() const noexcept override {
        return tell() >= fileSize_;
    }

    bool isValid() const noexcept override {
        return hFile_ != INVALID_HANDLE_VALUE;
    }

private:
    HANDLE hFile_ = INVALID_HANDLE_VALUE;
    int64_t fileSize_ = 0;
};

HostFileSystem::HostFileSystem(const std::wstring& rootPath)
    : rootPath_(rootPath) {
    // 末尾にスラッシュがなければ追加
    if (!rootPath_.empty() && rootPath_.back() != L'/' && rootPath_.back() != L'\\') {
        rootPath_ += L'/';
    }
}

std::wstring HostFileSystem::toAbsolutePath(const std::string& relativePath) const noexcept {
    // パスを正規化（スラッシュ統一など）
    // NOTE: ここでPathUtility::normalizeを呼ぶべきかもしれないが、
    //       現状は呼び出し元で正規化されている想定
    return rootPath_ + string_to_wstring(relativePath);
}

FileError HostFileSystem::makeError(FileError::Code code) noexcept {
    return FileError{ code, 0, {} };
}

FileError HostFileSystem::makeErrorFromLastError() noexcept {
    DWORD err = ::GetLastError();
    FileError::Code code = FileError::Code::Unknown;

    switch (err) {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
        code = FileError::Code::NotFound;
        break;
    case ERROR_ACCESS_DENIED:
        code = FileError::Code::AccessDenied;
        break;
    case ERROR_ALREADY_EXISTS:
    case ERROR_FILE_EXISTS:
        code = FileError::Code::AlreadyExists;
        break;
    case ERROR_DISK_FULL:
        code = FileError::Code::DiskFull;
        break;
    case ERROR_DIR_NOT_EMPTY:
        code = FileError::Code::NotEmpty;
        break;
    case ERROR_INVALID_NAME:
    case ERROR_BAD_PATHNAME:
        code = FileError::Code::InvalidPath;
        break;
    default:
        code = FileError::Code::Unknown;
        break;
    }

    return FileError{ code, static_cast<int32_t>(err), {} };
}

std::unique_ptr<IFileHandle> HostFileSystem::open(const std::string& path) noexcept {
    std::wstring fullPath = toAbsolutePath(path);

    HANDLE hFile = ::CreateFileW(
        fullPath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (hFile == INVALID_HANDLE_VALUE) {
        return nullptr;
    }

    LARGE_INTEGER fileSize;
    if (!::GetFileSizeEx(hFile, &fileSize)) {
        ::CloseHandle(hFile);
        return nullptr;
    }

    return std::make_unique<HostFileHandle>(hFile, fileSize.QuadPart);
}

FileReadResult HostFileSystem::read(const std::string& path) noexcept {
    FileReadResult result;
    std::wstring fullPath = toAbsolutePath(path);

    // ファイルを開く
    HANDLE hFile = ::CreateFileW(
        fullPath.c_str(),
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (hFile == INVALID_HANDLE_VALUE) {
        result.error = makeErrorFromLastError();
        result.error.context = path;
        return result;
    }

    // ファイルサイズ取得
    LARGE_INTEGER fileSize;
    if (!::GetFileSizeEx(hFile, &fileSize)) {
        result.error = makeErrorFromLastError();
        result.error.context = path;
        ::CloseHandle(hFile);
        return result;
    }

    // データ読み込み（4GB超対応：チャンク分割）
    const size_t totalSize = static_cast<size_t>(fileSize.QuadPart);
    result.bytes.resize(totalSize);

    constexpr DWORD chunkSize = 0x40000000; // 1GB単位
    size_t remaining = totalSize;
    std::byte* dest = result.bytes.data();

    while (remaining > 0) {
        DWORD toRead = static_cast<DWORD>(std::min<size_t>(remaining, chunkSize));
        DWORD bytesRead = 0;
        if (!::ReadFile(hFile, dest, toRead, &bytesRead, nullptr)) {
            result.error = makeErrorFromLastError();
            result.error.context = path;
            result.bytes.clear();
            ::CloseHandle(hFile);
            return result;
        }
        dest += bytesRead;
        remaining -= bytesRead;
    }

    ::CloseHandle(hFile);
    result.success = true;
    return result;
}

bool HostFileSystem::exists(const std::string& path) const noexcept {
    std::wstring fullPath = toAbsolutePath(path);
    DWORD attr = ::GetFileAttributesW(fullPath.c_str());
    return attr != INVALID_FILE_ATTRIBUTES;
}

int64_t HostFileSystem::getFileSize(const std::string& path) const noexcept {
    std::wstring fullPath = toAbsolutePath(path);

    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!::GetFileAttributesExW(fullPath.c_str(), GetFileExInfoStandard, &fad)) {
        return -1;
    }

    LARGE_INTEGER size;
    size.HighPart = fad.nFileSizeHigh;
    size.LowPart = fad.nFileSizeLow;
    return size.QuadPart;
}

bool HostFileSystem::isFile(const std::string& path) const noexcept {
    std::wstring fullPath = toAbsolutePath(path);
    DWORD attr = ::GetFileAttributesW(fullPath.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) return false;
    return (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

bool HostFileSystem::isDirectory(const std::string& path) const noexcept {
    std::wstring fullPath = toAbsolutePath(path);
    DWORD attr = ::GetFileAttributesW(fullPath.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES) return false;
    return (attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

int64_t HostFileSystem::getFreeSpaceSize() const noexcept {
    ULARGE_INTEGER freeBytesAvailable;
    if (!::GetDiskFreeSpaceExW(rootPath_.c_str(), &freeBytesAvailable, nullptr, nullptr)) {
        return -1;
    }
    return static_cast<int64_t>(freeBytesAvailable.QuadPart);
}

int64_t HostFileSystem::getLastWriteTime(const std::string& path) const noexcept {
    std::wstring fullPath = toAbsolutePath(path);

    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!::GetFileAttributesExW(fullPath.c_str(), GetFileExInfoStandard, &fad)) {
        return -1;
    }

    // FILETIMEをUnix時間に変換
    ULARGE_INTEGER ull;
    ull.LowPart = fad.ftLastWriteTime.dwLowDateTime;
    ull.HighPart = fad.ftLastWriteTime.dwHighDateTime;
    // FILETIMEは1601年1月1日からの100ナノ秒単位
    // Unix時間は1970年1月1日からの秒単位
    constexpr int64_t FILETIME_UNIX_DIFF = 116444736000000000LL;
    return static_cast<int64_t>((ull.QuadPart - FILETIME_UNIX_DIFF) / 10000000LL);
}

FileOperationResult HostFileSystem::createFile(const std::string& path, int64_t size) noexcept {
    FileOperationResult result;
    std::wstring fullPath = toAbsolutePath(path);

    HANDLE hFile = ::CreateFileW(
        fullPath.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_NEW,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (hFile == INVALID_HANDLE_VALUE) {
        result.error = makeErrorFromLastError();
        result.error.context = path;
        return result;
    }

    // サイズ指定がある場合はファイルサイズを設定
    if (size > 0) {
        LARGE_INTEGER li;
        li.QuadPart = size;
        if (!::SetFilePointerEx(hFile, li, nullptr, FILE_BEGIN) ||
            !::SetEndOfFile(hFile)) {
            result.error = makeErrorFromLastError();
            result.error.context = path;
            ::CloseHandle(hFile);
            return result;
        }
    }

    ::CloseHandle(hFile);
    result.success = true;
    return result;
}

FileOperationResult HostFileSystem::deleteFile(const std::string& path) noexcept {
    FileOperationResult result;
    std::wstring fullPath = toAbsolutePath(path);

    if (!::DeleteFileW(fullPath.c_str())) {
        result.error = makeErrorFromLastError();
        result.error.context = path;
        return result;
    }

    result.success = true;
    return result;
}

FileOperationResult HostFileSystem::renameFile(const std::string& oldPath, const std::string& newPath) noexcept {
    FileOperationResult result;
    std::wstring fullOldPath = toAbsolutePath(oldPath);
    std::wstring fullNewPath = toAbsolutePath(newPath);

    if (!::MoveFileW(fullOldPath.c_str(), fullNewPath.c_str())) {
        result.error = makeErrorFromLastError();
        result.error.context = oldPath + " -> " + newPath;
        return result;
    }

    result.success = true;
    return result;
}

FileOperationResult HostFileSystem::writeFile(const std::string& path, std::span<const std::byte> data) noexcept {
    FileOperationResult result;
    std::wstring fullPath = toAbsolutePath(path);

    HANDLE hFile = ::CreateFileW(
        fullPath.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);

    if (hFile == INVALID_HANDLE_VALUE) {
        result.error = makeErrorFromLastError();
        result.error.context = path;
        return result;
    }

    // 4GB超対応：チャンク分割
    const size_t totalSize = data.size();
    size_t remaining = totalSize;
    const std::byte* src = data.data();

    constexpr DWORD chunkSize = 0x40000000; // 1GB単位

    while (remaining > 0) {
        DWORD toWrite = static_cast<DWORD>(std::min<size_t>(remaining, chunkSize));
        DWORD bytesWritten = 0;
        if (!::WriteFile(hFile, src, toWrite, &bytesWritten, nullptr) || bytesWritten != toWrite) {
            result.error = makeErrorFromLastError();
            result.error.context = path;
            ::CloseHandle(hFile);
            return result;
        }
        src += bytesWritten;
        remaining -= bytesWritten;
    }

    ::CloseHandle(hFile);
    result.success = true;
    return result;
}

FileOperationResult HostFileSystem::createDirectory(const std::string& path) noexcept {
    FileOperationResult result;
    std::wstring fullPath = toAbsolutePath(path);

    if (!::CreateDirectoryW(fullPath.c_str(), nullptr)) {
        result.error = makeErrorFromLastError();
        result.error.context = path;
        return result;
    }

    result.success = true;
    return result;
}

FileOperationResult HostFileSystem::deleteDirectory(const std::string& path) noexcept {
    FileOperationResult result;
    std::wstring fullPath = toAbsolutePath(path);

    if (!::RemoveDirectoryW(fullPath.c_str())) {
        result.error = makeErrorFromLastError();
        result.error.context = path;
        return result;
    }

    result.success = true;
    return result;
}

FileOperationResult HostFileSystem::deleteDirectoryRecursively(const std::string& path) noexcept {
    FileOperationResult result;
    std::wstring fullPath = toAbsolutePath(path);

    try {
        std::filesystem::remove_all(fullPath);
        result.success = true;
    } catch (...) {
        result.error = FileError::make(FileError::Code::Unknown, 0, path);
    }

    return result;
}

FileOperationResult HostFileSystem::renameDirectory(const std::string& oldPath, const std::string& newPath) noexcept {
    // ファイルと同じ処理
    return renameFile(oldPath, newPath);
}

std::vector<DirectoryEntry> HostFileSystem::listDirectory(const std::string& path) const noexcept {
    std::vector<DirectoryEntry> entries;
    std::wstring fullPath = toAbsolutePath(path);

    // 末尾にワイルドカード追加
    if (!fullPath.empty() && fullPath.back() != L'/' && fullPath.back() != L'\\') {
        fullPath += L'/';
    }
    fullPath += L'*';

    WIN32_FIND_DATAW findData;
    HANDLE hFind = ::FindFirstFileW(fullPath.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        return entries;
    }

    do {
        // "." と ".." をスキップ
        if (wcscmp(findData.cFileName, L".") == 0 || wcscmp(findData.cFileName, L"..") == 0) {
            continue;
        }

        DirectoryEntry entry;
        entry.name = wstring_to_string(findData.cFileName);

        entry.type = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            ? FileEntryType::Directory
            : FileEntryType::File;

        if (entry.type == FileEntryType::File) {
            LARGE_INTEGER size;
            size.HighPart = findData.nFileSizeHigh;
            size.LowPart = findData.nFileSizeLow;
            entry.size = size.QuadPart;
        }

        entries.push_back(std::move(entry));
    } while (::FindNextFileW(hFind, &findData));

    ::FindClose(hFind);
    return entries;
}

