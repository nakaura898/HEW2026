#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>


//! ファイル変更イベントの種類
enum class FileChangeType {
    Modified,   //!< ファイルが変更された
    Created,    //!< ファイルが作成された
    Deleted,    //!< ファイルが削除された
    Renamed,    //!< ファイル名が変更された
};

//! ファイル変更イベント情報
struct FileChangeEvent {
    FileChangeType type;        //!< 変更種類
    std::wstring path;          //!< 変更されたファイルのパス
    std::wstring oldPath;       //!< リネーム時の旧パス（Renamed以外は空）
};

//! ファイル変更コールバック型
using FileChangeCallback = std::function<void(const FileChangeEvent&)>;

//! ファイル監視クラス
//! 指定したディレクトリ内のファイル変更を非同期で監視する
//! 主にホットリロード機能での使用を想定
class FileWatcher {
public:
    //! コンストラクタ
    FileWatcher();

    //! デストラクタ
    ~FileWatcher();

    //! コピー禁止
    FileWatcher(const FileWatcher&) = delete;
    FileWatcher& operator=(const FileWatcher&) = delete;

    //! ムーブ許可
    FileWatcher(FileWatcher&& other) noexcept;
    FileWatcher& operator=(FileWatcher&& other) noexcept;

    //! 監視を開始
    //! @param [in] directoryPath 監視するディレクトリパス
    //! @param [in] recursive サブディレクトリも監視するか
    //! @param [in] callback ファイル変更時のコールバック
    //! @return 成功したらtrue
    bool start(const std::wstring& directoryPath, bool recursive, FileChangeCallback callback);

    //! 監視を停止
    void stop();

    //! 監視中かどうか
    [[nodiscard]] bool isWatching() const noexcept;

    //! 監視中のディレクトリパスを取得
    [[nodiscard]] const std::wstring& getWatchPath() const noexcept;

    //! 変更イベントをポーリング（メインスレッドで呼び出す）
    //! コールバックが設定されている場合、溜まったイベントに対してコールバックを呼び出す
    //! @return 処理したイベント数
    size_t pollEvents();

    //! フィルター設定：監視する拡張子を指定
    //! @param [in] extensions 拡張子リスト（例: {".hlsl", ".cpp"}）空の場合は全て監視
    void setExtensionFilter(const std::vector<std::wstring>& extensions);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

