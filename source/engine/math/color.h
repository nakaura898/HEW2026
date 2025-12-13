//----------------------------------------------------------------------------
//! @file   color.h
//! @brief  カラー型定義（DirectXTK SimpleMath使用）
//----------------------------------------------------------------------------
#pragma once

#include <SimpleMath.h>

//===========================================================================
//! DirectXTK Color型のエイリアス
//===========================================================================
using Color = DirectX::SimpleMath::Color;

//===========================================================================
//! 定義済みカラー定数
//===========================================================================
namespace Colors {

// 基本色
inline const Color White       = { 1.0f, 1.0f, 1.0f, 1.0f };
inline const Color Black       = { 0.0f, 0.0f, 0.0f, 1.0f };
inline const Color Red         = { 1.0f, 0.0f, 0.0f, 1.0f };
inline const Color Green       = { 0.0f, 1.0f, 0.0f, 1.0f };
inline const Color Blue        = { 0.0f, 0.0f, 1.0f, 1.0f };
inline const Color Yellow      = { 1.0f, 1.0f, 0.0f, 1.0f };
inline const Color Cyan        = { 0.0f, 1.0f, 1.0f, 1.0f };
inline const Color Magenta     = { 1.0f, 0.0f, 1.0f, 1.0f };

// グレースケール
inline const Color Gray        = { 0.5f, 0.5f, 0.5f, 1.0f };
inline const Color DarkGray    = { 0.25f, 0.25f, 0.25f, 1.0f };
inline const Color LightGray   = { 0.75f, 0.75f, 0.75f, 1.0f };

// 追加色
inline const Color Orange      = { 1.0f, 0.5f, 0.0f, 1.0f };
inline const Color Purple      = { 0.5f, 0.0f, 0.5f, 1.0f };
inline const Color Pink        = { 1.0f, 0.75f, 0.8f, 1.0f };
inline const Color Brown       = { 0.6f, 0.3f, 0.0f, 1.0f };

// 透明
inline const Color Transparent = { 0.0f, 0.0f, 0.0f, 0.0f };

// コーンフラワーブルー（DirectX定番の背景色）
inline const Color CornflowerBlue = { 0.392f, 0.584f, 0.929f, 1.0f };

} // namespace Colors

//===========================================================================
//! カラーユーティリティ
//===========================================================================

//! RGBA (0-255) からColorを作成
[[nodiscard]] inline Color ColorFromRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    return Color(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
}

//! 16進数からColorを作成 (0xRRGGBBAA)
[[nodiscard]] inline Color ColorFromHex(uint32_t hex) {
    return Color(
        ((hex >> 24) & 0xFF) / 255.0f,
        ((hex >> 16) & 0xFF) / 255.0f,
        ((hex >> 8) & 0xFF) / 255.0f,
        (hex & 0xFF) / 255.0f
    );
}

//! HSVからColorを作成 (h: 0-360, s: 0-1, v: 0-1)
[[nodiscard]] inline Color ColorFromHSV(float h, float s, float v, float a = 1.0f) {
    float c = v * s;
    float x = c * (1.0f - std::abs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
    float m = v - c;

    float r, g, b;
    if (h < 60.0f) {
        r = c; g = x; b = 0.0f;
    } else if (h < 120.0f) {
        r = x; g = c; b = 0.0f;
    } else if (h < 180.0f) {
        r = 0.0f; g = c; b = x;
    } else if (h < 240.0f) {
        r = 0.0f; g = x; b = c;
    } else if (h < 300.0f) {
        r = x; g = 0.0f; b = c;
    } else {
        r = c; g = 0.0f; b = x;
    }

    return Color(r + m, g + m, b + m, a);
}
