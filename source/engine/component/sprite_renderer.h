//----------------------------------------------------------------------------
//! @file   sprite_renderer.h
//! @brief  スプライトレンダラーコンポーネント
//----------------------------------------------------------------------------
#pragma once

#include "component.h"
#include "engine/color.h"
#include "engine/scene/math_types.h"

// 前方宣言
class Texture;

//============================================================================
//! @brief スプライトレンダラーコンポーネント
//!
//! テクスチャを2Dスプライトとして描画するためのコンポーネント。
//! Transform2Dと組み合わせて使用する。
//============================================================================
class SpriteRenderer : public Component {
public:
    SpriteRenderer() = default;

    //------------------------------------------------------------------------
    //! @brief コンストラクタ
    //! @param texture テクスチャ
    //------------------------------------------------------------------------
    explicit SpriteRenderer(Texture* texture)
        : texture_(texture) {}

    //------------------------------------------------------------------------
    // テクスチャ
    //------------------------------------------------------------------------

    [[nodiscard]] Texture* GetTexture() const noexcept { return texture_; }
    void SetTexture(Texture* texture) noexcept { texture_ = texture; }

    //------------------------------------------------------------------------
    // カラー
    //------------------------------------------------------------------------

    [[nodiscard]] const Color& GetColor() const noexcept { return color_; }
    void SetColor(const Color& color) noexcept { color_ = color; }
    void SetColor(float r, float g, float b, float a = 1.0f) noexcept {
        color_ = Color(r, g, b, a);
    }

    //! @brief アルファ値のみ設定
    void SetAlpha(float alpha) noexcept {
        color_.w = alpha;
    }
    [[nodiscard]] float GetAlpha() const noexcept { return color_.w; }

    //------------------------------------------------------------------------
    // 描画順（レイヤー）
    //------------------------------------------------------------------------

    [[nodiscard]] int GetSortingLayer() const noexcept { return sortingLayer_; }
    void SetSortingLayer(int layer) noexcept { sortingLayer_ = layer; }

    [[nodiscard]] int GetOrderInLayer() const noexcept { return orderInLayer_; }
    void SetOrderInLayer(int order) noexcept { orderInLayer_ = order; }

    //------------------------------------------------------------------------
    // 反転
    //------------------------------------------------------------------------

    [[nodiscard]] bool IsFlipX() const noexcept { return flipX_; }
    void SetFlipX(bool flip) noexcept { flipX_ = flip; }

    [[nodiscard]] bool IsFlipY() const noexcept { return flipY_; }
    void SetFlipY(bool flip) noexcept { flipY_ = flip; }

    //------------------------------------------------------------------------
    // サイズ
    //------------------------------------------------------------------------

    //! @brief カスタムサイズを取得（0,0の場合はテクスチャサイズを使用）
    [[nodiscard]] const Vector2& GetSize() const noexcept { return size_; }

    //! @brief カスタムサイズを設定
    void SetSize(const Vector2& size) noexcept { size_ = size; }
    void SetSize(float width, float height) noexcept {
        size_.x = width;
        size_.y = height;
    }

    //! @brief テクスチャサイズを使用（デフォルト）
    void UseTextureSize() noexcept {
        size_ = Vector2::Zero;
    }

    //------------------------------------------------------------------------
    // ピボット（スプライトの原点）
    //------------------------------------------------------------------------

    //! @brief ピボットを取得（スプライト内のローカル座標）
    [[nodiscard]] const Vector2& GetPivot() const noexcept { return pivot_; }

    //! @brief ピボットを設定（スプライト内のローカル座標）
    //! @param pivot スプライト左上からの相対位置（ピクセル単位）
    void SetPivot(const Vector2& pivot) noexcept { pivot_ = pivot; }
    void SetPivot(float x, float y) noexcept {
        pivot_.x = x;
        pivot_.y = y;
    }

    //! @brief ピボットを中央に設定（Animatorと一緒に使う場合に便利）
    //! @param frameWidth フレームの幅
    //! @param frameHeight フレームの高さ
    //! @param offsetX 中心からのX方向オフセット（正=右）
    //! @param offsetY 中心からのY方向オフセット（正=下）
    void SetPivotFromCenter(float frameWidth, float frameHeight,
                            float offsetX = 0.0f, float offsetY = 0.0f) noexcept {
        pivot_.x = frameWidth * 0.5f + offsetX;
        pivot_.y = frameHeight * 0.5f + offsetY;
    }

    //! @brief ピボットが設定されているか
    [[nodiscard]] bool HasPivot() const noexcept {
        return pivot_.x != 0.0f || pivot_.y != 0.0f;
    }

private:
    Texture* texture_ = nullptr;
    Color color_ = Colors::White;    //!< 乗算カラー
    Vector2 size_ = Vector2::Zero;   //!< カスタムサイズ（0,0でテクスチャサイズ）
    Vector2 pivot_ = Vector2::Zero;  //!< スプライトの原点（0,0で左上）

    int sortingLayer_ = 0;           //!< 描画レイヤー（大きいほど手前）
    int orderInLayer_ = 0;           //!< レイヤー内の描画順

    bool flipX_ = false;             //!< X軸反転
    bool flipY_ = false;             //!< Y軸反転
};
