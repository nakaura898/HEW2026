//----------------------------------------------------------------------------
//! @file   collider2d.h
//! @brief  2D当たり判定コンポーネント（AABB）
//----------------------------------------------------------------------------
#pragma once

#include "component.h"
#include "engine/math/math_types.h"
#include "engine/c_systems/collision_manager.h"
#include <cstdint>

//============================================================================
//! @brief 2D当たり判定コンポーネント（AABB）
//!
//! GameObjectにアタッチして当たり判定を追加する。
//! 実データはCollisionManagerが所有し、このクラスはハンドルのみ保持。
//============================================================================
class Collider2D : public Component {
public:
    Collider2D() = default;

    //------------------------------------------------------------------------
    //! @brief コンストラクタ
    //! @param size 当たり判定のサイズ
    //! @param offset 中心からのオフセット
    //------------------------------------------------------------------------
    explicit Collider2D(const Vector2& size, const Vector2& offset = Vector2::Zero);

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
    void SetPosition(float x, float y);
    void SetPosition(const Vector2& pos);

    //------------------------------------------------------------------------
    // サイズとオフセット
    //------------------------------------------------------------------------

    void SetSize(float width, float height);
    void SetSize(const Vector2& size);
    [[nodiscard]] Vector2 GetSize() const;

    void SetOffset(float x, float y);
    void SetOffset(const Vector2& offset);
    [[nodiscard]] Vector2 GetOffset() const;

    //------------------------------------------------------------------------
    //! @brief 左上と右下の座標からコライダーを設定
    //! @param min 左上座標（Transform位置からの相対座標）
    //! @param max 右下座標（Transform位置からの相対座標）
    //------------------------------------------------------------------------
    void SetBounds(const Vector2& min, const Vector2& max);

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
    // AABB取得
    //------------------------------------------------------------------------

    [[nodiscard]] AABB GetAABB() const;

    //------------------------------------------------------------------------
    // 衝突コールバック
    //------------------------------------------------------------------------

    void SetOnCollision(CollisionCallback callback);
    void SetOnCollisionEnter(CollisionCallback callback);
    void SetOnCollisionExit(CollisionCallback callback);

    //------------------------------------------------------------------------
    // ハンドル取得（内部使用）
    //------------------------------------------------------------------------

    [[nodiscard]] ColliderHandle GetHandle() const noexcept { return handle_; }

    //------------------------------------------------------------------------
    // ユーザーデータ
    //------------------------------------------------------------------------

    void* userData = nullptr;

private:
    ColliderHandle handle_;

    // 初期化用の一時保存（OnAttach前に設定された値を保持）
    Vector2 initSize_ = Vector2::Zero;
    Vector2 initOffset_ = Vector2::Zero;
    uint8_t initLayer_ = 1;
    uint8_t initMask_ = 0xFF;
    bool initTrigger_ = false;
    bool syncWithTransform_ = true;  //!< Transform2Dと自動同期するか
};
