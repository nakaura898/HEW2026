//----------------------------------------------------------------------------
//! @file   camera2d.h
//! @brief  2Dカメラコンポーネント
//----------------------------------------------------------------------------
#pragma once

#include "component.h"
#include "engine/math/math_types.h"

// 前方宣言
class Transform2D;

//============================================================================
//! @brief 2Dカメラコンポーネント
//!
//! 2D空間でのビュー変換を管理する。
//! Transform2Dコンポーネントと連携し、位置・回転はTransform2Dから取得。
//! ズームとビューポートサイズはCamera2D固有の設定。
//!
//! @note 同じGameObjectにTransform2Dが必要
//============================================================================
class Camera2D : public Component {
public:
    Camera2D() = default;

    //------------------------------------------------------------------------
    //! @brief コンストラクタ
    //! @param viewportWidth ビューポート幅
    //! @param viewportHeight ビューポート高さ
    //------------------------------------------------------------------------
    Camera2D(float viewportWidth, float viewportHeight);

    //------------------------------------------------------------------------
    // Component オーバーライド
    //------------------------------------------------------------------------

    void OnAttach() override;

    //------------------------------------------------------------------------
    // 位置（Transform2Dに委譲）
    //------------------------------------------------------------------------

    [[nodiscard]] Vector2 GetPosition() const noexcept;
    void SetPosition(const Vector2& position) noexcept;
    void SetPosition(float x, float y) noexcept;
    void Translate(const Vector2& delta) noexcept;

    //------------------------------------------------------------------------
    // 回転（Transform2Dに委譲）
    //------------------------------------------------------------------------

    [[nodiscard]] float GetRotation() const noexcept;
    [[nodiscard]] float GetRotationDegrees() const noexcept;
    void SetRotation(float radians) noexcept;
    void SetRotationDegrees(float degrees) noexcept;

    //------------------------------------------------------------------------
    // ズーム（Camera2D固有）
    //------------------------------------------------------------------------

    [[nodiscard]] float GetZoom() const noexcept { return zoom_; }
    void SetZoom(float zoom) noexcept;

    //------------------------------------------------------------------------
    // ビューポート
    //------------------------------------------------------------------------

    [[nodiscard]] float GetViewportWidth() const noexcept { return viewportWidth_; }
    [[nodiscard]] float GetViewportHeight() const noexcept { return viewportHeight_; }
    void SetViewportSize(float width, float height) noexcept;

    //------------------------------------------------------------------------
    // 行列
    //------------------------------------------------------------------------

    //! @brief ビュー行列を取得
    [[nodiscard]] Matrix GetViewMatrix() const;

    //! @brief ビュープロジェクション行列を取得
    [[nodiscard]] Matrix GetViewProjectionMatrix() const;

    //------------------------------------------------------------------------
    // 座標変換
    //------------------------------------------------------------------------

    //! @brief スクリーン座標をワールド座標に変換
    [[nodiscard]] Vector2 ScreenToWorld(const Vector2& screenPos) const;

    //! @brief ワールド座標をスクリーン座標に変換
    [[nodiscard]] Vector2 WorldToScreen(const Vector2& worldPos) const;

    //! @brief カメラが映す領域の境界を取得
    void GetWorldBounds(Vector2& outMin, Vector2& outMax) const;

    //------------------------------------------------------------------------
    // ユーティリティ
    //------------------------------------------------------------------------

    //! @brief 指定位置を画面中央に映すようにカメラを移動
    void LookAt(const Vector2& target);

    //! @brief カメラを対象に追従（スムーズ）
    void Follow(const Vector2& target, float smoothing = 0.1f);

private:
    [[nodiscard]] Matrix BuildViewMatrix() const;

    Transform2D* transform_ = nullptr;  //!< 位置・回転の参照先
    float zoom_ = 1.0f;

    float viewportWidth_ = 1280.0f;
    float viewportHeight_ = 720.0f;
};
