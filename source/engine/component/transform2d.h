//----------------------------------------------------------------------------
//! @file   transform2d.h
//! @brief  2Dトランスフォームコンポーネント
//----------------------------------------------------------------------------
#pragma once

#include "component.h"
#include "engine/math/math_types.h"
#include <vector>
#include <algorithm>

//============================================================================
//! @brief 2Dトランスフォームコンポーネント
//!
//! 2D空間での位置・回転・スケールを管理する。
//! 親子階層をサポートし、ローカル/ワールド座標系の変換機能を提供。
//============================================================================
class Transform2D : public Component {
public:
    Transform2D() = default;

    //------------------------------------------------------------------------
    //! @brief コンストラクタ
    //! @param position 初期位置
    //------------------------------------------------------------------------
    explicit Transform2D(const Vector2& position)
        : position_(position) {}

    //------------------------------------------------------------------------
    //! @brief コンストラクタ
    //! @param position 初期位置
    //! @param rotation 初期回転（ラジアン）
    //! @param scale 初期スケール
    //------------------------------------------------------------------------
    Transform2D(const Vector2& position, float rotation, const Vector2& scale)
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
    [[nodiscard]] Transform2D* GetParent() const noexcept { return parent_; }

    //! @brief 親を設定
    //! @param parent 新しい親（nullptrで親なし）
    void SetParent(Transform2D* parent) {
        if (parent_ == parent) return;

        // 循環参照チェック
        if (parent) {
            Transform2D* p = parent;
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
    void AddChild(Transform2D* child) {
        if (child && child != this) {
            child->SetParent(this);
        }
    }

    //! @brief 子を削除
    void RemoveChild(Transform2D* child) {
        if (child && child->parent_ == this) {
            child->SetParent(nullptr);
        }
    }

    //! @brief 全ての子を取得
    [[nodiscard]] const std::vector<Transform2D*>& GetChildren() const noexcept { return children_; }

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

    //! @brief ワールド位置を取得
    [[nodiscard]] Vector2 GetWorldPosition() {
        if (parent_) {
            Vector3 localPos(position_.x, position_.y, 0.0f);
            Vector3 worldPos = Vector3::Transform(localPos, parent_->GetWorldMatrix());
            return Vector2(worldPos.x, worldPos.y);
        }
        return position_;
    }

    //! @brief ワールド回転を取得（ラジアン）
    [[nodiscard]] float GetWorldRotation() {
        float worldRot = rotation_;
        Transform2D* p = parent_;
        while (p) {
            worldRot += p->rotation_;
            p = p->parent_;
        }
        return worldRot;
    }

    //! @brief ワールドスケールを取得
    [[nodiscard]] Vector2 GetWorldScale() {
        Vector2 worldScale = scale_;
        Transform2D* p = parent_;
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
            Vector3 worldPos3(worldPos.x, worldPos.y, 0.0f);
            Vector3 localPos3 = Vector3::Transform(worldPos3, invParent);
            SetPosition(Vector2(localPos3.x, localPos3.y));
        } else {
            SetPosition(worldPos);
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
        for (Transform2D* child : children_) {
            child->SetDirty();
        }
    }

    void UpdateWorldMatrix() {
        // 変換順序: スケール → 回転 → 移動
        // ピボットを考慮: -pivot → scale → rotate → +pivot → translate

        Matrix pivotMat = Matrix::CreateTranslation(-pivot_.x, -pivot_.y, 0.0f);
        Matrix scaleMat = Matrix::CreateScale(scale_.x, scale_.y, 1.0f);
        Matrix rotMat = Matrix::CreateRotationZ(rotation_);
        Matrix pivotBackMat = Matrix::CreateTranslation(pivot_.x, pivot_.y, 0.0f);
        Matrix transMat = Matrix::CreateTranslation(position_.x, position_.y, 0.0f);

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
    float rotation_ = 0.0f;  //!< ラジアン
    Vector2 scale_ = Vector2::One;
    Vector2 pivot_ = Vector2::Zero;  //!< 回転・スケールの中心点

    // 階層構造
    Transform2D* parent_ = nullptr;
    std::vector<Transform2D*> children_;

    // キャッシュ
    Matrix worldMatrix_ = Matrix::Identity;
    bool dirty_ = true;
};
