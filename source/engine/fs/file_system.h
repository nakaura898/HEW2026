#pragma once

#include "file_system_types.h"
#include <memory>
#include <span>
#include <string>
#include <vector>


//==============================================================================
//! シーク起点
//==============================================================================
enum class SeekOrigin {
    Begin,      //!< ファイル先頭から
    Current,    //!< 現在位置から
    End,        //!< ファイル末尾から
};

//==============================================================================
//! ファイルハンドルインターフェース（読み取り用）
//!
//! @note 推奨される読み取りパターン:
//! @code
//!   auto handle = fs->open("file.dat");
//!   while (handle && !handle->isEof()) {
//!       auto result = handle->read(4096);
//!       if (!result.success) break;
//!       // result.bytes.size() が実際に読み取れたバイト数
//!       process(result.bytes);
//!   }
//! @endcode
//==============================================================================
class IFileHandle {
public:
    virtual ~IFileHandle() = default;

    //! データを読み込む
    //! @param [in] size 読み込むバイト数（最大値）
    //! @return 読み込み結果
    //! @note 契約:
    //!   - 要求サイズより少ないバイト数が返る場合がある（EOF付近）
    //!   - result.bytes.size() が実際に読み取れたバイト数
    //!   - EOF到達時: success=true, bytes.size()=0
    //!   - エラー時: success=false
    //! @note 例: 残り100バイトで512バイト要求 → success=true, bytes.size()=100
    [[nodiscard]] virtual FileReadResult read(size_t size) noexcept = 0;

    //! ファイル位置を移動
    //! @param [in] offset オフセット
    //! @param [in] origin 起点
    //! @return 成功したらtrue
    virtual bool seek(int64_t offset, SeekOrigin origin = SeekOrigin::Begin) noexcept = 0;

    //! 現在のファイル位置を取得
    [[nodiscard]] virtual int64_t tell() const noexcept = 0;

    //! ファイルサイズを取得
    [[nodiscard]] virtual int64_t size() const noexcept = 0;

    //! 終端に達したか
    //! @note 推奨パターン: isEof()を読み取り前にチェックするのではなく、
    //!       read()の結果（bytes.size()==0 && success）でEOFを判断することも可能
    [[nodiscard]] virtual bool isEof() const noexcept = 0;

    //! ハンドルが有効か
    [[nodiscard]] virtual bool isValid() const noexcept = 0;
};

//==============================================================================
//! ファイルシステム基底インターフェース（情報取得のみ）
//!
//! @note パス引数について:
//!   パスは `const std::string&` を使用しています。`std::string_view` も検討されましたが、
//!   - HostFileSystemでは内部でwstringへの変換が必要
//!   - マップ検索で一時stringが必要になる場合がある
//!   - SSO (Small String Optimization) により短いパスは効率的
//!   以上の理由から、シンプルさと十分なパフォーマンスを優先しています。
//==============================================================================
class IFileSystem {
public:
    virtual ~IFileSystem() = default;

    //! ファイル/ディレクトリの存在確認
    [[nodiscard]] virtual bool exists(const std::string& path) const noexcept = 0;

    //! ファイルサイズを取得（失敗時は-1）
    [[nodiscard]] virtual int64_t getFileSize(const std::string& path) const noexcept = 0;

    //! ファイルかどうか確認
    [[nodiscard]] virtual bool isFile(const std::string& path) const noexcept = 0;

    //! ディレクトリかどうか確認
    [[nodiscard]] virtual bool isDirectory(const std::string& path) const noexcept = 0;

    //! 空き容量を取得（失敗時は-1）
    [[nodiscard]] virtual int64_t getFreeSpaceSize() const noexcept = 0;

    //! 最終更新時刻を取得（Unix時間、失敗時は-1）
    [[nodiscard]] virtual int64_t getLastWriteTime(const std::string& path) const noexcept = 0;
};

