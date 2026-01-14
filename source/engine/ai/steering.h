//----------------------------------------------------------------------------
//! @file   steering.h
//! @brief  ステアリング行動 - AI移動の基本アルゴリズム
//----------------------------------------------------------------------------
#pragma once

#include <DirectXMath.h>
#include <vector>
#include <cmath>
#include <random>

//----------------------------------------------------------------------------
//! @brief ステアリング行動名前空間
//! @details 敵AI、NPC、群れ行動などに使用する移動アルゴリズム
//----------------------------------------------------------------------------
namespace Steering
{
    using DirectX::SimpleMath::Vector2;
    //------------------------------------------------------------------------
    //! @brief 目標に向かう（Seek）
    //! @param position 現在位置
    //! @param target 目標位置
    //! @param maxSpeed 最大速度
    //! @return 速度ベクトル
    //------------------------------------------------------------------------
    inline Vector2 Seek(const Vector2& position, const Vector2& target, float maxSpeed)
    {
        Vector2 desired = target - position;
        float distance = desired.Length();

        if (distance < 0.001f) {
            return Vector2::Zero;
        }

        desired.Normalize();
        desired *= maxSpeed;

        return desired;
    }

    //------------------------------------------------------------------------
    //! @brief 目標から逃げる（Flee）
    //! @param position 現在位置
    //! @param threat 脅威位置
    //! @param maxSpeed 最大速度
    //! @return 速度ベクトル
    //------------------------------------------------------------------------
    inline Vector2 Flee(const Vector2& position, const Vector2& threat, float maxSpeed)
    {
        Vector2 desired = position - threat;
        float distance = desired.Length();

        if (distance < 0.001f) {
            return Vector2::Zero;
        }

        desired.Normalize();
        desired *= maxSpeed;

        return desired;
    }

    //------------------------------------------------------------------------
    //! @brief ランダム徘徊（Wander）
    //! @param position 現在位置
    //! @param wanderRadius 徘徊円の半径
    //! @param wanderAngle 現在の徘徊角度（参照で更新）
    //! @param angleChange 1フレームあたりの角度変化量（ラジアン）
    //! @return 速度ベクトル（正規化済み）
    //------------------------------------------------------------------------
    inline Vector2 Wander([[maybe_unused]] const Vector2& position, float wanderRadius, float& wanderAngle, float angleChange = 0.5f)
    {
        // スレッドセーフな乱数生成
        static thread_local std::mt19937 rng(std::random_device{}());
        std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

        // ランダムに角度を変更
        wanderAngle += dist(rng) * angleChange;

        // 徘徊ターゲットを計算
        Vector2 wanderTarget;
        wanderTarget.x = std::cos(wanderAngle) * wanderRadius;
        wanderTarget.y = std::sin(wanderAngle) * wanderRadius;

        wanderTarget.Normalize();
        return wanderTarget;
    }

    //------------------------------------------------------------------------
    //! @brief 目標に到着（Arrive）- 近づくと減速
    //! @param position 現在位置
    //! @param target 目標位置
    //! @param maxSpeed 最大速度
    //! @param slowRadius 減速開始距離
    //! @return 速度ベクトル
    //------------------------------------------------------------------------
    inline Vector2 Arrive(const Vector2& position, const Vector2& target, float maxSpeed, float slowRadius)
    {
        Vector2 desired = target - position;
        float distance = desired.Length();

        if (distance < 0.001f) {
            return Vector2::Zero;
        }

        desired.Normalize();

        // 減速領域内なら速度を下げる
        if (distance < slowRadius) {
            desired *= maxSpeed * (distance / slowRadius);
        } else {
            desired *= maxSpeed;
        }

        return desired;
    }

    //------------------------------------------------------------------------
    //! @brief 分離（Separation）- 近くのエージェントと距離を保つ
    //! @param position 現在位置
    //! @param neighbors 近くのエージェントの位置
    //! @param separationRadius 分離距離
    //! @return 分離力ベクトル
    //------------------------------------------------------------------------
    inline Vector2 Separation(const Vector2& position, const std::vector<Vector2>& neighbors, float separationRadius)
    {
        Vector2 steeringForce = Vector2::Zero;
        int count = 0;

        for (const Vector2& neighbor : neighbors) {
            Vector2 diff = position - neighbor;
            float distance = diff.Length();

            if (distance > 0.001f && distance < separationRadius) {
                // 距離に反比例した力を加える
                diff.Normalize();
                diff /= distance;  // 近いほど強い力
                steeringForce += diff;
                count++;
            }
        }

        if (count > 0) {
            steeringForce /= static_cast<float>(count);
        }

        return steeringForce;
    }

    //------------------------------------------------------------------------
    //! @brief 結合（Cohesion）- グループの中心に向かう
    //! @param position 現在位置
    //! @param neighbors 近くのエージェントの位置
    //! @param maxSpeed 最大速度
    //! @return 速度ベクトル
    //------------------------------------------------------------------------
    inline Vector2 Cohesion(const Vector2& position, const std::vector<Vector2>& neighbors, float maxSpeed)
    {
        if (neighbors.empty()) {
            return Vector2::Zero;
        }

        Vector2 center = Vector2::Zero;
        for (const Vector2& neighbor : neighbors) {
            center += neighbor;
        }
        center /= static_cast<float>(neighbors.size());

        return Seek(position, center, maxSpeed);
    }

    //------------------------------------------------------------------------
    //! @brief 整列（Alignment）- グループの平均速度に合わせる
    //! @param neighborVelocities 近くのエージェントの速度
    //! @return 平均速度ベクトル
    //------------------------------------------------------------------------
    inline Vector2 Alignment(const std::vector<Vector2>& neighborVelocities)
    {
        if (neighborVelocities.empty()) {
            return Vector2::Zero;
        }

        Vector2 average = Vector2::Zero;
        for (const Vector2& velocity : neighborVelocities) {
            average += velocity;
        }
        average /= static_cast<float>(neighborVelocities.size());

        return average;
    }

} // namespace Steering
