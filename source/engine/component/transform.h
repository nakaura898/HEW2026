//----------------------------------------------------------------------------
//! @file   transform.h
//! @brief  トランスフォームコンポーネント
//----------------------------------------------------------------------------
#pragma once

#include "component.h"
#include "engine/math/math_types.h"
#include <vector>
#include <algorithm>

//============================================================================
//! @brief トランスフォームコンポーネント
//!
//! 位置・回転・スケールを管理する。
//! 親子階層をサポートし、ローカル/ワールド座標系の変換機能を提供。
//============================================================================
class Transform : public Component {
public:
    Transform() = default;

    //! @brief デストラクタ
    //! @note 親子関係を安全にクリーンアップ
    ~Transform() {
        // 親から自分を削除
        if (parent_) {
            auto& siblings = parent_->children_;
            siblings.erase(std::remove(siblings.begin(), siblings.end(), this), siblings.end());
            parent_ = nullptr;
        }
        // 子の親参照をクリア
        for (Transform* child : children_) {
            child->parent_ = nullptr;
        }
        children_.clear();
    }

    //------------------------------------------------------------------------
    //! @brief コンストラクタ
    //! @param position 初期位置
    //------------------------------------------------------------------------
    explicit Transform(const Vector2& position)
        : position_(position) {}

    //------------------------------------------------------------------------
    //! @brief コンストラクタ
    //! @param position 初期位置
    //! @param rotation 初期回転（ラジアン）
    //! @param scale 初期スケール
    //------------------------------------------------------------------------
    Transform(const Vector2& position, float rotation, const Vector2& scale)
        : position_(position), rotation_(rotation), scale_(scale) {}

    //------------------------------------------------------------------------
    // 位置
    //------------------------------------------------------------------------

    [[nodiscard]] const Vector2& GetPosition() const noexcept { return position_; }
    void SetPosition(const Vector2& position) noexcept {
        position_ = position;
        SetDirty();
    }
    void SetPosition(float x, float y) noexcept {
        position_.x = x;
        position_.y = y;
        SetDirty();
    }

    //! @brief Z座標を取得（深度）
    [[nodiscard]] float GetZ() const noexcept { return z_; }

    //! @brief Z座標を設定（深度）
    //! @param z Z座標（大きいほど手前、0.0〜1.0推奨）
    void SetZ(float z) noexcept {
        z_ = z;
        SetDirty();
    }

    //! @brief XYZ全てを設定
    void SetPosition(float x, float y, float z) noexcept {
        position_.x = x;
        position_.y = y;
        z_ = z;
        SetDirty();
    }

    //! @brief 3D位置を取得
    [[nodiscard]] Vector3 GetPosition3D() const noexcept {
        return Vector3(position_.x, position_.y, z_);
    }

    //! @brief 3D位置を設定
    void SetPosition3D(const Vector3& position) noexcept {
        position_.x = position.x;
        position_.y = position.y;
        z_ = position.z;
        SetDirty();
    }

    //! @brief 移動
    void Translate(const Vector2& delta) noexcept {
        position_ += delta;
        SetDirty();
    }
    void Translate(float dx, float dy) noexcept {
        position_.x += dx;
        position_.y += dy;
        SetDirty();
    }
    void Translate(float dx, float dy, float dz) noexcept {
        position_.x += dx;
        position_.y += dy;
        z_ += dz;
        SetDirty();
    }

    //------------------------------------------------------------------------
    // 回転
    //------------------------------------------------------------------------

    //! @brief 回転角度を取得（ラジアン）
    [[nodiscard]] float GetRotation() const noexcept { return rotation_; }

    //! @brief 回転角度を取得（度）
    [[nodiscard]] float GetRotationDegrees() const noexcept { return ToDegrees(rotation_); }

    //! @brief 回転角度を設定（ラジアン）
    void SetRotation(float radians) noexcept {
        rotation_ = radians;
        SetDirty();
    }

    //! @brief 回転角度を設定（度）
    void SetRotationDegrees(float degrees) noexcept {
        rotation_ = ToRadians(degrees);
        SetDirty();
    }

    //! @brief 回転を加算（ラジアン）
    void Rotate(float radians) noexcept {
        rotation_ += radians;
        SetDirty();
    }

    //! @brief 回転を加算（度）
    void RotateDegrees(float degrees) noexcept {
        rotation_ += ToRadians(degrees);
        SetDirty();
    }

    //------------------------------------------------------------------------
    // 3D回転（Quaternion）
    //------------------------------------------------------------------------

    //! @brief 3D回転モードを有効化
    void Enable3DRotation() noexcept {
        use3DRotation_ = true;
        SetDirty();
    }

    //! @brief 3D回転を取得
    [[nodiscard]] const Quaternion& GetRotation3D() const noexcept { return rotation3D_; }

    //! @brief 3D回転を設定（Quaternion）
    void SetRotation3D(const Quaternion& q) noexcept {
        rotation3D_ = q;
        use3DRotation_ = true;
        SetDirty();
    }