//==============================================================================
//! 読み取り可能ファイルシステムインターフェース
//==============================================================================
class IReadableFileSystem : public IFileSystem {
public:
    //! ファイルを開く
    //! @param [in] path ファイルパス
    //! @return ファイルハンドル（失敗時はnullptr）
    [[nodiscard]] virtual std::unique_ptr<IFileHandle> open(const std::string& path) noexcept = 0;

    //! ファイルを読み込む（全体を一度に）
    [[nodiscard]] virtual FileReadResult read(const std::string& path) noexcept = 0;

    //! ディレクトリ内のエントリを列挙
    [[nodiscard]] virtual std::vector<DirectoryEntry> listDirectory(const std::string& path) const noexcept = 0;

    //----------------------------------------------------------
    //! @name   非同期読み込み
    //----------------------------------------------------------

    //! ファイルを非同期で読み込む
    //! @param [in] path ファイルパス
    //! @return 非同期ハンドル
    [[nodiscard]] virtual AsyncReadHandle readAsync(const std::string& path) {
        // デフォルト実装: std::asyncで同期読み込みをラップ
        auto future = std::async(std::launch::async, [this, path]() {
            return this->read(path);
        });
        return AsyncReadHandle(std::move(future));
    }

    //! ファイルを非同期で読み込む（コールバック版）
    //! @param [in] path ファイルパス
    //! @param [in] callback 完了時コールバック
    //! @return 非同期ハンドル
    [[nodiscard]] virtual AsyncReadHandle readAsync(const std::string& path, AsyncReadCallback callback) {
        auto future = std::async(std::launch::async, [this, path, cb = std::move(callback)]() {
            auto result = this->read(path);
            if (cb) cb(result);
            return result;
        });
        return AsyncReadHandle(std::move(future));
    }

    //----------------------------------------------------------
    //! @name   便利メソッド（デフォルト実装）
    //----------------------------------------------------------

    //! テキストとして読み込む
    [[nodiscard]] virtual std::string readAsText(const std::string& path) noexcept {
        auto result = read(path);
        if (!result.success) return {};
        return std::string(
            reinterpret_cast<const char*>(result.bytes.data()),
            result.bytes.size());
    }

    //! char配列として読み込む（シェーダーソース用）
    [[nodiscard]] virtual std::vector<char> readAsChars(const std::string& path) noexcept {
        auto result = read(path);
        if (!result.success) return {};
        return std::vector<char>(
            reinterpret_cast<const char*>(result.bytes.data()),
            reinterpret_cast<const char*>(result.bytes.data()) + result.bytes.size());
    }
};

//==============================================================================
//! 書き込み可能ファイルシステムインターフェース
//==============================================================================
class IWritableFileSystem : public IReadableFileSystem {
public:
    //----------------------------------------------------------
    //! @name   ファイル操作
    //----------------------------------------------------------

    //! ファイルを作成
    virtual FileOperationResult createFile(const std::string& path, int64_t size = 0) noexcept = 0;

    //! ファイルを削除
    virtual FileOperationResult deleteFile(const std::string& path) noexcept = 0;

    //! ファイルをリネーム/移動
    virtual FileOperationResult renameFile(const std::string& oldPath, const std::string& newPath) noexcept = 0;

    //! ファイルにデータを書き込む
    virtual FileOperationResult writeFile(const std::string& path, std::span<const std::byte> data) noexcept = 0;

    //----------------------------------------------------------
    //! @name   ディレクトリ操作
    //----------------------------------------------------------

    //! ディレクトリを作成
    virtual FileOperationResult createDirectory(const std::string& path) noexcept = 0;

    //! ディレクトリを削除
    virtual FileOperationResult deleteDirectory(const std::string& path) noexcept = 0;

    //! ディレクトリを再帰的に削除
    virtual FileOperationResult deleteDirectoryRecursively(const std::string& path) noexcept = 0;

    //! ディレクトリをリネーム/移動
    virtual FileOperationResult renameDirectory(const std::string& oldPath, const std::string& newPath) noexcept = 0;
};

