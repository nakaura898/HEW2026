#pragma once

#include "engine/math/math_types.h"
#include "engine/math/color.h"
#include <algorithm>

class UIGauge
{
public:
 
    //コンストラクタ
    UIGauge(const Vector2& pos, const Vector2& size);

    // 描画
    void Render();


    // ゲージ量の割合を設定
    void SetValue(float ratio) {
        // 0.0?1.0の範囲に収める
        if (ratio < 0.0f) ratio = 0.0f;
        if (ratio > 1.0f) ratio = 1.0f;
        ratio_ = ratio;
    }

    // 色の設定
    void SetBackgroundColor(const Color& c) { bgColor_ = c; }   // 背景色
    void SetFillColor(const Color& c) { fillColor_ = c; }       // ゲージ色
    void SetPosition(const Vector2& pos) { position_ = pos; }   // 位置変更

private:
    Vector2 position_;   // ゲージ中心のスクリーン座標
    Vector2 size_;       // ゲージ全体のサイズ

    float ratio_ = 1.0f; // 現在の値（0.0?1.0）

    Color bgColor_ = Color(0.2f, 0.2f, 0.2f, 0.8f);  // 背景: 暗いグレー
    Color fillColor_ = Color(0.0f, 1.0f, 0.0f, 1.0f);  // ゲージ: 緑
};
