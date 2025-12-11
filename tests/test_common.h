//----------------------------------------------------------------------------
//! @file   test_common.h
//! @brief  テスト共通ユーティリティ
//!
//! @details
//! 全テストファイルで使用される共通のユーティリティを提供します。
//! - TEST_ASSERT: テストアサーションマクロ
//! - TestContext: テスト実行コンテキスト（カウンター管理）
//----------------------------------------------------------------------------
#pragma once

#include "common/logging/logging.h"
#include <iostream>
#include <string>

namespace tests {

//----------------------------------------------------------------------------
// テストコンテキスト
//----------------------------------------------------------------------------

//! テスト実行コンテキスト
//! @details テストカウンターとユーティリティ関数を提供
class TestContext
{
public:
    //! テスト実行回数
    int testCount = 0;
    //! テスト成功回数
    int passCount = 0;

    //! カウンターをリセット
    void Reset()
    {
        testCount = 0;
        passCount = 0;
    }

    //! 全テスト成功か判定
    [[nodiscard]] bool AllPassed() const
    {
        return passCount == testCount;
    }

    //! 結果サマリーを出力
    void PrintSummary(const char* suiteName) const
    {
        std::cout << "\n----------------------------------------" << std::endl;
        std::cout << suiteName << ": " << passCount << "/" << testCount << " 成功" << std::endl;
        std::cout << "----------------------------------------" << std::endl;
    }
};

//----------------------------------------------------------------------------
// テストアサーションマクロ
//----------------------------------------------------------------------------

//! テストアサートマクロ
//! @param ctx TestContextへの参照
//! @param condition テスト条件
//! @param message テスト説明メッセージ
#define TEST_ASSERT_CTX(ctx, condition, message) \
    do { \
        (ctx).testCount++; \
        if (!(condition)) { \
            LOG_ERROR("[失敗] " message); \
            std::cout << "[失敗] " << message << std::endl; \
        } else { \
            (ctx).passCount++; \
            std::cout << "[成功] " << message << std::endl; \
        } \
    } while(0)

//! 値比較付きテストアサートマクロ
//! @param ctx TestContextへの参照
//! @param expected 期待値
//! @param actual 実際の値
//! @param message テスト説明メッセージ
#define TEST_ASSERT_EQ_CTX(ctx, expected, actual, message) \
    do { \
        (ctx).testCount++; \
        if ((expected) != (actual)) { \
            LOG_ERROR("[失敗] " message); \
            std::cout << "[失敗] " << message << std::endl; \
            std::cout << "  期待値: " << (expected) << ", 実際: " << (actual) << std::endl; \
        } else { \
            (ctx).passCount++; \
            std::cout << "[成功] " << message << std::endl; \
        } \
    } while(0)

//----------------------------------------------------------------------------
// 後方互換性のためのグローバルコンテキスト
//----------------------------------------------------------------------------

//! グローバルテストカウンター（後方互換性用）
//! @note 新規コードではTestContextを使用することを推奨
inline int& GetGlobalTestCount()
{
    static int s_testCount = 0;
    return s_testCount;
}

//! グローバルテストパスカウンター（後方互換性用）
inline int& GetGlobalPassCount()
{
    static int s_passCount = 0;
    return s_passCount;
}

//! グローバルカウンターをリセット
inline void ResetGlobalCounters()
{
    GetGlobalTestCount() = 0;
    GetGlobalPassCount() = 0;
}

//! 後方互換性用TEST_ASSERTマクロ
//! @note グローバルカウンターを使用
#define TEST_ASSERT(condition, message) \
    do { \
        tests::GetGlobalTestCount()++; \
        if (!(condition)) { \
            LOG_ERROR("[失敗] " message); \
            std::cout << "[失敗] " << message << std::endl; \
        } else { \
            tests::GetGlobalPassCount()++; \
            std::cout << "[成功] " << message << std::endl; \
        } \
    } while(0)

} // namespace tests
