//----------------------------------------------------------------------------
//! @file   collision_manager3d.h
//! @brief  3D衝突判定マネージャー（DOD設計）
//!
//! @note スレッドセーフではない。メインスレッドからのみ呼び出すこと。
//----------------------------------------------------------------------------
#pragma once

#include "common/utility/non_copyable.h"
#include "engine/math/math_types.h"
#include <vector>
#include <unordered_map>
#include <functional>
#include <cstdint>
#include <optional>
#include <memory>
#include <cassert>

class Collider3D;
class GameObject;

//============================================================================
// 定数定義
//============================================================================
namespace CollisionConstants3D {
    static constexpr uint16_t kInvalidIndex = UINT16_MAX;   //!< 無効なインデックス
    static constexpr uint8_t kDefaultLayer = 0x01;          //!< デフォルトレイヤー
    static constexpr uint8_t kDefaultMask = 0xFF;           //!< デフォルトマスク
    static constexpr int kDefaultCellSize = 100;            //!< デフォルトセルサイズ
}

//============================================================================
//! @brief 3Dコライダー形状
//============================================================================
enum class ColliderShape3D : uint8_t {
    AABB,       //!< 軸並行バウンディングボックス
    Sphere,     //!< 球
    Capsule     //!< カプセル（将来実装用）
};

//============================================================================
//! @brief 3Dコライダーハンドル
//============================================================================
struct Collider3DHandle {
    uint16_t index = CollisionConstants3D::kInvalidIndex;
    uint16_t generation = 0;

    [[nodiscard]] bool IsValid() const noexcept {
        return index != CollisionConstants3D::kInvalidIndex;
    }
    bool operator==(const Collider3DHandle& other) const noexcept {
        return index == other.index && generation == other.generation;
    }
};

//============================================================================
//! @brief 3D AABB
//============================================================================
struct AABB3D {
    float minX = 0.0f, minY = 0.0f, minZ = 0.0f;
    float maxX = 0.0f, maxY = 0.0f, maxZ = 0.0f;

    AABB3D() = default;
    AABB3D(float x, float y, float z, float w, float h, float d)
        : minX(x), minY(y), minZ(z)
        , maxX(x + w), maxY(y + h), maxZ(z + d) {}

    [[nodiscard]] bool Intersects(const AABB3D& other) const noexcept {
        return minX < other.maxX && maxX > other.minX &&
               minY < other.maxY && maxY > other.minY &&
               minZ < other.maxZ && maxZ > other.minZ;
    }

    [[nodiscard]] Vector3 GetCenter() const noexcept {
        return Vector3((minX + maxX) * 0.5f,
                       (minY + maxY) * 0.5f,
                       (minZ + maxZ) * 0.5f);
    }

    [[nodiscard]] Vector3 GetSize() const noexcept {
        return Vector3(maxX - minX, maxY - minY, maxZ - minZ);
    }
};

//============================================================================
//! @brief バウンディング球
//============================================================================
struct BoundingSphere3D {
    Vector3 center = Vector3::Zero;
    float radius = 0.5f;

    BoundingSphere3D() = default;
    BoundingSphere3D(const Vector3& c, float r) : center(c), radius(r) {}

    [[nodiscard]] bool Intersects(const BoundingSphere3D& other) const noexcept {
        float distSq = Vector3::DistanceSquared(center, other.center);
        float radiusSum = radius + other.radius;
        return distSq < radiusSum * radiusSum;
    }

    [[nodiscard]] bool Intersects(const AABB3D& aabb) const noexcept {
        // 最近接点を計算
        float closestX = Clamp(center.x, aabb.minX, aabb.maxX);
        float closestY = Clamp(center.y, aabb.minY, aabb.maxY);
        float closestZ = Clamp(center.z, aabb.minZ, aabb.maxZ);

        float distSq = (center.x - closestX) * (center.x - closestX) +
                       (center.y - closestY) * (center.y - closestY) +
                       (center.z - closestZ) * (center.z - closestZ);
        return distSq < radius * radius;
    }
};

