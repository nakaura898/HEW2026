#pragma once

#include <string>
#include <vector>


//! パスユーティリティ（静的関数）
class PathUtility {
public:
    //! ファイル名を取得
    //! @param [in] path パス（例: "assets:/dir/file.txt"）
    //! @return ファイル名（例: "file.txt"）
    [[nodiscard]] static std::string getFileName(const std::string& path) {
        auto pos = path.find_last_of("/\\");
        if (pos == std::string::npos) {
            // マウントパスの場合は ":/" の後を探す
            auto mountPos = path.find(":/");
            if (mountPos != std::string::npos) {
                return path.substr(mountPos + 2);
            }
            return path;
        }
        return path.substr(pos + 1);
    }

    //! 拡張子を取得
    //! @param [in] path パス（例: "assets:/dir/file.txt"）
    //! @return 拡張子（例: ".txt"）
    [[nodiscard]] static std::string getExtension(const std::string& path) {
        auto fileName = getFileName(path);
        auto pos = fileName.rfind('.');
        if (pos == std::string::npos || pos == 0) return {};
        return fileName.substr(pos);
    }

    //! 親ディレクトリのパスを取得
    //! @param [in] path パス（例: "assets:/dir/file.txt"）
    //! @return 親パス（例: "assets:/dir"）
    //! @note 契約:
    //!   - "assets:/dir/file.txt" → "assets:/dir"
    //!   - "assets:/file.txt" → "assets:/" （マウントルート）
    //!   - "assets:/" → "" （親なし）
    //!   - "/" → "" （親なし）
    //!   - "file.txt" → "" （親なし）
    //!   空文字列は「親が存在しない」ことを示す
    [[nodiscard]] static std::string getParentPath(const std::string& path) {
        if (path.empty()) return {};

        auto pos = path.find_last_of("/\\");
        if (pos == std::string::npos) return {};

        // マウントパスの場合
        auto mountPos = path.find(":/");
        if (mountPos != std::string::npos) {
            // "assets:/" 自体の場合は親なし
            if (pos == mountPos + 1 && pos == path.size() - 1) {
                return {};
            }
            // "assets:/file.txt" の場合はルートを返す
            if (pos == mountPos + 1) {
                return path.substr(0, pos + 1);
            }
        }

        // "/" 自体の場合は親なし
        if (pos == 0 && path.size() == 1) {
            return {};
        }

        // "/file.txt" の場合はルートを返す
        if (pos == 0) {
            return "/";
        }

        return path.substr(0, pos);
    }

    //! パスを結合
    //! @param [in] base ベースパス（例: "assets:/dir"）
    //! @param [in] relative 相対パス（例: "sub/file.txt"）
    //! @return 結合パス（例: "assets:/dir/sub/file.txt"）
    [[nodiscard]] static std::string combine(const std::string& base, const std::string& relative) {
        if (base.empty()) return relative;
        if (relative.empty()) return base;

        char lastChar = base.back();
        if (lastChar == '/' || lastChar == '\\') {
            return base + relative;
        }
        return base + '/' + relative;
    }

    //! マウント名を取得
    //! @param [in] mountPath マウントパス（例: "assets:/dir/file.txt"）
    //! @return マウント名（例: "assets"）
    [[nodiscard]] static std::string getMountName(const std::string& mountPath) {
        auto pos = mountPath.find(":/");
        if (pos == std::string::npos) return {};
        return mountPath.substr(0, pos);
    }

    //! 相対パスを取得
    //! @param [in] mountPath マウントパス（例: "assets:/dir/file.txt"）
    //! @return 相対パス（例: "dir/file.txt"）
    [[nodiscard]] static std::string getRelativePath(const std::string& mountPath) {
        auto pos = mountPath.find(":/");
        if (pos == std::string::npos) return mountPath;
        return mountPath.substr(pos + 2);
    }

