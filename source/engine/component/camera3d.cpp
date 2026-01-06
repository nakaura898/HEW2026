//----------------------------------------------------------------------------
//! @file   camera3d.cpp
//! @brief  3Dカメラコンポーネント実装
//----------------------------------------------------------------------------
#include "camera3d.h"
#include "game_object.h"
#include "transform.h"

namespace {
    constexpr float kMinFOV = 1.0f;
    constexpr float kMaxFOV = 179.0f;
    constexpr float kMinNear = 0.001f;
    constexpr float kHalf = 0.5f;
}

//----------------------------------------------------------------------------
Camera3D::Camera3D(float fovDegrees, float aspectRatio)
    : fov_(fovDegrees)
    , aspectRatio_(aspectRatio)
{
}

//----------------------------------------------------------------------------
void Camera3D::OnAttach()
{
    transform_ = GetOwner()->GetComponent<Transform>();
    if (transform_) {
        transform_->Enable3DRotation();
    }
}

//----------------------------------------------------------------------------
Vector3 Camera3D::GetPosition() const noexcept
{
    return transform_ ? transform_->GetPosition3D() : Vector3::Zero;
}

//----------------------------------------------------------------------------
void Camera3D::SetPosition(const Vector3& position) noexcept
{
    if (transform_) transform_->SetPosition3D(position);
}

//----------------------------------------------------------------------------
void Camera3D::SetPosition(float x, float y, float z) noexcept
{
    if (transform_) transform_->SetPosition(x, y, z);
}

//----------------------------------------------------------------------------
void Camera3D::Translate(const Vector3& delta) noexcept
{
    if (transform_) transform_->Translate(delta.x, delta.y, delta.z);
}

//----------------------------------------------------------------------------
Quaternion Camera3D::GetRotation() const noexcept
{
    return transform_ ? transform_->GetRotation3D() : Quaternion::Identity;
}

//----------------------------------------------------------------------------
void Camera3D::SetRotation(const Quaternion& rotation) noexcept
{
    if (transform_) transform_->SetRotation3D(rotation);
}

//----------------------------------------------------------------------------
void Camera3D::SetRotation(float pitch, float yaw, float roll) noexcept
{
    if (transform_) transform_->SetRotation3D(pitch, yaw, roll);
}

//----------------------------------------------------------------------------
Matrix Camera3D::GetViewMatrix() const
{
    return BuildViewMatrix();
}

//----------------------------------------------------------------------------
Matrix Camera3D::GetProjectionMatrix() const
{
    return BuildProjectionMatrix();
}

//----------------------------------------------------------------------------
Matrix Camera3D::GetViewProjectionMatrix() const
{
    Matrix view = BuildViewMatrix();
    Matrix projection = BuildProjectionMatrix();
    Matrix viewProj = view * projection;
    return viewProj.Transpose();  // シェーダー用に転置
}

//----------------------------------------------------------------------------
Vector3 Camera3D::GetForward() const
{
    Quaternion rot = GetRotation();
    return Vector3::Transform(Vector3::Forward, rot);
}

//----------------------------------------------------------------------------
Vector3 Camera3D::GetRight() const
{
    Quaternion rot = GetRotation();
    return Vector3::Transform(Vector3::Right, rot);
}

//----------------------------------------------------------------------------
Vector3 Camera3D::GetUp() const
{
    Quaternion rot = GetRotation();
    return Vector3::Transform(Vector3::Up, rot);
}

//----------------------------------------------------------------------------
void Camera3D::LookAt(const Vector3& target, const Vector3& up)
{
    if (!transform_) return;

    Vector3 position = GetPosition();
    Vector3 forward = target - position;

    // ゼロベクトルガード: target == position の場合は何もしない
    float lengthSq = forward.x * forward.x + forward.y * forward.y + forward.z * forward.z;
    if (lengthSq < 1e-8f) return;

    forward.Normalize();

    // forward方向からQuaternionを計算
    Quaternion rotation = Quaternion::LookRotation(forward, up);
    transform_->SetRotation3D(rotation);
}

//----------------------------------------------------------------------------
Vector3 Camera3D::ScreenToWorld(const Vector2& screenPos,
                                float screenWidth, float screenHeight,
                                float depth) const
{
    Matrix view = BuildViewMatrix();
    Matrix projection = BuildProjectionMatrix();
    Matrix viewProj = view * projection;
    Matrix invViewProj = viewProj.Invert();

    // スクリーン座標をNDC座標に変換
    float ndcX = (screenPos.x / screenWidth) * 2.0f - 1.0f;
    float ndcY = 1.0f - (screenPos.y / screenHeight) * 2.0f;
    float ndcZ = depth;  // 0.0 = near, 1.0 = far

    Vector3 worldPos = Vector3::Transform(Vector3(ndcX, ndcY, ndcZ), invViewProj);
    return worldPos;
}

//----------------------------------------------------------------------------
Vector2 Camera3D::WorldToScreen(const Vector3& worldPos,
                                float screenWidth, float screenHeight) const
{
    Matrix view = BuildViewMatrix();
    Matrix projection = BuildProjectionMatrix();
    Matrix viewProj = view * projection;

    // Vector4で変換して透視除算（w除算）を実行
    Vector4 v4(worldPos.x, worldPos.y, worldPos.z, 1.0f);
    Vector4 result = Vector4::Transform(v4, viewProj);

    // カメラ後方または位置にある点はスクリーン外として処理
    if (result.w <= 0.0f) {
        return Vector2(-1.0f, -1.0f);  // 画面外を示すセンチネル値
    }

    // 透視除算: w成分で割ってNDC座標を取得
    result.x /= result.w;
    result.y /= result.w;

    // NDC座標をスクリーン座標に変換
    float screenX = (result.x + 1.0f) * kHalf * screenWidth;
    float screenY = (1.0f - result.y) * kHalf * screenHeight;
    return Vector2(screenX, screenY);
}

//----------------------------------------------------------------------------
void Camera3D::ScreenPointToRay(const Vector2& screenPos,
                                float screenWidth, float screenHeight,
                                Vector3& outOrigin, Vector3& outDirection) const
{
    // near平面とfar平面上のワールド座標を取得
    Vector3 nearPoint = ScreenToWorld(screenPos, screenWidth, screenHeight, 0.0f);
    Vector3 farPoint = ScreenToWorld(screenPos, screenWidth, screenHeight, 1.0f);

    outOrigin = nearPoint;
    outDirection = farPoint - nearPoint;
    outDirection.Normalize();
}

//----------------------------------------------------------------------------
Matrix Camera3D::BuildViewMatrix() const
{
    Vector3 position = GetPosition();
    Quaternion rotation = GetRotation();

    // 回転の逆変換
    Quaternion invRotation;
    rotation.Inverse(invRotation);
    Matrix rotMatrix = Matrix::CreateFromQuaternion(invRotation);

    // 位置の逆変換
    Matrix transMatrix = Matrix::CreateTranslation(-position.x, -position.y, -position.z);

    // ビュー行列 = 位置逆変換 * 回転逆変換
    return transMatrix * rotMatrix;
}

//----------------------------------------------------------------------------
Matrix Camera3D::BuildProjectionMatrix() const
{
    float fovClamped = Clamp(fov_, kMinFOV, kMaxFOV);
    float fovRad = ToRadians(fovClamped);
    float nearClamped = (nearPlane_ < kMinNear) ? kMinNear : nearPlane_;

    return Matrix::CreatePerspectiveFieldOfView(
        fovRad,
        aspectRatio_,
        nearClamped,
        farPlane_
    );
}