//============================================================================
//! @brief 衝突コールバック型
//============================================================================
using CollisionCallback3D = std::function<void(Collider3D*, Collider3D*)>;

//============================================================================
//! @brief レイキャストヒット情報
//============================================================================
struct RaycastHit3D {
    Collider3D* collider = nullptr;
    float distance = 0.0f;
    Vector3 point;
    Vector3 normal;
};

//============================================================================
//! @brief 3D衝突判定マネージャー（DOD設計）
//!
//! Structure of Arrays（SoA）でデータを保持し、
//! キャッシュ効率の良い衝突判定を行う。
//============================================================================
class CollisionManager3D final : private NonCopyableNonMovable {
public:
    //! @brief シングルトンインスタンス取得
    static CollisionManager3D& Get()
    {
        assert(instance_ && "CollisionManager3D::Create() must be called first");
        return *instance_;
    }

    //! @brief インスタンス生成
    static void Create()
    {
        if (!instance_) {
            instance_ = std::unique_ptr<CollisionManager3D>(new CollisionManager3D());
        }
    }

    //! @brief インスタンス破棄
    static void Destroy()
    {
        instance_.reset();
    }

    ~CollisionManager3D() = default;

    //------------------------------------------------------------------------
    // 初期化・終了
    //------------------------------------------------------------------------

    void Initialize(int cellSize = CollisionConstants3D::kDefaultCellSize);
    void Shutdown();

    //------------------------------------------------------------------------
    // コライダー登録
    //------------------------------------------------------------------------

    [[nodiscard]] Collider3DHandle Register(Collider3D* collider, ColliderShape3D shape);
    void Unregister(Collider3DHandle handle);
    [[nodiscard]] bool IsValid(Collider3DHandle handle) const noexcept;
    void Clear();

    //------------------------------------------------------------------------
    // データ設定
    //------------------------------------------------------------------------

    void SetPosition(Collider3DHandle handle, const Vector3& pos);
    void SetAABBSize(Collider3DHandle handle, const Vector3& size);
    void SetSphereRadius(Collider3DHandle handle, float radius);
    void SetOffset(Collider3DHandle handle, const Vector3& offset);
    void SetLayer(Collider3DHandle handle, uint8_t layer);
    void SetMask(Collider3DHandle handle, uint8_t mask);
    void SetEnabled(Collider3DHandle handle, bool enabled);
    void SetTrigger(Collider3DHandle handle, bool trigger);

    void SetOnCollision(Collider3DHandle handle, CollisionCallback3D cb);
    void SetOnCollisionEnter(Collider3DHandle handle, CollisionCallback3D cb);
    void SetOnCollisionExit(Collider3DHandle handle, CollisionCallback3D cb);

    //------------------------------------------------------------------------
    // データ取得
    //------------------------------------------------------------------------

    [[nodiscard]] AABB3D GetAABB(Collider3DHandle handle) const;
    [[nodiscard]] BoundingSphere3D GetBoundingSphere(Collider3DHandle handle) const;
    [[nodiscard]] Vector3 GetSize(Collider3DHandle handle) const;
    [[nodiscard]] float GetRadius(Collider3DHandle handle) const;
    [[nodiscard]] Vector3 GetOffset(Collider3DHandle handle) const;
    [[nodiscard]] uint8_t GetLayer(Collider3DHandle handle) const;
    [[nodiscard]] uint8_t GetMask(Collider3DHandle handle) const;
    [[nodiscard]] bool IsEnabled(Collider3DHandle handle) const;
    [[nodiscard]] bool IsTrigger(Collider3DHandle handle) const;
    [[nodiscard]] ColliderShape3D GetShape(Collider3DHandle handle) const;
    [[nodiscard]] Collider3D* GetCollider(Collider3DHandle handle) const;

    //------------------------------------------------------------------------
    // 更新
    //------------------------------------------------------------------------

    void Update(float deltaTime);
    [[nodiscard]] static constexpr float GetFixedDeltaTime() noexcept { return kFixedDeltaTime; }