    //! パスを正規化する
    //! - バックスラッシュをスラッシュに統一
    //! - 連続するスラッシュを単一に
    //! - "." と ".." を解決
    //! - 末尾のスラッシュを除去（ルートを除く）
    //! @param [in] path パス
    //! @return 正規化されたパス
    //! @note マウントポイント越えの".."は無視される（セキュリティ対策）
    //!       例: "assets:/../etc/passwd" → "assets:/etc/passwd"（ルート越え防止）
    [[nodiscard]] static std::string normalize(const std::string& path) {
        if (path.empty()) return {};

        // マウントパスの場合、マウント部分と相対パス部分を分離
        std::string mountPrefix;
        std::string workPath = path;

        auto mountPos = path.find(":/");
        if (mountPos != std::string::npos) {
            mountPrefix = path.substr(0, mountPos + 2); // "mount:/"含む
            workPath = path.substr(mountPos + 2);
        }

        // バックスラッシュをスラッシュに統一
        std::string normalized;
        normalized.reserve(workPath.size());

        bool prevWasSlash = false;
        for (char c : workPath) {
            if (c == '\\') c = '/';

            // 連続スラッシュをスキップ
            if (c == '/') {
                if (prevWasSlash) continue;
                prevWasSlash = true;
            } else {
                prevWasSlash = false;
            }
            normalized += c;
        }

        // 先頭スラッシュを保持（ルートパス対応）
        bool hasLeadingSlash = !normalized.empty() && normalized[0] == '/';

        // "." と ".." を解決
        std::vector<std::string> components;
        size_t start = hasLeadingSlash ? 1 : 0;
        size_t end = start;

        while (end <= normalized.size()) {
            if (end == normalized.size() || normalized[end] == '/') {
                if (end > start) {
                    std::string component = normalized.substr(start, end - start);
                    if (component == "..") {
                        // マウントパスまたは絶対パスの場合、ルートを越えない
                        // （セキュリティ: サンドボックス外へのアクセス防止）
                        if (!components.empty()) {
                            components.pop_back();
                        }
                        // components が空の場合、".." は無視される（ルート越え防止）
                    } else if (component != ".") {
                        components.push_back(std::move(component));
                    }
                }
                start = end + 1;
            }
            ++end;
        }

        // 結果を組み立て
        std::string result = mountPrefix;

        // マウントパスでない絶対パスの場合、先頭スラッシュを追加
        if (hasLeadingSlash && mountPrefix.empty()) {
            result += '/';
        }

        for (size_t i = 0; i < components.size(); ++i) {
            if (i > 0) result += '/';
            result += components[i];
        }

        // ルートパスのみの場合 "/" を返す（既に追加済み）
        return result;
    }

    //! ワイド文字列パスを正規化する
    //! @param [in] path パス
    //! @return 正規化されたパス
    //! @note Windows固有パス形式のサポート:
    //!   - UNCパス: \\\\server\\share\\path → \\\\server\\share/path
    //!   - ドライブレター: C:\\path → C:/path
    [[nodiscard]] static std::wstring normalizeW(const std::wstring& path) {
        if (path.empty()) return {};

        // プレフィックス（UNC or ドライブレター）と作業パスを分離
        // プレフィックスは正規化対象外として保持される
        std::wstring prefix;
        std::wstring workPath = path;

        if (path.size() >= 2 && path[0] == L'\\' && path[1] == L'\\') {
            // UNCパス: \\server\share を prefix として保持
            // 例: \\server\share\dir\file
            //     ^^^^^^^^^^^^^^^^ prefix (\\server\share)
            //                     ^^^^^^^^^ workPath (\dir\file)
            size_t serverEnd = path.find_first_of(L"\\/", 2);
            if (serverEnd != std::wstring::npos) {
                size_t shareEnd = path.find_first_of(L"\\/", serverEnd + 1);
                if (shareEnd == std::wstring::npos) shareEnd = path.size();
                prefix = L"\\\\";
                prefix += path.substr(2, shareEnd - 2);
                workPath = (shareEnd < path.size()) ? path.substr(shareEnd) : L"";
            } else {
                // サーバー名のみ (\\server)
                prefix = path;
                workPath = L"";
            }
        }
        else if (path.size() >= 2 && path[1] == L':') {
            // ドライブレター: C: を prefix として保持
            // 例: C:\Users\test
            //     ^^ prefix (C:)
            //       ^^^^^^^^^^ workPath (\Users\test)
            prefix = path.substr(0, 2);
            workPath = path.substr(2);
        }

        // バックスラッシュをスラッシュに統一
        std::wstring normalized;
        normalized.reserve(workPath.size());

        bool prevWasSlash = false;
        for (wchar_t c : workPath) {
            if (c == L'\\') c = L'/';

            if (c == L'/') {
                if (prevWasSlash) continue;
                prevWasSlash = true;
            } else {
                prevWasSlash = false;
            }
            normalized += c;
        }

        // "." と ".." を解決
        std::vector<std::wstring> components;
        size_t start = 0;
        size_t end = 0;

        // 先頭スラッシュを保持
        bool hasLeadingSlash = !normalized.empty() && normalized[0] == L'/';
        if (hasLeadingSlash) start = 1;

        end = start;
        while (end <= normalized.size()) {
            if (end == normalized.size() || normalized[end] == L'/') {
                if (end > start) {
                    std::wstring component = normalized.substr(start, end - start);
                    if (component == L"..") {
                        if (!components.empty()) {
                            components.pop_back();
                        }
                    } else if (component != L".") {
                        components.push_back(std::move(component));
                    }
                }
                start = end + 1;
            }
            ++end;
        }

        // 結果を組み立て
        std::wstring result = prefix;
        if (hasLeadingSlash) result += L'/';

        for (size_t i = 0; i < components.size(); ++i) {
            if (i > 0) result += L'/';
            result += components[i];
        }

        return result;
    }

