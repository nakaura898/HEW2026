//----------------------------------------------------------------------------
//! @file   collider3d.h
//! @brief  3D当たり判定コンポーネント
//----------------------------------------------------------------------------
#pragma once

#include "component.h"
#include "engine/math/math_types.h"
#include "engine/c_systems/collision_manager3d.h"
#include <cstdint>

//============================================================================
//! @brief 3D当たり判定コンポーネント
//!
//! GameObjectにアタッチして3D当たり判定を追加する。
//! AABB、球、カプセル形状をサポート。
//! 実データはCollisionManager3Dが所有し、このクラスはハンドルのみ保持。
//============================================================================
class Collider3D : public Component {
public:
    Collider3D() = default;

    //------------------------------------------------------------------------
    //! @brief AABBコライダー作成
    //! @param size 当たり判定のサイズ
    //! @param offset 中心からのオフセット
    //------------------------------------------------------------------------
    static Collider3D CreateAABB(const Vector3& size, const Vector3& offset = Vector3::Zero);

    //------------------------------------------------------------------------
    //! @brief 球コライダー作成
    //! @param radius 半径
    //! @param offset 中心からのオフセット
    //------------------------------------------------------------------------
    static Collider3D CreateSphere(float radius, const Vector3& offset = Vector3::Zero);

    //------------------------------------------------------------------------
    // Component オーバーライド
    //------------------------------------------------------------------------

    void OnAttach() override;
    void OnDetach() override;
    void Update(float deltaTime) override;

    //------------------------------------------------------------------------
    // 位置（毎フレーム更新用）
    //------------------------------------------------------------------------

    //! @brief 位置を直接設定（Transformを使わない場合）
    void SetPosition(const Vector3& pos);
    void SetPosition(float x, float y, float z);

    //------------------------------------------------------------------------
    // サイズ（AABB用）
    //------------------------------------------------------------------------

    void SetSize(const Vector3& size);
    void SetSize(float w, float h, float d);
    [[nodiscard]] Vector3 GetSize() const;

    //------------------------------------------------------------------------
    // 半径（球用）
    //------------------------------------------------------------------------

    void SetRadius(float radius);
    [[nodiscard]] float GetRadius() const;

    //------------------------------------------------------------------------
    // オフセット
    //------------------------------------------------------------------------

    void SetOffset(const Vector3& offset);
    void SetOffset(float x, float y, float z);
    [[nodiscard]] Vector3 GetOffset() const;

    //------------------------------------------------------------------------
    // レイヤーとマスク
    //------------------------------------------------------------------------

    void SetLayer(uint8_t layer);
    [[nodiscard]] uint8_t GetLayer() const;

    void SetMask(uint8_t mask);
    [[nodiscard]] uint8_t GetMask() const;

    [[nodiscard]] bool CanCollideWith(uint8_t otherLayer) const;

    //------------------------------------------------------------------------
    // トリガーモード
    //------------------------------------------------------------------------

    void SetTrigger(bool trigger);
    [[nodiscard]] bool IsTrigger() const;

    //------------------------------------------------------------------------
    // 有効/無効
    //------------------------------------------------------------------------

    void SetColliderEnabled(bool enabled);
    [[nodiscard]] bool IsColliderEnabled() const;

    //------------------------------------------------------------------------
    // 形状取得
    //------------------------------------------------------------------------

    [[nodiscard]] ColliderShape3D GetShape() const noexcept { return shape_; }

    //------------------------------------------------------------------------
    // AABB/球取得
    //------------------------------------------------------------------------

    [[nodiscard]] AABB3D GetAABB() const;
    [[nodiscard]] BoundingSphere3D GetBoundingSphere() const;

    //------------------------------------------------------------------------
    // 衝突コールバック
    //------------------------------------------------------------------------

    void SetOnCollision(CollisionCallback3D callback);
    void SetOnCollisionEnter(CollisionCallback3D callback);
    void SetOnCollisionExit(CollisionCallback3D callback);

    //------------------------------------------------------------------------
    // ハンドル取得（内部使用）
    //------------------------------------------------------------------------

    [[nodiscard]] Collider3DHandle GetHandle() const noexcept { return handle_; }

    //------------------------------------------------------------------------
    // ユーザーデータ
    //------------------------------------------------------------------------

    void SetUserData(void* data) noexcept { userData_ = data; }
    [[nodiscard]] void* GetUserData() const noexcept { return userData_; }

    template<typename T>
    void SetUserData(T* data) noexcept { userData_ = static_cast<void*>(data); }

    template<typename T>
    [[nodiscard]] T* GetUserDataAs() const noexcept { return static_cast<T*>(userData_); }

    //------------------------------------------------------------------------
    // Transform同期設定
    //------------------------------------------------------------------------

    void SetSyncWithTransform(bool sync) noexcept { syncWithTransform_ = sync; }
    [[nodiscard]] bool IsSyncWithTransform() const noexcept { return syncWithTransform_; }

private:
    Collider3DHandle handle_;
    ColliderShape3D shape_ = ColliderShape3D::AABB;

    // 初期化用の一時保存
    Vector3 initSize_ = Vector3::One;
    float initRadius_ = 0.5f;
    Vector3 initOffset_ = Vector3::Zero;
    uint8_t initLayer_ = CollisionConstants3D::kDefaultLayer;
    uint8_t initMask_ = CollisionConstants3D::kDefaultMask;
    bool initTrigger_ = false;
    bool initEnabled_ = true;
    bool syncWithTransform_ = true;

    // コールバックのキャッシュ（OnAttach前に設定された場合用）
    CollisionCallback3D initOnCollision_;
    CollisionCallback3D initOnEnter_;
    CollisionCallback3D initOnExit_;

    void* userData_ = nullptr;
};
