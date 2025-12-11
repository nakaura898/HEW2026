//----------------------------------------------------------------------------
//! @file   logging.h
//! @brief  シンプルなログシステム（マクロベース）
//----------------------------------------------------------------------------
#pragma once

#include <Windows.h>
#include <string>
#include <format>
#include <source_location>
#include <vector>
#include <memory>

//----------------------------------------------------------------------------
// ログレベル定義
//----------------------------------------------------------------------------
enum class LogLevel {
    Debug   = 0,
    Info    = 1,
    Warning = 2,
    Error   = 3
};

//----------------------------------------------------------------------------
// ログ出力インターフェース（DIP対応）
//----------------------------------------------------------------------------
class ILogOutput {
public:
    virtual ~ILogOutput() = default;
    virtual void write(LogLevel level, const std::string& message) = 0;
};

//----------------------------------------------------------------------------
// デフォルト実装：OutputDebugString
//----------------------------------------------------------------------------
class DebugLogOutput : public ILogOutput {
public:
    void write(LogLevel level, const std::string& message) override {
        (void)level;  // 未使用パラメータの警告を抑制
        OutputDebugStringA(message.c_str());
    }
};

//----------------------------------------------------------------------------
// コンソール出力実装
//----------------------------------------------------------------------------
class ConsoleLogOutput : public ILogOutput {
public:
    ConsoleLogOutput() {
        // コンソールウィンドウを割り当て
        if (AllocConsole()) {
            FILE* fp = nullptr;
            freopen_s(&fp, "CONOUT$", "w", stdout);
            freopen_s(&fp, "CONOUT$", "w", stderr);
            SetConsoleOutputCP(CP_UTF8);
        }
    }

    void write(LogLevel level, const std::string& message) override {
        // レベルに応じて色を変更
        HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
        WORD color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;  // 白
        switch (level) {
            case LogLevel::Debug:   color = FOREGROUND_GREEN | FOREGROUND_BLUE; break;  // シアン
            case LogLevel::Info:    color = FOREGROUND_GREEN; break;  // 緑
            case LogLevel::Warning: color = FOREGROUND_RED | FOREGROUND_GREEN; break;  // 黄
            case LogLevel::Error:   color = FOREGROUND_RED; break;  // 赤
        }
        SetConsoleTextAttribute(hConsole, color);
        printf("%s", message.c_str());
        SetConsoleTextAttribute(hConsole, FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE);  // リセット
    }
};

//----------------------------------------------------------------------------
// デバッグ + コンソール両方出力
//----------------------------------------------------------------------------
class MultiLogOutput : public ILogOutput {
public:
    void write(LogLevel level, const std::string& message) override {
        debug_.write(level, message);
        console_.write(level, message);
    }
private:
    DebugLogOutput debug_;
    ConsoleLogOutput console_;
};

//----------------------------------------------------------------------------
// グローバルログシステム
//----------------------------------------------------------------------------
class LogSystem {
private:
    static inline ILogOutput* output_ = nullptr;
    static inline LogLevel minLevel_ = LogLevel::Debug;

public:
    // ログ出力先を設定（nullptr可）
    static void setOutput(ILogOutput* output) {
        output_ = output;
    }

    // 最小ログレベルを設定
    static void setMinLevel(LogLevel level) {
        minLevel_ = level;
    }

    // ログ出力
    static void log(LogLevel level, const std::string& message, const std::source_location& loc = std::source_location::current()) {
        if (level < minLevel_) return;

        // デフォルト出力先
        if (!output_) {
            static DebugLogOutput defaultOutput;
            output_ = &defaultOutput;
        }

        // フォーマット: [LEVEL] filename(line): message
        std::string levelStr;
        switch (level) {
            case LogLevel::Debug:   levelStr = "DEBUG"; break;
            case LogLevel::Info:    levelStr = "INFO"; break;
            case LogLevel::Warning: levelStr = "WARN"; break;
            case LogLevel::Error:   levelStr = "ERROR"; break;
        }

        // ファイル名のみ取得（フルパスではなく）
        std::string filename = loc.file_name();
        auto pos = filename.find_last_of("/\\");
        if (pos != std::string::npos) {
            filename = filename.substr(pos + 1);
        }

        char buffer[1024];
        snprintf(buffer, sizeof(buffer), "[%s] %s(%d): %s\n",
            levelStr.c_str(), filename.c_str(), loc.line(), message.c_str());

        output_->write(level, std::string(buffer));
    }
};