    //------------------------------------------------------------------------
    // 統計
    //------------------------------------------------------------------------

    [[nodiscard]] size_t GetColliderCount() const noexcept { return activeCount_; }

    //------------------------------------------------------------------------
    // クエリ
    //------------------------------------------------------------------------

    void QueryAABB(const AABB3D& aabb, std::vector<Collider3D*>& results,
                   uint8_t layerMask = CollisionConstants3D::kDefaultMask);

    void QuerySphere(const BoundingSphere3D& sphere, std::vector<Collider3D*>& results,
                     uint8_t layerMask = CollisionConstants3D::kDefaultMask);

    [[nodiscard]] std::optional<RaycastHit3D> Raycast(
        const Vector3& origin, const Vector3& direction, float maxDistance,
        uint8_t layerMask = CollisionConstants3D::kDefaultMask);

private:
    CollisionManager3D() = default;

    static inline std::unique_ptr<CollisionManager3D> instance_ = nullptr;

    void FixedUpdate();

    [[nodiscard]] uint16_t AllocateIndex();
    void FreeIndex(uint16_t index);

    [[nodiscard]] static uint32_t MakePairKey(uint16_t a, uint16_t b) noexcept {
        if (a > b) { uint16_t t = a; a = b; b = t; }
        return (static_cast<uint32_t>(a) << 16) | b;
    }

    [[nodiscard]] bool TestCollision(uint16_t indexA, uint16_t indexB) const;

    //------------------------------------------------------------------------
    // グリッド
    //------------------------------------------------------------------------

    struct Cell {
        int x, y, z;
        bool operator==(const Cell& other) const noexcept {
            return x == other.x && y == other.y && z == other.z;
        }
    };

    struct CellHash {
        size_t operator()(const Cell& c) const noexcept {
            auto h1 = std::hash<int>{}(c.x);
            auto h2 = std::hash<int>{}(c.y);
            auto h3 = std::hash<int>{}(c.z);
            size_t seed = h1;
            seed ^= h2 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= h3 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            return seed;
        }
    };

    [[nodiscard]] Cell ToCell(float x, float y, float z) const noexcept;
    void RebuildGrid();

    //------------------------------------------------------------------------
    // SoA構造
    //------------------------------------------------------------------------

    // ホットデータ
    std::vector<float> posX_, posY_, posZ_;
    std::vector<float> halfW_, halfH_, halfD_;  // AABB半サイズ
    std::vector<float> radius_;                  // 球半径
    std::vector<uint8_t> shape_;                 // ColliderShape3D
    std::vector<uint8_t> layer_, mask_, flags_;

    // ウォームデータ
    std::vector<float> offsetX_, offsetY_, offsetZ_;
    std::vector<float> sizeW_, sizeH_, sizeD_;

    // コールドデータ
    std::vector<Collider3D*> colliders_;
    std::vector<CollisionCallback3D> onCollision_;
    std::vector<CollisionCallback3D> onEnter_;
    std::vector<CollisionCallback3D> onExit_;

    // 世代管理
    std::vector<uint16_t> generations_;

    // フリーリスト
    std::vector<uint16_t> freeIndices_;
    size_t activeCount_ = 0;

    // 空間ハッシュグリッド
    int cellSize_ = CollisionConstants3D::kDefaultCellSize;
    std::unordered_map<Cell, std::vector<uint16_t>, CellHash> grid_;

    // 衝突ペア
    std::vector<uint32_t> previousPairs_;
    std::vector<uint32_t> currentPairs_;
    std::vector<uint32_t> testedPairs_;

    // フラグビット定義
    static constexpr uint8_t kFlagEnabled = 0x01;
    static constexpr uint8_t kFlagTrigger = 0x02;

    // 固定タイムステップ
    static constexpr float kFixedDeltaTime = 1.0f / 60.0f;
    float accumulator_ = 0.0f;

    // クエリ用バッファ
    mutable std::vector<uint16_t> queryBuffer_;
};
