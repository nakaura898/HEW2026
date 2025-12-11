#pragma once

#include "file_error.h"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <vector>


//! マウント名の最大長（NULL終端を含まない）
inline constexpr int MountNameLengthMax = 15;

//! パスの最大長（NULL終端を含まない）
inline constexpr int PathLengthMax = 260;

//! ファイル読み込み結果
struct FileReadResult {
    bool success = false;             //!< 成功フラグ
    FileError error;                  //!< エラー情報
    std::vector<std::byte> bytes;     //!< ファイルデータ

    //! エラーメッセージを取得
    //! @return エラーメッセージ（error.message()のエイリアス）
    [[nodiscard]] std::string errorMessage() const { return error.message(); }
};

//! ファイル操作結果（書き込み、削除等）
struct FileOperationResult {
    bool success = false;         //!< 成功フラグ
    FileError error;              //!< エラー情報

    //! エラーメッセージを取得
    //! @return エラーメッセージ（error.message()のエイリアス）
    [[nodiscard]] std::string errorMessage() const { return error.message(); }
};

//! エントリの種類
enum class FileEntryType {
    File,       //!< ファイル
    Directory,  //!< ディレクトリ
};

//! ディレクトリエントリ情報
struct DirectoryEntry {
    std::string name;       //!< エントリ名
    FileEntryType type;     //!< 種類
    int64_t size = 0;       //!< ファイルサイズ（ディレクトリの場合は0）
};

//==============================================================================
//! 非同期読み込み
//==============================================================================

//! 非同期読み込みの状態
enum class AsyncReadState {
    Pending,    //!< 実行待ち
    Running,    //!< 実行中
    Completed,  //!< 完了
    Cancelled,  //!< キャンセル済み
    Failed,     //!< 失敗
};

//! 非同期読み込み完了コールバック
using AsyncReadCallback = std::function<void(const FileReadResult&)>;

//! 非同期読み込みハンドル
//! @note std::futureは真のキャンセルをサポートしないため、requestCancellation()は
//!       協調的キャンセルのフラグを設定するのみ。実際のI/O操作は完了まで実行される。
//! @note get()は複数回呼び出し可能。初回呼び出し時に結果をキャッシュする。
class AsyncReadHandle {
public:
    AsyncReadHandle() = default;

    //! コンストラクタ
    //! @param future 非同期操作のfuture
    //! @param cancellationToken キャンセルトークン（nullptrの場合は内部で作成）
    //! @note 状態はRunningに初期化される（std::asyncは即座に開始するため）
    explicit AsyncReadHandle(std::future<FileReadResult>&& future,
                            std::shared_ptr<std::atomic<bool>> cancellationToken = nullptr)
        : future_(std::make_shared<std::future<FileReadResult>>(std::move(future)))
        , state_(std::make_shared<std::atomic<AsyncReadState>>(AsyncReadState::Running))
        , cancellationRequested_(cancellationToken ? cancellationToken
                                                   : std::make_shared<std::atomic<bool>>(false))
        , cachedResult_(std::make_shared<std::optional<FileReadResult>>()) {}

    //! 完了したか確認
    [[nodiscard]] bool isReady() const noexcept {
        if (cachedResult_ && cachedResult_->has_value()) return true;
        if (!future_) return true;
        return future_->wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    }

    //! 状態を取得
    [[nodiscard]] AsyncReadState getState() const noexcept {
        if (!state_) return AsyncReadState::Failed;
        return state_->load();
    }

    //! キャンセルがリクエストされているか
    [[nodiscard]] bool isCancellationRequested() const noexcept {
        if (!cancellationRequested_) return false;
        return cancellationRequested_->load();
    }

    //! キャンセルをリクエスト（協調的キャンセル）
    //! @note 実際のI/O操作は中断されない。操作完了後に状態がCancelledになる。
    //!       readAsync実装側でキャンセルトークンをチェックする必要がある。
    void requestCancellation() noexcept {
        if (cancellationRequested_) {
            cancellationRequested_->store(true);
        }
        if (state_) {
            auto current = state_->load();
            // Running状態の場合のみCancelledに遷移
            if (current == AsyncReadState::Running || current == AsyncReadState::Pending) {
                state_->store(AsyncReadState::Cancelled);
            }
        }
    }

    //! 結果を取得（ブロッキング）
    //! @note 複数回呼び出し可能。初回呼び出し時に結果をキャッシュする。
    //! @note キャンセル済みの場合でもfutureの結果を返す（I/Oは完了している）
    [[nodiscard]] FileReadResult get() {
        // キャッシュ済みなら即座に返す
        if (cachedResult_ && cachedResult_->has_value()) {
            return cachedResult_->value();
        }

        if (!future_) {
            FileReadResult result;
            result.error = FileError::make(FileError::Code::Unknown, 0, "Invalid async handle");
            return result;
        }

        auto result = future_->get();

        if (state_) {
            // キャンセルリクエスト済みの場合
            if (cancellationRequested_ && cancellationRequested_->load()) {
                state_->store(AsyncReadState::Cancelled);
                result.success = false;
                result.error = FileError::make(FileError::Code::Cancelled, 0, "Operation was cancelled");
            } else {
                state_->store(result.success ? AsyncReadState::Completed : AsyncReadState::Failed);
            }
        }

        // 結果をキャッシュ
        if (cachedResult_) {
            *cachedResult_ = result;
        }

        return result;
    }

    //! 結果を取得（タイムアウト付き）
    //! @return 結果（タイムアウト時はnullopt）
    template<typename Rep, typename Period>
    [[nodiscard]] std::optional<FileReadResult> getFor(const std::chrono::duration<Rep, Period>& timeout) {
        // キャッシュ済みなら即座に返す
        if (cachedResult_ && cachedResult_->has_value()) {
            return cachedResult_->value();
        }
        if (!future_) return std::nullopt;
        if (future_->wait_for(timeout) == std::future_status::ready) {
            return get();
        }
        return std::nullopt;
    }

    //! 有効なハンドルか
    [[nodiscard]] bool isValid() const noexcept {
        return future_ != nullptr || (cachedResult_ && cachedResult_->has_value());
    }

    //! キャンセルトークンを取得（readAsync実装用）
    [[nodiscard]] std::shared_ptr<std::atomic<bool>> getCancellationToken() const noexcept {
        return cancellationRequested_;
    }

private:
    std::shared_ptr<std::future<FileReadResult>> future_;
    std::shared_ptr<std::atomic<AsyncReadState>> state_;
    std::shared_ptr<std::atomic<bool>> cancellationRequested_;  //!< キャンセルリクエストフラグ
    std::shared_ptr<std::optional<FileReadResult>> cachedResult_;  //!< キャッシュされた結果
};

