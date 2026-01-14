//----------------------------------------------------------------------------
//! @file   debug_draw.cpp
//! @brief  デバッグ描画ユーティリティ実装（Debugビルドのみ）
//----------------------------------------------------------------------------
#include "debug_draw.h"

#ifdef _DEBUG

#include "engine/c_systems/sprite_batch.h"
#include "engine/texture/texture_manager.h"
#include "circle_renderer.h"
#include <cmath>

//----------------------------------------------------------------------------
// ※ Create()/Destroy()/Get()はヘッダーでインライン実装

//----------------------------------------------------------------------------
void DebugDraw::Shutdown()
{
    whiteTexture_.reset();
    initialized_ = false;
}

//----------------------------------------------------------------------------
void DebugDraw::EnsureInitialized()
{
    if (initialized_) return;

    // 1x1の白テクスチャを作成
    uint32_t whitePixel = 0xFFFFFFFF;
    whiteTexture_ = TextureManager::Get().Create2D(
        1, 1,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        D3D11_BIND_SHADER_RESOURCE,
        &whitePixel,
        sizeof(uint32_t)
    );

    initialized_ = true;
}

//----------------------------------------------------------------------------
void DebugDraw::DrawRectOutline(
    const Vector2& center,
    const Vector2& size,
    const Color& color,
    float lineWidth)
{
    float left = center.x - size.x * 0.5f;
    float top = center.y - size.y * 0.5f;

    DrawRectOutlineTopLeft(Vector2(left, top), size, color, lineWidth);
}

//----------------------------------------------------------------------------
void DebugDraw::DrawRectOutlineTopLeft(
    const Vector2& topLeft,
    const Vector2& size,
    const Color& color,
    float lineWidth)
{
    EnsureInitialized();
    if (!whiteTexture_) return;

    SpriteBatch& batch = SpriteBatch::Get();
    Texture* tex = whiteTexture_.get();

    float left = topLeft.x;
    float top = topLeft.y;
    float right = left + size.x;
    float bottom = top + size.y;

    // 上辺
    batch.Draw(tex, Vector2(left, top), color, 0.0f,
        Vector2(0, 0), Vector2(size.x, lineWidth), false, false, 100, 0);
    // 下辺
    batch.Draw(tex, Vector2(left, bottom - lineWidth), color, 0.0f,
        Vector2(0, 0), Vector2(size.x, lineWidth), false, false, 100, 0);
    // 左辺
    batch.Draw(tex, Vector2(left, top), color, 0.0f,
        Vector2(0, 0), Vector2(lineWidth, size.y), false, false, 100, 0);
    // 右辺
    batch.Draw(tex, Vector2(right - lineWidth, top), color, 0.0f,
        Vector2(0, 0), Vector2(lineWidth, size.y), false, false, 100, 0);
}

//----------------------------------------------------------------------------
void DebugDraw::DrawRectFilled(
    const Vector2& center,
    const Vector2& size,
    const Color& color)
{
    EnsureInitialized();
    if (!whiteTexture_) return;

    SpriteBatch& batch = SpriteBatch::Get();

    float left = center.x - size.x * 0.5f;
    float top = center.y - size.y * 0.5f;

    batch.Draw(whiteTexture_.get(), Vector2(left, top), color, 0.0f,
        Vector2(0, 0), size, false, false, 100, 0);
}

//----------------------------------------------------------------------------
void DebugDraw::DrawLine(
    const Vector2& start,
    const Vector2& end,
    const Color& color,
    float lineWidth)
{
    EnsureInitialized();
    if (!whiteTexture_) return;

    SpriteBatch& batch = SpriteBatch::Get();

    Vector2 diff = end - start;
    float length = diff.Length();
    if (length < 0.001f) return;

    // 角度計算
    float angle = std::atan2(diff.y, diff.x);

    // 線の中心位置
    Vector2 center = (start + end) * 0.5f;

    // 回転付きで描画
    batch.Draw(whiteTexture_.get(), center, color, angle,
        Vector2(0.5f, 0.5f), Vector2(length, lineWidth), false, false, 100, 0);
}

//----------------------------------------------------------------------------
void DebugDraw::DrawCircleOutline(
    const Vector2& center,
    float radius,
    const Color& color,
    int segments,
    float lineWidth)
{
    if (segments < 3) segments = 3;

    constexpr float kPi = 3.14159265358979323846f;
    float angleStep = 2.0f * kPi / static_cast<float>(segments);

    for (int i = 0; i < segments; ++i) {
        float angle1 = angleStep * static_cast<float>(i);
        float angle2 = angleStep * static_cast<float>(i + 1);

        Vector2 p1(
            center.x + std::cos(angle1) * radius,
            center.y + std::sin(angle1) * radius
        );
        Vector2 p2(
            center.x + std::cos(angle2) * radius,
            center.y + std::sin(angle2) * radius
        );

        DrawLine(p1, p2, color, lineWidth);
    }
}

//----------------------------------------------------------------------------
void DebugDraw::DrawCircleFilled(
    const Vector2& center,
    float radius,
    const Color& color,
    int /*segments*/)
{
    // 注意: CircleRendererはBegin/Endパターンを使用するため、
    // このメソッドは単独では描画されない。
    // 正しい使用法:
    //   CircleRenderer::Get().Begin(camera);
    //   CircleRenderer::Get().DrawFilled(center, radius, color);
    //   CircleRenderer::Get().End();
    CircleRenderer::Get().DrawFilled(center, radius, color);
}

#endif // _DEBUG
