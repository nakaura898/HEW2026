//----------------------------------------------------------------------------
//! @file   collider3d.cpp
//! @brief  3D当たり判定コンポーネント実装
//----------------------------------------------------------------------------
#include "collider3d.h"
#include "game_object.h"
#include "transform.h"

//----------------------------------------------------------------------------
Collider3D Collider3D::CreateAABB(const Vector3& size, const Vector3& offset)
{
    Collider3D collider;
    collider.shape_ = ColliderShape3D::AABB;
    collider.initSize_ = size;
    collider.initOffset_ = offset;
    return collider;
}

//----------------------------------------------------------------------------
Collider3D Collider3D::CreateSphere(float radius, const Vector3& offset)
{
    Collider3D collider;
    collider.shape_ = ColliderShape3D::Sphere;
    collider.initRadius_ = radius;
    collider.initOffset_ = offset;
    return collider;
}

//----------------------------------------------------------------------------
void Collider3D::OnAttach()
{
    auto& manager = CollisionManager3D::Get();
    handle_ = manager.Register(this, shape_);

    if (shape_ == ColliderShape3D::AABB) {
        manager.SetAABBSize(handle_, initSize_);
    } else if (shape_ == ColliderShape3D::Sphere) {
        manager.SetSphereRadius(handle_, initRadius_);
    }

    manager.SetOffset(handle_, initOffset_);
    manager.SetLayer(handle_, initLayer_);
    manager.SetMask(handle_, initMask_);
    manager.SetTrigger(handle_, initTrigger_);

    // 初期位置を設定
    if (Transform* transform = GetOwner()->GetComponent<Transform>()) {
        Vector3 pos = transform->GetPosition3D();
        manager.SetPosition(handle_, pos);
    }
}

//----------------------------------------------------------------------------
void Collider3D::OnDetach()
{
    if (handle_.IsValid()) {
        CollisionManager3D::Get().Unregister(handle_);
        handle_ = Collider3DHandle();
    }
}

//----------------------------------------------------------------------------
void Collider3D::Update(float deltaTime)
{
    (void)deltaTime;

    if (!handle_.IsValid()) return;
    if (!syncWithTransform_) return;

    if (Transform* transform = GetOwner()->GetComponent<Transform>()) {
        Vector3 pos = transform->GetPosition3D();
        CollisionManager3D::Get().SetPosition(handle_, pos);
    }
}

//----------------------------------------------------------------------------
void Collider3D::SetPosition(const Vector3& pos)
{
    if (handle_.IsValid()) {
        CollisionManager3D::Get().SetPosition(handle_, pos);
    }
}

//----------------------------------------------------------------------------
void Collider3D::SetPosition(float x, float y, float z)
{
    SetPosition(Vector3(x, y, z));
}

//----------------------------------------------------------------------------
void Collider3D::SetSize(const Vector3& size)
{
    initSize_ = size;
    if (handle_.IsValid()) {
        CollisionManager3D::Get().SetAABBSize(handle_, size);
    }
}

//----------------------------------------------------------------------------
void Collider3D::SetSize(float w, float h, float d)
{
    SetSize(Vector3(w, h, d));
}

//----------------------------------------------------------------------------
Vector3 Collider3D::GetSize() const
{
    if (handle_.IsValid()) {
        return CollisionManager3D::Get().GetSize(handle_);
    }
    return initSize_;
}

//----------------------------------------------------------------------------
void Collider3D::SetRadius(float radius)
{
    initRadius_ = radius;
    if (handle_.IsValid()) {
        CollisionManager3D::Get().SetSphereRadius(handle_, radius);
    }
}

//----------------------------------------------------------------------------
float Collider3D::GetRadius() const
{
    if (handle_.IsValid()) {
        return CollisionManager3D::Get().GetRadius(handle_);
    }
    return initRadius_;
}