    //! @brief 3D回転を設定（オイラー角: pitch, yaw, roll）
    //! @param pitch X軸回転（ラジアン）
    //! @param yaw Y軸回転（ラジアン）
    //! @param roll Z軸回転（ラジアン）
    void SetRotation3D(float pitch, float yaw, float roll) noexcept {
        rotation3D_ = Quaternion::CreateFromYawPitchRoll(yaw, pitch, roll);
        use3DRotation_ = true;
        SetDirty();
    }

    //! @brief 軸周りに回転を追加
    //! @param axis 回転軸（正規化済み）
    //! @param angle 回転角（ラジアン）
    void Rotate3D(const Vector3& axis, float angle) noexcept {
        Quaternion delta = Quaternion::CreateFromAxisAngle(axis, angle);
        rotation3D_ = rotation3D_ * delta;
        use3DRotation_ = true;
        SetDirty();
    }

    //! @brief 3D回転モードかどうか
    [[nodiscard]] bool Is3DRotationEnabled() const noexcept { return use3DRotation_; }

    //! @brief 2D回転モードに戻す
    void Disable3DRotation() noexcept {
        use3DRotation_ = false;
        SetDirty();
    }

    //------------------------------------------------------------------------
    // スケール
    //------------------------------------------------------------------------

    [[nodiscard]] const Vector2& GetScale() const noexcept { return scale_; }
    void SetScale(const Vector2& scale) noexcept {
        scale_ = scale;
        SetDirty();
    }
    void SetScale(float uniformScale) noexcept {
        scale_.x = uniformScale;
        scale_.y = uniformScale;
        SetDirty();
    }
    void SetScale(float x, float y) noexcept {
        scale_.x = x;
        scale_.y = y;
        SetDirty();
    }

    //------------------------------------------------------------------------
    // ピボット（回転・スケールの中心点）
    //------------------------------------------------------------------------

    [[nodiscard]] const Vector2& GetPivot() const noexcept { return pivot_; }
    void SetPivot(const Vector2& pivot) noexcept {
        pivot_ = pivot;
        SetDirty();
    }
    void SetPivot(float x, float y) noexcept {
        pivot_.x = x;
        pivot_.y = y;
        SetDirty();
    }

    //------------------------------------------------------------------------
    // 親子階層
    //------------------------------------------------------------------------

    //! @brief 親を取得
    [[nodiscard]] Transform* GetParent() const noexcept { return parent_; }

    //! @brief 親を設定
    //! @param parent 新しい親（nullptrで親なし）
    void SetParent(Transform* parent) {
        if (parent_ == parent) return;

        // 循環参照チェック
        if (parent) {
            Transform* p = parent;
            while (p) {
                if (p == this) return;  // 自分が親の祖先にいる場合は無視
                p = p->parent_;
            }
        }

        // 古い親から削除
        if (parent_) {
            auto& siblings = parent_->children_;
            siblings.erase(std::remove(siblings.begin(), siblings.end(), this), siblings.end());
        }

        // 新しい親に追加
        parent_ = parent;
        if (parent_) {
            parent_->children_.push_back(this);
        }

        SetDirty();
    }

    //! @brief 子を追加
    void AddChild(Transform* child) {
        if (child && child != this) {
            child->SetParent(this);
        }
    }

    //! @brief 子を削除
    void RemoveChild(Transform* child) {
        if (child && child->parent_ == this) {
            child->SetParent(nullptr);
        }
    }

    //! @brief 全ての子を取得
    [[nodiscard]] const std::vector<Transform*>& GetChildren() const noexcept { return children_; }

    //! @brief 子の数を取得
    [[nodiscard]] size_t GetChildCount() const noexcept { return children_.size(); }

    //! @brief 親子関係を解除してルートにする
    void DetachFromParent() { SetParent(nullptr); }

    //! @brief 全ての子を解除
    void DetachAllChildren() {
        while (!children_.empty()) {
            children_.back()->SetParent(nullptr);
        }
    }

    //------------------------------------------------------------------------
    // ワールド座標（親の変換を考慮）
    //------------------------------------------------------------------------

    //! @brief ワールド位置を取得（XY）
    [[nodiscard]] Vector2 GetWorldPosition() {
        if (parent_) {
            Vector3 localPos(position_.x, position_.y, z_);
            Vector3 worldPos = Vector3::Transform(localPos, parent_->GetWorldMatrix());
            return Vector2(worldPos.x, worldPos.y);
        }
        return position_;
    }

    //! @brief ワールドZ座標を取得
    [[nodiscard]] float GetWorldZ() {
        if (parent_) {
            Vector3 localPos(position_.x, position_.y, z_);
            Vector3 worldPos = Vector3::Transform(localPos, parent_->GetWorldMatrix());
            return worldPos.z;
        }
        return z_;
    }

