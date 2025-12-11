#include "file_watcher.h"
#include "path_utility.h"

#include <Windows.h>

#include <algorithm>
#include <atomic>
#include <mutex>
#include <queue>
#include <thread>


//==============================================================================
// FileWatcher::Impl
//==============================================================================
class FileWatcher::Impl {
public:
    Impl() = default;

    ~Impl() {
        stop();
    }

    bool start(const std::wstring& directoryPath, bool recursive, FileChangeCallback callback) {
        if (watching_.load()) {
            return false; // 既に監視中
        }

        // ディレクトリハンドルを開く
        hDirectory_ = ::CreateFileW(
            directoryPath.c_str(),
            FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
            nullptr);

        if (hDirectory_ == INVALID_HANDLE_VALUE) {
            return false;
        }

        // イベントハンドル作成
        hEvent_ = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
        if (hEvent_ == nullptr) {
            ::CloseHandle(hDirectory_);
            hDirectory_ = INVALID_HANDLE_VALUE;
            return false;
        }

        watchPath_ = directoryPath;
        recursive_ = recursive;
        callback_ = std::move(callback);
        watching_.store(true);

        // 監視スレッド開始
        watchThread_ = std::thread(&Impl::watchThreadFunc, this);

        return true;
    }

    void stop() {
        if (!watching_.load()) {
            return;
        }

        watching_.store(false);

        // イベントをシグナル状態にしてスレッドを起こす
        if (hEvent_ != nullptr) {
            ::SetEvent(hEvent_);
        }

        // スレッド終了待ち
        if (watchThread_.joinable()) {
            watchThread_.join();
        }

        // ハンドルクリーンアップ
        if (hEvent_ != nullptr) {
            ::CloseHandle(hEvent_);
            hEvent_ = nullptr;
        }
        if (hDirectory_ != INVALID_HANDLE_VALUE) {
            ::CloseHandle(hDirectory_);
            hDirectory_ = INVALID_HANDLE_VALUE;
        }

        // イベントキューをクリア
        std::lock_guard<std::mutex> lock(mutex_);
        while (!eventQueue_.empty()) {
            eventQueue_.pop();
        }
    }

    [[nodiscard]] bool isWatching() const noexcept {
        return watching_.load();
    }

    [[nodiscard]] const std::wstring& getWatchPath() const noexcept {
        return watchPath_;
    }

    size_t pollEvents() {
        std::vector<FileChangeEvent> events;

        // イベントを取り出し
        {
            std::lock_guard<std::mutex> lock(mutex_);
            while (!eventQueue_.empty()) {
                events.push_back(std::move(eventQueue_.front()));
                eventQueue_.pop();
            }
        }

        // コールバック呼び出し
        if (callback_) {
            for (const auto& event : events) {
                // 拡張子フィルターチェック
                if (!extensionFilter_.empty()) {
                    std::wstring ext = getExtensionW(event.path);
                    bool match = std::any_of(extensionFilter_.begin(), extensionFilter_.end(),
                        [&ext](const std::wstring& filter) {
                            return _wcsicmp(ext.c_str(), filter.c_str()) == 0;
                        });
                    if (!match) continue;
                }
                callback_(event);
            }
        }

        return events.size();
    }

    void setExtensionFilter(const std::vector<std::wstring>& extensions) {
        std::lock_guard<std::mutex> lock(mutex_);
        extensionFilter_ = extensions;
    }

private:
    void watchThreadFunc() {
        constexpr DWORD bufferSize = 64 * 1024; // 64KB
        std::vector<BYTE> buffer(bufferSize);

        OVERLAPPED overlapped = {};
        overlapped.hEvent = hEvent_;

        while (watching_.load()) {
            ::ResetEvent(hEvent_);

            DWORD bytesReturned = 0;
            BOOL result = ::ReadDirectoryChangesW(
                hDirectory_,
                buffer.data(),
                bufferSize,
                recursive_ ? TRUE : FALSE,
                FILE_NOTIFY_CHANGE_FILE_NAME |
                FILE_NOTIFY_CHANGE_DIR_NAME |
                FILE_NOTIFY_CHANGE_LAST_WRITE |
                FILE_NOTIFY_CHANGE_CREATION,
                &bytesReturned,
                &overlapped,
                nullptr);

            if (!result) {
                DWORD err = ::GetLastError();
                if (err != ERROR_IO_PENDING) {
                    break;
                }
            }

            // 完了またはシャットダウンを待つ
            DWORD waitResult = ::WaitForSingleObject(hEvent_, INFINITE);

            if (!watching_.load()) {
                // シャットダウン要求
                ::CancelIoEx(hDirectory_, &overlapped);
                break;
            }

            if (waitResult == WAIT_OBJECT_0) {
                if (!::GetOverlappedResult(hDirectory_, &overlapped, &bytesReturned, FALSE)) {
                    continue;
                }

                if (bytesReturned > 0) {
                    processNotifications(buffer.data(), bytesReturned);
                }
            }
        }
    }

