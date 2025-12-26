//----------------------------------------------------------------------------
//! @file   debug_draw.h
//! @brief  デバッグ描画ユーティリティ（Debugビルドのみ有効）
//----------------------------------------------------------------------------
#pragma once

#include "engine/math/math_types.h"
#include "engine/math/color.h"

#ifdef _DEBUG

#include "dx11/gpu/texture.h"

//----------------------------------------------------------------------------
//! @brief デバッグ描画クラス（Debugビルドのみ）
//----------------------------------------------------------------------------
class DebugDraw
{
public:
    //! @brief シングルトン取得
    static DebugDraw& Get();

    //! @brief リソース解放
    void Shutdown();

    //! @brief 矩形の枠線を描画（中心基準）
    void DrawRectOutline(
        const Vector2& center,
        const Vector2& size,
        const Color& color,
        float lineWidth = 2.0f
    );

    //! @brief 矩形の枠線を描画（左上基準）
    void DrawRectOutlineTopLeft(
        const Vector2& topLeft,
        const Vector2& size,
        const Color& color,
        float lineWidth = 2.0f
    );

    //! @brief 塗りつぶし矩形を描画（中心基準）
    void DrawRectFilled(
        const Vector2& center,
        const Vector2& size,
        const Color& color
    );

    //! @brief 線を描画
    void DrawLine(
        const Vector2& start,
        const Vector2& end,
        const Color& color,
        float lineWidth = 2.0f
    );

    //! @brief 円の枠線を描画
    //! @param center 中心座標
    //! @param radius 半径
    //! @param color 色
    //! @param segments 分割数（デフォルト32）
    //! @param lineWidth 線の太さ
    void DrawCircleOutline(
        const Vector2& center,
        float radius,
        const Color& color,
        int segments = 32,
        float lineWidth = 2.0f
    );

    //! @brief 塗りつぶし円を描画
    //! @param center 中心座標
    //! @param radius 半径
    //! @param color 色
    //! @param segments 分割数（デフォルト32）
    void DrawCircleFilled(
        const Vector2& center,
        float radius,
        const Color& color,
        int segments = 32
    );

private:
    DebugDraw() = default;
    ~DebugDraw() = default;
    DebugDraw(const DebugDraw&) = delete;
    DebugDraw& operator=(const DebugDraw&) = delete;

    void EnsureInitialized();

    TexturePtr whiteTexture_;
    bool initialized_ = false;
};

//----------------------------------------------------------------------------
// デバッグ描画マクロ（Debugビルド: 実行、Releaseビルド: 消える）
//----------------------------------------------------------------------------
#define DEBUG_RECT(center, size, color, ...) \
    DebugDraw::Get().DrawRectOutline(center, size, color, ##__VA_ARGS__)
#define DEBUG_RECT_TL(topLeft, size, color, ...) \
    DebugDraw::Get().DrawRectOutlineTopLeft(topLeft, size, color, ##__VA_ARGS__)
#define DEBUG_RECT_FILL(center, size, color) \
    DebugDraw::Get().DrawRectFilled(center, size, color)
#define DEBUG_LINE(start, end, color, ...) \
    DebugDraw::Get().DrawLine(start, end, color, ##__VA_ARGS__)
#define DEBUG_CIRCLE(center, radius, color, ...) \
    DebugDraw::Get().DrawCircleOutline(center, radius, color, ##__VA_ARGS__)
#define DEBUG_CIRCLE_FILL(center, radius, color, ...) \
    DebugDraw::Get().DrawCircleFilled(center, radius, color, ##__VA_ARGS__)

#else

//----------------------------------------------------------------------------
// Releaseビルド: 全マクロが空になる
//----------------------------------------------------------------------------
#define DEBUG_RECT(...)       ((void)0)
#define DEBUG_RECT_TL(...)    ((void)0)
#define DEBUG_RECT_FILL(...)  ((void)0)
#define DEBUG_LINE(...)       ((void)0)
#define DEBUG_CIRCLE(...)     ((void)0)
#define DEBUG_CIRCLE_FILL(...) ((void)0)

#endif