    //! @brief ワールド位置を取得（XYZ）
    [[nodiscard]] Vector3 GetWorldPosition3D() {
        if (parent_) {
            Vector3 localPos(position_.x, position_.y, z_);
            return Vector3::Transform(localPos, parent_->GetWorldMatrix());
        }
        return Vector3(position_.x, position_.y, z_);
    }

    //! @brief ワールド回転を取得（ラジアン）
    [[nodiscard]] float GetWorldRotation() {
        float worldRot = rotation_;
        Transform* p = parent_;
        while (p) {
            worldRot += p->rotation_;
            p = p->parent_;
        }
        return worldRot;
    }

    //! @brief ワールドスケールを取得
    [[nodiscard]] Vector2 GetWorldScale() {
        Vector2 worldScale = scale_;
        Transform* p = parent_;
        while (p) {
            worldScale.x *= p->scale_.x;
            worldScale.y *= p->scale_.y;
            p = p->parent_;
        }
        return worldScale;
    }

    //! @brief ワールド位置を設定（ローカル位置を逆算）
    void SetWorldPosition(const Vector2& worldPos) {
        if (parent_) {
            // 親の逆行列を使ってローカル位置を計算
            Matrix invParent = parent_->GetWorldMatrix().Invert();
            Vector3 worldPos3(worldPos.x, worldPos.y, z_);
            Vector3 localPos3 = Vector3::Transform(worldPos3, invParent);
            SetPosition(Vector2(localPos3.x, localPos3.y));
        } else {
            SetPosition(worldPos);
        }
    }

    //! @brief ワールド位置を設定（XYZ、ローカル位置を逆算）
    void SetWorldPosition3D(const Vector3& worldPos) {
        if (parent_) {
            Matrix invParent = parent_->GetWorldMatrix().Invert();
            Vector3 localPos3 = Vector3::Transform(worldPos, invParent);
            SetPosition(localPos3.x, localPos3.y, localPos3.z);
        } else {
            SetPosition3D(worldPos);
        }
    }

    //! @brief ワールド回転を設定（ローカル回転を逆算）
    void SetWorldRotation(float worldRot) {
        if (parent_) {
            float parentWorldRot = parent_->GetWorldRotation();
            SetRotation(worldRot - parentWorldRot);
        } else {
            SetRotation(worldRot);
        }
    }

    //------------------------------------------------------------------------
    // ワールド行列
    //------------------------------------------------------------------------

    //! @brief ワールド行列を取得
    //! @return 3x3相当の変換行列（Matrix4x4形式）
    [[nodiscard]] const Matrix& GetWorldMatrix() {
        if (dirty_) {
            UpdateWorldMatrix();
        }
        return worldMatrix_;
    }

    //! @brief 行列の再計算を強制
    void ForceUpdateMatrix() {
        dirty_ = true;
    }

private:
    //! @brief ダーティフラグを設定（子にも伝播）
    void SetDirty() noexcept {
        if (dirty_) return;  // 既にダーティなら子も既にダーティ
        dirty_ = true;
        for (Transform* child : children_) {
            child->SetDirty();
        }
    }

    void UpdateWorldMatrix() {
        // 変換順序: スケール → 回転 → 移動
        // ピボットを考慮: -pivot → scale → rotate → +pivot → translate

        Matrix pivotMat = Matrix::CreateTranslation(-pivot_.x, -pivot_.y, 0.0f);
        Matrix scaleMat = Matrix::CreateScale(scale_.x, scale_.y, 1.0f);

        // 回転行列：2Dモードと3Dモードで分岐
        Matrix rotMat;
        if (use3DRotation_) {
            rotMat = Matrix::CreateFromQuaternion(rotation3D_);
        } else {
            rotMat = Matrix::CreateRotationZ(rotation_);
        }

        Matrix pivotBackMat = Matrix::CreateTranslation(pivot_.x, pivot_.y, 0.0f);
        Matrix transMat = Matrix::CreateTranslation(position_.x, position_.y, z_);

        Matrix localMatrix = pivotMat * scaleMat * rotMat * pivotBackMat * transMat;

        // 親がいる場合は親のワールド行列を乗算
        if (parent_) {
            worldMatrix_ = localMatrix * parent_->GetWorldMatrix();
        } else {
            worldMatrix_ = localMatrix;
        }

        dirty_ = false;
    }

    // ローカル変換
    Vector2 position_ = Vector2::Zero;
    float z_ = 0.0f;                           //!< Z座標（深度）
    float rotation_ = 0.0f;                    //!< Z軸回転（ラジアン、2Dモード用）
    Quaternion rotation3D_ = Quaternion::Identity;  //!< 3D回転（Quaternion）
    bool use3DRotation_ = false;               //!< 3D回転モードフラグ
    Vector2 scale_ = Vector2::One;
    Vector2 pivot_ = Vector2::Zero;            //!< 回転・スケールの中心点

    // 階層構造
    Transform* parent_ = nullptr;
    std::vector<Transform*> children_;

    // キャッシュ
    Matrix worldMatrix_ = Matrix::Identity;
    bool dirty_ = true;
};
