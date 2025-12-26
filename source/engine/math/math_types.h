//----------------------------------------------------------------------------
//! @file   math_types.h
//! @brief  数学型定義（DirectXTK SimpleMath使用）
//----------------------------------------------------------------------------
#pragma once

#include <SimpleMath.h>

//===========================================================================
//! DirectXTK SimpleMath型のエイリアス
//===========================================================================
using Vector2    = DirectX::SimpleMath::Vector2;
using Vector3    = DirectX::SimpleMath::Vector3;
using Vector4    = DirectX::SimpleMath::Vector4;
using Matrix     = DirectX::SimpleMath::Matrix;
using Matrix4x4  = DirectX::SimpleMath::Matrix;  // エイリアス
using Quaternion = DirectX::SimpleMath::Quaternion;
using Plane      = DirectX::SimpleMath::Plane;
using Ray        = DirectX::SimpleMath::Ray;
// Rectangle/Rect は Windows API と衝突するため定義しない
// 必要な場合は DirectX::SimpleMath::Rectangle を直接使用
using Viewport   = DirectX::SimpleMath::Viewport;

//===========================================================================
//! 追加の便利関数
//===========================================================================

//! 度をラジアンに変換
[[nodiscard]] constexpr float ToRadians(float degrees) noexcept {
    return degrees * (DirectX::XM_PI / 180.0f);
}

//! ラジアンを度に変換
[[nodiscard]] constexpr float ToDegrees(float radians) noexcept {
    return radians * (180.0f / DirectX::XM_PI);
}

//! 値をクランプ
template<typename T>
[[nodiscard]] constexpr T Clamp(T value, T min, T max) noexcept {
    return (value < min) ? min : (value > max) ? max : value;
}

//! 線形補間
template<typename T>
[[nodiscard]] constexpr T Lerp(const T& a, const T& b, float t) noexcept {
    return a + (b - a) * t;
}

//! 0〜1にクランプした線形補間
template<typename T>
[[nodiscard]] constexpr T LerpClamped(const T& a, const T& b, float t) noexcept {
    return Lerp(a, b, Clamp(t, 0.0f, 1.0f));
}

//===========================================================================
//! @brief 線分（2D）
//!
//! 始点から終点への線分を表す。
//! 主に「切」の縁切断判定で使用。
//===========================================================================
struct LineSegment {
    Vector2 start;  //!< 始点
    Vector2 end;    //!< 終点

    LineSegment() = default;
    LineSegment(const Vector2& s, const Vector2& e) : start(s), end(e) {}
    LineSegment(float x1, float y1, float x2, float y2)
        : start(x1, y1), end(x2, y2) {}

    //! @brief 線分の方向ベクトルを取得
    [[nodiscard]] Vector2 Direction() const noexcept {
        return end - start;
    }

    //! @brief 線分の長さを取得
    [[nodiscard]] float Length() const noexcept {
        return Direction().Length();
    }

    //! @brief 線分の長さの2乗を取得
    [[nodiscard]] float LengthSquared() const noexcept {
        return Direction().LengthSquared();
    }

    //! @brief 他の線分と交差するか判定
    //! @param other 判定対象の線分
    //! @return 交差する場合true
    [[nodiscard]] bool Intersects(const LineSegment& other) const noexcept {
        Vector2 intersection;
        return Intersects(other, intersection);
    }

    //! @brief 他の線分と交差するか判定（交点取得版）
    //! @param other 判定対象の線分
    //! @param[out] intersection 交点（交差する場合のみ有効）
    //! @return 交差する場合true
    [[nodiscard]] bool Intersects(const LineSegment& other, Vector2& intersection) const noexcept {
        // 線分AB と 線分CD の交差判定
        // A = this->start, B = this->end
        // C = other.start, D = other.end

        const Vector2 ab = end - start;
        const Vector2 cd = other.end - other.start;
        const Vector2 ac = other.start - start;

        // 外積: cross(v1, v2) = v1.x * v2.y - v1.y * v2.x
        const float cross_ab_cd = ab.x * cd.y - ab.y * cd.x;

        // 平行判定（外積が0に近い場合）
        constexpr float kEpsilon = 1e-6f;
        if (std::abs(cross_ab_cd) < kEpsilon) {
            return false;  // 平行（重なりは非交差として扱う）
        }

        // パラメータ t, u を計算
        // P = A + t * AB = C + u * CD
        const float t = (ac.x * cd.y - ac.y * cd.x) / cross_ab_cd;
        const float u = (ac.x * ab.y - ac.y * ab.x) / cross_ab_cd;

        // 両方のパラメータが [0, 1] 範囲内なら交差
        if (t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f) {
            intersection = start + ab * t;
            return true;
        }

        return false;
    }

    //! @brief 点までの最短距離を計算
    //! @param point 点
    //! @return 線分上の最近点までの距離
    [[nodiscard]] float DistanceToPoint(const Vector2& point) const noexcept {
        const Vector2 ab = end - start;
        const Vector2 ap = point - start;

        const float lengthSq = ab.LengthSquared();
        if (lengthSq < 1e-8f) {
            // 線分が点に縮退している場合
            return ap.Length();
        }

        // 射影パラメータをクランプ
        float t = Clamp(ap.Dot(ab) / lengthSq, 0.0f, 1.0f);

        // 最近点
        const Vector2 closest = start + ab * t;
        return (point - closest).Length();
    }
};