    //! 2つのパスが同一かどうか比較（正規化して比較）
    //! @param [in] path1 パス1
    //! @param [in] path2 パス2
    //! @return 同一ならtrue
    [[nodiscard]] static bool equals(const std::string& path1, const std::string& path2) {
        return normalize(path1) == normalize(path2);
    }

    //! パスが絶対パスかどうか
    //! @param [in] path パス
    //! @return 絶対パスならtrue
    [[nodiscard]] static bool isAbsolute(const std::string& path) {
        if (path.empty()) return false;
        // Windowsドライブレター (C:/, D:\ など)
        if (path.size() >= 3 && isAlpha(path[0]) && path[1] == ':' && (path[2] == '/' || path[2] == '\\')) {
            return true;
        }
        // UNCパス (\\server\share)
        if (path.size() >= 2 && path[0] == '\\' && path[1] == '\\') {
            return true;
        }
        return false;
    }

    //! ワイド文字列パスが絶対パスかどうか
    [[nodiscard]] static bool isAbsoluteW(const std::wstring& path) {
        if (path.empty()) return false;
        if (path.size() >= 3 && isAlphaW(path[0]) && path[1] == L':' && (path[2] == L'/' || path[2] == L'\\')) {
            return true;
        }
        if (path.size() >= 2 && path[0] == L'\\' && path[1] == L'\\') {
            return true;
        }
        return false;
    }

    //! 2つのパスが同一かどうか比較（正規化して大文字小文字を無視して比較）
    //! @param [in] path1 パス1
    //! @param [in] path2 パス2
    //! @return 同一ならtrue
    //! @note パフォーマンス: 2つの正規化文字列を作成するため、頻繁な呼び出しには非効率。
    //!       ホットパスで使用する場合は、同時走査による最適化版の実装を検討。
    //!       現状は明確さと保守性を優先。
    [[nodiscard]] static bool equalsIgnoreCase(const std::string& path1, const std::string& path2) {
        std::string n1 = normalize(path1);
        std::string n2 = normalize(path2);
        if (n1.size() != n2.size()) return false;
        for (size_t i = 0; i < n1.size(); ++i) {
            if (toLower(n1[i]) != toLower(n2[i])) return false;
        }
        return true;
    }

private:
    [[nodiscard]] static bool isAlpha(char c) noexcept {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
    }

    [[nodiscard]] static bool isAlphaW(wchar_t c) noexcept {
        return (c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z');
    }

    [[nodiscard]] static char toLower(char c) noexcept {
        return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;
    }
};

