//----------------------------------------------------------------------------
//! @file   camera3d.h
//! @brief  3Dカメラコンポーネント
//----------------------------------------------------------------------------
#pragma once

#include "component.h"
#include "engine/math/math_types.h"

// 前方宣言
class Transform;

//============================================================================
//! @brief 3Dカメラコンポーネント
//!
//! 透視投影による3D描画をサポート。
//! Transformコンポーネントと連携して位置・回転を管理。
//!
//! @note 同じGameObjectにTransformが必要
//============================================================================
class Camera3D : public Component {
public:
    Camera3D() = default;

    //------------------------------------------------------------------------
    //! @brief コンストラクタ
    //! @param fovDegrees 視野角（度）
    //! @param aspectRatio アスペクト比
    //------------------------------------------------------------------------
    explicit Camera3D(float fovDegrees, float aspectRatio = 16.0f / 9.0f);

    //------------------------------------------------------------------------
    // Component オーバーライド
    //------------------------------------------------------------------------

    void OnAttach() override;

    //------------------------------------------------------------------------
    // 位置（Transformに委譲）
    //------------------------------------------------------------------------

    [[nodiscard]] Vector3 GetPosition() const noexcept;
    void SetPosition(const Vector3& position) noexcept;
    void SetPosition(float x, float y, float z) noexcept;
    void Translate(const Vector3& delta) noexcept;

    //------------------------------------------------------------------------
    // 回転（Transformに委譲、Quaternion）
    //------------------------------------------------------------------------

    [[nodiscard]] Quaternion GetRotation() const noexcept;
    void SetRotation(const Quaternion& rotation) noexcept;

    //! @brief オイラー角で回転を設定
    //! @param pitch X軸回転（ラジアン）
    //! @param yaw Y軸回転（ラジアン）
    //! @param roll Z軸回転（ラジアン）
    void SetRotation(float pitch, float yaw, float roll) noexcept;

    //------------------------------------------------------------------------
    // 投影設定
    //------------------------------------------------------------------------

    //! @brief 視野角を設定（度）
    void SetFOV(float degrees) noexcept { fov_ = degrees; }
    [[nodiscard]] float GetFOV() const noexcept { return fov_; }

    //! @brief ニアクリップ面を設定
    void SetNearPlane(float nearPlane) noexcept { nearPlane_ = nearPlane; }
    [[nodiscard]] float GetNearPlane() const noexcept { return nearPlane_; }

    //! @brief ファークリップ面を設定
    void SetFarPlane(float farPlane) noexcept { farPlane_ = farPlane; }
    [[nodiscard]] float GetFarPlane() const noexcept { return farPlane_; }

    //! @brief アスペクト比を設定
    void SetAspectRatio(float ratio) noexcept { aspectRatio_ = ratio; }
    [[nodiscard]] float GetAspectRatio() const noexcept { return aspectRatio_; }

    //! @brief ビューポートサイズからアスペクト比を設定
    void SetViewportSize(float width, float height) noexcept {
        if (height > 0.0f) {
            aspectRatio_ = width / height;
        }
    }

    //------------------------------------------------------------------------
    // 行列取得
    //------------------------------------------------------------------------

    //! @brief ビュー行列を取得
    [[nodiscard]] Matrix GetViewMatrix() const;

    //! @brief 投影行列を取得
    [[nodiscard]] Matrix GetProjectionMatrix() const;

    //! @brief ビュープロジェクション行列を取得
    [[nodiscard]] Matrix GetViewProjectionMatrix() const;

    //------------------------------------------------------------------------
    // 方向ベクトル
    //------------------------------------------------------------------------

    //! @brief カメラの前方向ベクトルを取得
    [[nodiscard]] Vector3 GetForward() const;

    //! @brief カメラの右方向ベクトルを取得
    [[nodiscard]] Vector3 GetRight() const;

    //! @brief カメラの上方向ベクトルを取得
    [[nodiscard]] Vector3 GetUp() const;

    //------------------------------------------------------------------------
    // ユーティリティ
    //------------------------------------------------------------------------

    //! @brief ターゲットを注視
    //! @param target 注視するワールド座標
    //! @param up 上方向ベクトル
    void LookAt(const Vector3& target, const Vector3& up = Vector3::Up);

    //! @brief スクリーン座標をワールド座標に変換
    //! @param screenPos スクリーン座標
    //! @param screenWidth 画面幅
    //! @param screenHeight 画面高さ
    //! @param depth 深度（0.0=near, 1.0=far）
    [[nodiscard]] Vector3 ScreenToWorld(const Vector2& screenPos,
                                        float screenWidth, float screenHeight,
                                        float depth) const;

    //! @brief ワールド座標をスクリーン座標に変換
    //! @param worldPos ワールド座標
    //! @param screenWidth 画面幅
    //! @param screenHeight 画面高さ
    [[nodiscard]] Vector2 WorldToScreen(const Vector3& worldPos,
                                        float screenWidth, float screenHeight) const;

    //! @brief スクリーン座標からレイを生成
    //! @param screenPos スクリーン座標
    //! @param screenWidth 画面幅
    //! @param screenHeight 画面高さ
    //! @param outOrigin レイの始点（出力）
    //! @param outDirection レイの方向（出力）
    void ScreenPointToRay(const Vector2& screenPos,
                          float screenWidth, float screenHeight,
                          Vector3& outOrigin, Vector3& outDirection) const;

private:
    [[nodiscard]] Matrix BuildViewMatrix() const;
    [[nodiscard]] Matrix BuildProjectionMatrix() const;

    Transform* transform_ = nullptr;  //!< 位置・回転の参照先

    float fov_ = 60.0f;               //!< 視野角（度）
    float nearPlane_ = 0.1f;          //!< ニアクリップ
    float farPlane_ = 1000.0f;        //!< ファークリップ
    float aspectRatio_ = 16.0f / 9.0f;  //!< アスペクト比
};
