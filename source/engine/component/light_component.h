//----------------------------------------------------------------------------
//! @file   light_component.h
//! @brief  ライトコンポーネント
//----------------------------------------------------------------------------
#pragma once

#include "component.h"
#include "engine/lighting/light.h"

//============================================================================
//! @brief ライトコンポーネント
//!
//! シーン内のライトを表すコンポーネント。
//! LightingManagerに自動登録される。
//!
//! @code
//!   // 平行光源
//!   auto* light = obj->AddComponent<LightComponent>();
//!   light->SetType(LightType::Directional);
//!   light->SetDirection(Vector3(0, -1, 0.5f));
//!   light->SetColor(Colors::White);
//!   light->SetIntensity(1.0f);
//!
//!   // 点光源
//!   auto* pointLight = obj->AddComponent<LightComponent>();
//!   pointLight->SetType(LightType::Point);
//!   pointLight->SetRange(10.0f);
//! @endcode
//============================================================================
class LightComponent : public Component
{
public:
    LightComponent() = default;

    //------------------------------------------------------------------------
    //! @brief コンストラクタ
    //! @param type ライトタイプ
    //------------------------------------------------------------------------
    explicit LightComponent(LightType type)
        : type_(type) {}

    //------------------------------------------------------------------------
    // ライトタイプ
    //------------------------------------------------------------------------

    //! @brief ライトタイプを取得
    [[nodiscard]] LightType GetType() const noexcept { return type_; }

    //! @brief ライトタイプを設定
    void SetType(LightType type) noexcept { type_ = type; }

    //------------------------------------------------------------------------
    // 色・強度
    //------------------------------------------------------------------------

    //! @brief 色を取得
    [[nodiscard]] const Color& GetColor() const noexcept { return color_; }

    //! @brief 色を設定
    void SetColor(const Color& color) noexcept { color_ = color; }

    //! @brief 強度を取得
    [[nodiscard]] float GetIntensity() const noexcept { return intensity_; }

    //! @brief 強度を設定
    void SetIntensity(float intensity) noexcept { intensity_ = intensity; }

    //------------------------------------------------------------------------
    // 方向（Directional, Spot用）
    //------------------------------------------------------------------------

    //! @brief 方向を取得（ローカル空間）
    [[nodiscard]] const Vector3& GetDirection() const noexcept { return direction_; }

    //! @brief 方向を設定（自動正規化）
    void SetDirection(const Vector3& direction) noexcept {
        direction_ = Normalize(direction);
    }

    //------------------------------------------------------------------------
    // 範囲（Point, Spot用）
    //------------------------------------------------------------------------

    //! @brief 有効範囲を取得
    [[nodiscard]] float GetRange() const noexcept { return range_; }

    //! @brief 有効範囲を設定
    void SetRange(float range) noexcept { range_ = range; }

    //------------------------------------------------------------------------
    // スポットライトパラメータ
    //------------------------------------------------------------------------

    //! @brief 内角を取得（度）
    [[nodiscard]] float GetInnerAngle() const noexcept { return innerAngle_; }

    //! @brief 内角を設定（度）
    void SetInnerAngle(float degrees) noexcept { innerAngle_ = degrees; }

    //! @brief 外角を取得（度）
    [[nodiscard]] float GetOuterAngle() const noexcept { return outerAngle_; }

    //! @brief 外角を設定（度）
    void SetOuterAngle(float degrees) noexcept { outerAngle_ = degrees; }

    //! @brief スポットライト角度を一括設定
    void SetSpotAngles(float innerDegrees, float outerDegrees) noexcept {
        innerAngle_ = innerDegrees;
        outerAngle_ = outerDegrees;
    }

    //------------------------------------------------------------------------
    // シャドウ
    //------------------------------------------------------------------------

    //! @brief シャドウキャストを取得
    [[nodiscard]] bool IsCastShadow() const noexcept { return castShadow_; }

    //! @brief シャドウキャストを設定
    void SetCastShadow(bool cast) noexcept { castShadow_ = cast; }

    //------------------------------------------------------------------------
    // 有効/無効
    //------------------------------------------------------------------------

    //! @brief ライトが有効か
    [[nodiscard]] bool IsEnabled() const noexcept { return enabled_; }

    //! @brief ライトの有効/無効を設定
    void SetEnabled(bool enabled) noexcept { enabled_ = enabled; }

    //------------------------------------------------------------------------
    // LightData変換
    //------------------------------------------------------------------------

    //! @brief GPU用LightDataを構築
    //! @param worldPosition ワールド座標
    //! @param worldDirection ワールド方向（正規化済み）
    [[nodiscard]] LightData ToLightData(const Vector3& worldPosition,
                                         const Vector3& worldDirection) const noexcept
    {
        LightData data = {};
        data.position = Vector4(worldPosition.x, worldPosition.y, worldPosition.z,
                                static_cast<float>(type_));
        data.direction = Vector4(worldDirection.x, worldDirection.y, worldDirection.z, range_);
        data.color = Color(color_.R(), color_.G(), color_.B(), intensity_);

        if (type_ == LightType::Spot) {
            float innerCos = std::cos(ToRadians(innerAngle_ * 0.5f));
            float outerCos = std::cos(ToRadians(outerAngle_ * 0.5f));
            data.spotParams = Vector4(innerCos, outerCos, 1.0f, 0);
        }
        else {
            data.spotParams = Vector4(0, 0, 1.0f, 0);
        }

        return data;
    }

private:
    LightType type_ = LightType::Directional;   //!< ライトタイプ
    Color color_ = Colors::White;               //!< ライト色
    float intensity_ = 1.0f;                    //!< 強度

    Vector3 direction_ = Vector3(0, -1, 0);     //!< 方向（ローカル空間）
    float range_ = 10.0f;                       //!< 有効範囲

    float innerAngle_ = 30.0f;                  //!< スポット内角（度）
    float outerAngle_ = 45.0f;                  //!< スポット外角（度）

    bool castShadow_ = false;                   //!< シャドウキャスト
    bool enabled_ = true;                       //!< 有効フラグ
};