    void processNotifications(const BYTE* buffer, [[maybe_unused]] DWORD size) {
        const FILE_NOTIFY_INFORMATION* info = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(buffer);
        std::wstring oldName; // リネーム時の旧名前保持用

        while (true) {
            // ファイル名を取得
            std::wstring fileName(info->FileName, info->FileNameLength / sizeof(WCHAR));
            std::wstring fullPath = watchPath_;
            if (!fullPath.empty() && fullPath.back() != L'/' && fullPath.back() != L'\\') {
                fullPath += L'\\';
            }
            fullPath += fileName;

            // パスを正規化
            fullPath = PathUtility::normalizeW(fullPath);

            FileChangeEvent event;
            event.path = fullPath;

            switch (info->Action) {
            case FILE_ACTION_ADDED:
                event.type = FileChangeType::Created;
                enqueueEvent(std::move(event));
                break;

            case FILE_ACTION_REMOVED:
                event.type = FileChangeType::Deleted;
                enqueueEvent(std::move(event));
                break;

            case FILE_ACTION_MODIFIED:
                event.type = FileChangeType::Modified;
                enqueueEvent(std::move(event));
                break;

            case FILE_ACTION_RENAMED_OLD_NAME:
                oldName = fullPath;
                break;

            case FILE_ACTION_RENAMED_NEW_NAME:
                event.type = FileChangeType::Renamed;
                event.oldPath = oldName;
                enqueueEvent(std::move(event));
                oldName.clear();
                break;
            }

            // 次のエントリへ
            if (info->NextEntryOffset == 0) {
                break;
            }
            info = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(
                reinterpret_cast<const BYTE*>(info) + info->NextEntryOffset);
        }
    }

    void enqueueEvent(FileChangeEvent&& event) {
        std::lock_guard<std::mutex> lock(mutex_);
        eventQueue_.push(std::move(event));
    }

    [[nodiscard]] static std::wstring getExtensionW(const std::wstring& path) {
        auto pos = path.rfind(L'.');
        if (pos == std::wstring::npos) return {};
        auto slashPos = path.find_last_of(L"/\\");
        if (slashPos != std::wstring::npos && pos < slashPos) return {};
        return path.substr(pos);
    }

private:
    HANDLE hDirectory_ = INVALID_HANDLE_VALUE;
    HANDLE hEvent_ = nullptr;
    std::wstring watchPath_;
    bool recursive_ = false;
    FileChangeCallback callback_;
    std::atomic<bool> watching_{false};
    std::thread watchThread_;
    std::mutex mutex_;
    std::queue<FileChangeEvent> eventQueue_;
    std::vector<std::wstring> extensionFilter_;
};

//==============================================================================
// FileWatcher
//==============================================================================
FileWatcher::FileWatcher()
    : impl_(std::make_unique<Impl>()) {
}

FileWatcher::~FileWatcher() = default;

FileWatcher::FileWatcher(FileWatcher&& other) noexcept = default;

FileWatcher& FileWatcher::operator=(FileWatcher&& other) noexcept = default;

bool FileWatcher::start(const std::wstring& directoryPath, bool recursive, FileChangeCallback callback) {
    return impl_->start(directoryPath, recursive, std::move(callback));
}

void FileWatcher::stop() {
    impl_->stop();
}

bool FileWatcher::isWatching() const noexcept {
    return impl_->isWatching();
}

const std::wstring& FileWatcher::getWatchPath() const noexcept {
    return impl_->getWatchPath();
}

size_t FileWatcher::pollEvents() {
    return impl_->pollEvents();
}

void FileWatcher::setExtensionFilter(const std::vector<std::wstring>& extensions) {
    impl_->setExtensionFilter(extensions);
}

