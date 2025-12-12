//----------------------------------------------------------------------------
//! @file   collider2d.cpp
//! @brief  2D当たり判定コンポーネント実装
//----------------------------------------------------------------------------

#include "collider2d.h"
#include "transform2d.h"
#include "game_object.h"

Collider2D::Collider2D(const Vector2& size, const Vector2& offset)
    : initSize_(size), initOffset_(offset)
{
}

void Collider2D::OnAttach()
{
    handle_ = CollisionManager::Get().Register(this);

    // 初期値を設定
    CollisionManager::Get().SetSize(handle_, initSize_.x, initSize_.y);
    CollisionManager::Get().SetOffset(handle_, initOffset_.x, initOffset_.y);
    CollisionManager::Get().SetLayer(handle_, initLayer_);
    CollisionManager::Get().SetMask(handle_, initMask_);
    CollisionManager::Get().SetTrigger(handle_, initTrigger_);
}

void Collider2D::OnDetach()
{
    CollisionManager::Get().Unregister(handle_);
    handle_ = ColliderHandle{};
}

void Collider2D::Update([[maybe_unused]] float deltaTime)
{
    // Transform2Dと自動同期
    if (syncWithTransform_) {
        if (GameObject* owner = GetOwner()) {
            if (Transform2D* transform = owner->GetComponent<Transform2D>()) {
                Vector2 pos = transform->GetWorldPosition();
                CollisionManager::Get().SetPosition(handle_, pos.x, pos.y);
            }
        }
    }
}

//----------------------------------------------------------------------------
// 位置
//----------------------------------------------------------------------------

void Collider2D::SetPosition(float x, float y)
{
    syncWithTransform_ = false;  // 手動設定に切り替え
    CollisionManager::Get().SetPosition(handle_, x, y);
}

void Collider2D::SetPosition(const Vector2& pos)
{
    SetPosition(pos.x, pos.y);
}

//----------------------------------------------------------------------------
// サイズとオフセット
//----------------------------------------------------------------------------

void Collider2D::SetSize(float width, float height)
{
    initSize_ = Vector2(width, height);  // 常に保存
    if (handle_.IsValid()) {
        CollisionManager::Get().SetSize(handle_, width, height);
    }
}

void Collider2D::SetSize(const Vector2& size)
{
    SetSize(size.x, size.y);
}

Vector2 Collider2D::GetSize() const
{
    if (handle_.IsValid()) {
        // CollisionManagerから取得（sizeW_, sizeH_を公開する必要あり）
        // 簡易実装: AABBから計算
        AABB aabb = CollisionManager::Get().GetAABB(handle_);
        return Vector2(aabb.maxX - aabb.minX, aabb.maxY - aabb.minY);
    }
    return initSize_;
}

void Collider2D::SetOffset(float x, float y)
{
    initOffset_ = Vector2(x, y);  // 常に保存
    if (handle_.IsValid()) {
        CollisionManager::Get().SetOffset(handle_, x, y);
    }
}

void Collider2D::SetOffset(const Vector2& offset)
{
    SetOffset(offset.x, offset.y);
}

Vector2 Collider2D::GetOffset() const
{
    return initOffset_;  // 簡易実装
}

void Collider2D::SetBounds(const Vector2& min, const Vector2& max)
{
    // min = 左上、max = 右下（Transform位置からの相対座標）
    // X+ = 右、X- = 左、Y+ = 下、Y- = 上
    Vector2 size(max.x - min.x, max.y - min.y);
    // オフセット = 矩形の中心位置
    Vector2 offset(min.x + size.x * 0.5f, min.y + size.y * 0.5f);
    SetSize(size);
    SetOffset(offset);
}

//----------------------------------------------------------------------------
// レイヤーとマスク
//----------------------------------------------------------------------------

void Collider2D::SetLayer(uint8_t layer)
{
    if (handle_.IsValid()) {
        CollisionManager::Get().SetLayer(handle_, layer);
    } else {
        initLayer_ = layer;
    }
}

uint8_t Collider2D::GetLayer() const
{
    if (handle_.IsValid()) {
        return CollisionManager::Get().GetLayer(handle_);
    }
    return initLayer_;
}

void Collider2D::SetMask(uint8_t mask)
{
    if (handle_.IsValid()) {
        CollisionManager::Get().SetMask(handle_, mask);
    } else {
        initMask_ = mask;
    }
}

uint8_t Collider2D::GetMask() const
{
    if (handle_.IsValid()) {
        return CollisionManager::Get().GetMask(handle_);
    }
    return initMask_;
}

bool Collider2D::CanCollideWith(uint8_t otherLayer) const
{
    return (GetMask() & otherLayer) != 0;
}

//----------------------------------------------------------------------------
// トリガー
//----------------------------------------------------------------------------

void Collider2D::SetTrigger(bool trigger)
{
    if (handle_.IsValid()) {
        CollisionManager::Get().SetTrigger(handle_, trigger);
    } else {
        initTrigger_ = trigger;
    }
}

bool Collider2D::IsTrigger() const
{
    return initTrigger_;  // 簡易実装
}

//----------------------------------------------------------------------------
// 有効/無効
//----------------------------------------------------------------------------

void Collider2D::SetColliderEnabled(bool enabled)
{
    if (handle_.IsValid()) {
        CollisionManager::Get().SetEnabled(handle_, enabled);
    }
}

bool Collider2D::IsColliderEnabled() const
{
    if (handle_.IsValid()) {
        return CollisionManager::Get().IsEnabled(handle_);
    }
    return true;
}

//----------------------------------------------------------------------------
// AABB
//----------------------------------------------------------------------------

AABB Collider2D::GetAABB() const
{
    return CollisionManager::Get().GetAABB(handle_);
}

//----------------------------------------------------------------------------
// コールバック
//----------------------------------------------------------------------------

void Collider2D::SetOnCollision(CollisionCallback callback)
{
    CollisionManager::Get().SetOnCollision(handle_, std::move(callback));
}

void Collider2D::SetOnCollisionEnter(CollisionCallback callback)
{
    CollisionManager::Get().SetOnCollisionEnter(handle_, std::move(callback));
}

void Collider2D::SetOnCollisionExit(CollisionCallback callback)
{
    CollisionManager::Get().SetOnCollisionExit(handle_, std::move(callback));
}