//----------------------------------------------------------------------------
// ログマクロ定義（簡易版）
//----------------------------------------------------------------------------

// シンプルな文字列連結バージョン（std::format不要）
#ifdef _DEBUG
    #define LOG_DEBUG(msg) LogSystem::log(LogLevel::Debug, std::string(msg))
    #define LOG_INFO(msg)  LogSystem::log(LogLevel::Info, std::string(msg))
    #define LOG_WARN(msg)  LogSystem::log(LogLevel::Warning, std::string(msg))
    #define LOG_ERROR(msg) LogSystem::log(LogLevel::Error, std::string(msg))
#else
    // リリースビルドではDEBUGレベルを無効化
    #define LOG_DEBUG(msg) ((void)0)
    #define LOG_INFO(msg)  LogSystem::log(LogLevel::Info, std::string(msg))
    #define LOG_WARN(msg)  LogSystem::log(LogLevel::Warning, std::string(msg))
    #define LOG_ERROR(msg) LogSystem::log(LogLevel::Error, std::string(msg))
#endif

// HRESULT用の特殊マクロ（常にログ出力、FAILEDチェックは呼び出し側で行う）
#define LOG_HRESULT(hr, msg) \
    do { \
        char buf_##__LINE__[256]; \
        snprintf(buf_##__LINE__, sizeof(buf_##__LINE__), "%s (HRESULT: 0x%08lX)", msg, static_cast<unsigned long>(hr)); \
        LOG_ERROR(buf_##__LINE__); \
    } while(0)

//----------------------------------------------------------------------------
// HRESULT例外クラス
//----------------------------------------------------------------------------
class HResultException : public std::runtime_error {
public:
    HResultException(HRESULT hr, const char* msg, const char* file, int line)
        : std::runtime_error(Format(hr, msg, file, line)), hr_(hr) {}

    [[nodiscard]] HRESULT Code() const noexcept { return hr_; }

private:
    HRESULT hr_;

    static std::string Format(HRESULT hr, const char* msg, const char* file, int line) {
        // ファイル名のみ取得
        const char* filename = file;
        for (const char* p = file; *p; ++p) {
            if (*p == '/' || *p == '\\') filename = p + 1;
        }
        char buf[512];
        snprintf(buf, sizeof(buf), "%s (HRESULT: 0x%08lX) at %s:%d",
                 msg, static_cast<unsigned long>(hr), filename, line);
        return buf;
    }
};

#ifdef _DEBUG
// デバッグ: FAILED時にログ出力 + 例外スロー
#define THROW_IF_FAILED(hr, msg) \
    do { \
        HRESULT _hr = (hr); \
        if (FAILED(_hr)) { \
            LOG_HRESULT(_hr, msg); \
            throw HResultException(_hr, msg, __FILE__, __LINE__); \
        } \
    } while(0)

// デバッグ: FAILED時にログ出力 + return false
#define RETURN_FALSE_IF_FAILED(hr, msg) \
    do { \
        HRESULT _hr = (hr); \
        if (FAILED(_hr)) { \
            LOG_HRESULT(_hr, msg); \
            return false; \
        } \
    } while(0)

// デバッグ: FAILED時にログ出力 + return nullptr
#define RETURN_NULL_IF_FAILED(hr, msg) \
    do { \
        HRESULT _hr = (hr); \
        if (FAILED(_hr)) { \
            LOG_HRESULT(_hr, msg); \
            return nullptr; \
        } \
    } while(0)

// デバッグ: FAILED時にログ出力 + return (void関数用)
#define RETURN_IF_FAILED(hr, msg) \
    do { \
        HRESULT _hr = (hr); \
        if (FAILED(_hr)) { \
            LOG_HRESULT(_hr, msg); \
            return; \
        } \
    } while(0)
#else
// リリース: スルー（何もしない）
#define THROW_IF_FAILED(hr, msg)        ((void)(hr))
#define RETURN_FALSE_IF_FAILED(hr, msg) ((void)(hr))
#define RETURN_NULL_IF_FAILED(hr, msg)  ((void)(hr))
#define RETURN_IF_FAILED(hr, msg)       ((void)(hr))
#endif

//----------------------------------------------------------------------------
// 汎用例外クラス
//----------------------------------------------------------------------------
class LogException : public std::runtime_error {
public:
    LogException(const char* msg, const char* file, int line)
        : std::runtime_error(Format(msg, file, line)) {}

private:
    static std::string Format(const char* msg, const char* file, int line) {
        const char* filename = file;
        for (const char* p = file; *p; ++p) {
            if (*p == '/' || *p == '\\') filename = p + 1;
        }
        char buf[512];
        snprintf(buf, sizeof(buf), "%s at %s:%d", msg, filename, line);
        return buf;
    }
};

#ifdef _DEBUG
// デバッグ: nullptr時にログ出力 + 例外スロー
#define THROW_IF_NULL(ptr, msg) \
    do { \
        if ((ptr) == nullptr) { \
            LOG_ERROR(msg); \
            throw LogException(msg, __FILE__, __LINE__); \
        } \
    } while(0)

// デバッグ: false時にログ出力 + 例外スロー
#define THROW_IF_FALSE(cond, msg) \
    do { \
        if (!(cond)) { \
            LOG_ERROR(msg); \
            throw LogException(msg, __FILE__, __LINE__); \
        } \
    } while(0)

// デバッグ: nullptr時にログ出力 + return nullptr
#define RETURN_NULL_IF_NULL(ptr, msg) \
    do { \
        if ((ptr) == nullptr) { \
            LOG_ERROR(msg); \
            return nullptr; \
        } \
    } while(0)

// デバッグ: nullptr時にログ出力 + return false
#define RETURN_FALSE_IF_NULL(ptr, msg) \
    do { \
        if ((ptr) == nullptr) { \
            LOG_ERROR(msg); \
            return false; \
        } \
    } while(0)

// デバッグ: nullptr時にログ出力 + return (void関数用)
#define RETURN_IF_NULL(ptr, msg) \
    do { \
        if ((ptr) == nullptr) { \
            LOG_ERROR(msg); \
            return; \
        } \
    } while(0)

// デバッグ: false時にログ出力 + return false
#define RETURN_FALSE_IF_FALSE(cond, msg) \
    do { \
        if (!(cond)) { \
            LOG_ERROR(msg); \
            return false; \
        } \
    } while(0)

// デバッグ: false時にログ出力 + return (void関数用)
#define RETURN_IF_FALSE(cond, msg) \
    do { \
        if (!(cond)) { \
            LOG_ERROR(msg); \
            return; \
        } \
    } while(0)
#else
// リリース: スルー（何もしない）
#define THROW_IF_NULL(ptr, msg)       ((void)(ptr))
#define THROW_IF_FALSE(cond, msg)     ((void)(cond))
#define RETURN_NULL_IF_NULL(ptr, msg) ((void)(ptr))
#define RETURN_FALSE_IF_NULL(ptr, msg) ((void)(ptr))
#define RETURN_IF_NULL(ptr, msg)      ((void)(ptr))
#define RETURN_FALSE_IF_FALSE(cond, msg) ((void)(cond))
#define RETURN_IF_FALSE(cond, msg)    ((void)(cond))
#endif

// Wide文字列変換ヘルパー
inline std::string wstringToString(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string str(size - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, str.data(), size, nullptr, nullptr);
    return str;
}