//----------------------------------------------------------------------------
//! @file   camera2d.cpp
//! @brief  2Dカメラコンポーネント実装
//----------------------------------------------------------------------------
#include "camera2d.h"
#include "game_object.h"
#include "transform2d.h"

namespace {
    //! @brief 最小ズームレベル
    constexpr float kMinZoom = 0.001f;
    //! @brief ビューポート中央計算用係数
    constexpr float kHalf = 0.5f;
}

//----------------------------------------------------------------------------
Camera2D::Camera2D(float viewportWidth, float viewportHeight)
    : viewportWidth_(viewportWidth)
    , viewportHeight_(viewportHeight)
{
}

//----------------------------------------------------------------------------
void Camera2D::OnAttach()
{
    transform_ = GetOwner()->GetComponent<Transform2D>();
}

//----------------------------------------------------------------------------
Vector2 Camera2D::GetPosition() const noexcept
{
    return transform_ ? transform_->GetPosition() : Vector2::Zero;
}

//----------------------------------------------------------------------------
void Camera2D::SetPosition(const Vector2& position) noexcept
{
    if (transform_) transform_->SetPosition(position);
}

//----------------------------------------------------------------------------
void Camera2D::SetPosition(float x, float y) noexcept
{
    if (transform_) transform_->SetPosition(x, y);
}

//----------------------------------------------------------------------------
void Camera2D::Translate(const Vector2& delta) noexcept
{
    if (transform_) transform_->Translate(delta);
}

//----------------------------------------------------------------------------
float Camera2D::GetRotation() const noexcept
{
    return transform_ ? transform_->GetRotation() : 0.0f;
}

//----------------------------------------------------------------------------
float Camera2D::GetRotationDegrees() const noexcept
{
    return transform_ ? transform_->GetRotationDegrees() : 0.0f;
}

//----------------------------------------------------------------------------
void Camera2D::SetRotation(float radians) noexcept
{
    if (transform_) transform_->SetRotation(radians);
}

//----------------------------------------------------------------------------
void Camera2D::SetRotationDegrees(float degrees) noexcept
{
    if (transform_) transform_->SetRotationDegrees(degrees);
}

//----------------------------------------------------------------------------
void Camera2D::SetZoom(float zoom) noexcept
{
    zoom_ = (zoom > kMinZoom) ? zoom : kMinZoom;
}

//----------------------------------------------------------------------------
void Camera2D::SetViewportSize(float width, float height) noexcept
{
    viewportWidth_ = width;
    viewportHeight_ = height;
}

//----------------------------------------------------------------------------
Matrix Camera2D::GetViewMatrix() const
{
    return BuildViewMatrix();
}

//----------------------------------------------------------------------------
Matrix Camera2D::GetViewProjectionMatrix() const
{
    Matrix view = BuildViewMatrix();
    // 座標系: 左上が(0,0)、X+が右、Y+が下（標準スクリーン座標）
    // 深度範囲: -1.0 ～ 1.0 （スプライトのZ値が0.0～1.0で使用可能）
    Matrix projection = Matrix::CreateOrthographicOffCenter(
        0.0f, viewportWidth_,
        viewportHeight_, 0.0f,
        -1.0f, 1.0f
    );
    Matrix viewProj = view * projection;
    return viewProj.Transpose();  // シェーダー用に転置
}

//----------------------------------------------------------------------------
Vector2 Camera2D::ScreenToWorld(const Vector2& screenPos) const
{
    // GetViewProjectionMatrix()は転置済みなので、転置前のmatrixを直接計算
    Matrix view = BuildViewMatrix();
    Matrix projection = Matrix::CreateOrthographicOffCenter(
        0.0f, viewportWidth_,
        viewportHeight_, 0.0f,
        -1.0f, 1.0f
    );
    Matrix viewProj = view * projection;
    Matrix invViewProj = viewProj.Invert();

    float ndcX = (screenPos.x / viewportWidth_) * 2.0f - 1.0f;
    float ndcY = 1.0f - (screenPos.y / viewportHeight_) * 2.0f;

    Vector3 worldPos = Vector3::Transform(Vector3(ndcX, ndcY, 0.0f), invViewProj);
    return Vector2(worldPos.x, worldPos.y);
}

//----------------------------------------------------------------------------
Vector2 Camera2D::WorldToScreen(const Vector2& worldPos) const
{
    // GetViewProjectionMatrix()は転置済みなので、転置前のmatrixを直接計算
    Matrix view = BuildViewMatrix();
    Matrix projection = Matrix::CreateOrthographicOffCenter(
        0.0f, viewportWidth_,
        viewportHeight_, 0.0f,
        -1.0f, 1.0f
    );
    Matrix viewProj = view * projection;
    Vector3 ndcPos = Vector3::Transform(Vector3(worldPos.x, worldPos.y, 0.0f), viewProj);

    float screenX = (ndcPos.x + 1.0f) * kHalf * viewportWidth_;
    float screenY = (1.0f - ndcPos.y) * kHalf * viewportHeight_;
    return Vector2(screenX, screenY);
}

//----------------------------------------------------------------------------
void Camera2D::GetWorldBounds(Vector2& outMin, Vector2& outMax) const
{
    outMin = ScreenToWorld(Vector2::Zero);
    outMax = ScreenToWorld(Vector2(viewportWidth_, viewportHeight_));
}

//----------------------------------------------------------------------------
void Camera2D::LookAt(const Vector2& target)
{
    SetPosition(target);
}

//----------------------------------------------------------------------------
void Camera2D::Follow(const Vector2& target, float smoothing)
{
    Vector2 pos = GetPosition();
    Vector2 diff = target - pos;
    Translate(diff * Clamp(smoothing, 0.0f, 1.0f));
}

//----------------------------------------------------------------------------
Matrix Camera2D::BuildViewMatrix() const
{
    Vector2 position = GetPosition();
    float rotation = GetRotation();

    float halfWidth = viewportWidth_ * kHalf;
    float halfHeight = viewportHeight_ * kHalf;

    Matrix translation = Matrix::CreateTranslation(-position.x, -position.y, 0.0f);
    Matrix rot = Matrix::CreateRotationZ(-rotation);
    Matrix scale = Matrix::CreateScale(zoom_, zoom_, 1.0f);
    Matrix centerOffset = Matrix::CreateTranslation(halfWidth, halfHeight, 0.0f);

    return translation * rot * scale * centerOffset;
}