//----------------------------------------------------------------------------
void Collider3D::SetOffset(const Vector3& offset)
{
    initOffset_ = offset;
    if (handle_.IsValid()) {
        CollisionManager3D::Get().SetOffset(handle_, offset);
    }
}

//----------------------------------------------------------------------------
void Collider3D::SetOffset(float x, float y, float z)
{
    SetOffset(Vector3(x, y, z));
}

//----------------------------------------------------------------------------
Vector3 Collider3D::GetOffset() const
{
    if (handle_.IsValid()) {
        return CollisionManager3D::Get().GetOffset(handle_);
    }
    return initOffset_;
}

//----------------------------------------------------------------------------
void Collider3D::SetLayer(uint8_t layer)
{
    initLayer_ = layer;
    if (handle_.IsValid()) {
        CollisionManager3D::Get().SetLayer(handle_, layer);
    }
}

//----------------------------------------------------------------------------
uint8_t Collider3D::GetLayer() const
{
    if (handle_.IsValid()) {
        return CollisionManager3D::Get().GetLayer(handle_);
    }
    return initLayer_;
}

//----------------------------------------------------------------------------
void Collider3D::SetMask(uint8_t mask)
{
    initMask_ = mask;
    if (handle_.IsValid()) {
        CollisionManager3D::Get().SetMask(handle_, mask);
    }
}

//----------------------------------------------------------------------------
uint8_t Collider3D::GetMask() const
{
    if (handle_.IsValid()) {
        return CollisionManager3D::Get().GetMask(handle_);
    }
    return initMask_;
}

//----------------------------------------------------------------------------
bool Collider3D::CanCollideWith(uint8_t otherLayer) const
{
    return (GetMask() & otherLayer) != 0;
}

//----------------------------------------------------------------------------
void Collider3D::SetTrigger(bool trigger)
{
    initTrigger_ = trigger;
    if (handle_.IsValid()) {
        CollisionManager3D::Get().SetTrigger(handle_, trigger);
    }
}

//----------------------------------------------------------------------------
bool Collider3D::IsTrigger() const
{
    if (handle_.IsValid()) {
        return CollisionManager3D::Get().IsTrigger(handle_);
    }
    return initTrigger_;
}

//----------------------------------------------------------------------------
void Collider3D::SetColliderEnabled(bool enabled)
{
    if (handle_.IsValid()) {
        CollisionManager3D::Get().SetEnabled(handle_, enabled);
    }
}

//----------------------------------------------------------------------------
bool Collider3D::IsColliderEnabled() const
{
    if (handle_.IsValid()) {
        return CollisionManager3D::Get().IsEnabled(handle_);
    }
    return true;
}

//----------------------------------------------------------------------------
AABB3D Collider3D::GetAABB() const
{
    if (handle_.IsValid()) {
        return CollisionManager3D::Get().GetAABB(handle_);
    }
    return AABB3D();
}

//----------------------------------------------------------------------------
BoundingSphere3D Collider3D::GetBoundingSphere() const
{
    if (handle_.IsValid()) {
        return CollisionManager3D::Get().GetBoundingSphere(handle_);
    }
    return BoundingSphere3D();
}

//----------------------------------------------------------------------------
void Collider3D::SetOnCollision(CollisionCallback3D callback)
{
    if (handle_.IsValid()) {
        CollisionManager3D::Get().SetOnCollision(handle_, std::move(callback));
    }
}

//----------------------------------------------------------------------------
void Collider3D::SetOnCollisionEnter(CollisionCallback3D callback)
{
    if (handle_.IsValid()) {
        CollisionManager3D::Get().SetOnCollisionEnter(handle_, std::move(callback));
    }
}

//----------------------------------------------------------------------------
void Collider3D::SetOnCollisionExit(CollisionCallback3D callback)
{
    if (handle_.IsValid()) {
        CollisionManager3D::Get().SetOnCollisionExit(handle_, std::move(callback));
    }
}
