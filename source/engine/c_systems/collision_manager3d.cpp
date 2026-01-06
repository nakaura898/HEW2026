//----------------------------------------------------------------------------
//! @file   collision_manager3d.cpp
//! @brief  3D衝突判定マネージャー実装
//----------------------------------------------------------------------------
#include "collision_manager3d.h"
#include "engine/component/collider3d.h"
#include <algorithm>
#include <cmath>

namespace {
    constexpr size_t kInitialCapacity = 256;
}

//----------------------------------------------------------------------------
void CollisionManager3D::Initialize(int cellSize)
{
    cellSize_ = cellSize > 0 ? cellSize : CollisionConstants3D::kDefaultCellSize;

    // 配列を初期容量で確保
    posX_.reserve(kInitialCapacity);
    posY_.reserve(kInitialCapacity);
    posZ_.reserve(kInitialCapacity);
    halfW_.reserve(kInitialCapacity);
    halfH_.reserve(kInitialCapacity);
    halfD_.reserve(kInitialCapacity);
    radius_.reserve(kInitialCapacity);
    shape_.reserve(kInitialCapacity);
    layer_.reserve(kInitialCapacity);
    mask_.reserve(kInitialCapacity);
    flags_.reserve(kInitialCapacity);
    offsetX_.reserve(kInitialCapacity);
    offsetY_.reserve(kInitialCapacity);
    offsetZ_.reserve(kInitialCapacity);
    sizeW_.reserve(kInitialCapacity);
    sizeH_.reserve(kInitialCapacity);
    sizeD_.reserve(kInitialCapacity);
    colliders_.reserve(kInitialCapacity);
    onCollision_.reserve(kInitialCapacity);
    onEnter_.reserve(kInitialCapacity);
    onExit_.reserve(kInitialCapacity);
    generations_.reserve(kInitialCapacity);
}

//----------------------------------------------------------------------------
void CollisionManager3D::Shutdown()
{
    Clear();
    grid_.clear();
    previousPairs_.clear();
    currentPairs_.clear();
    testedPairs_.clear();
}

//----------------------------------------------------------------------------
Collider3DHandle CollisionManager3D::Register(Collider3D* collider, ColliderShape3D shape)
{
    uint16_t index = AllocateIndex();
    Collider3DHandle handle;
    handle.index = index;
    handle.generation = generations_[index];

    colliders_[index] = collider;
    shape_[index] = static_cast<uint8_t>(shape);
    flags_[index] = kFlagEnabled;

    ++activeCount_;
    return handle;
}

//----------------------------------------------------------------------------
void CollisionManager3D::Unregister(Collider3DHandle handle)
{
    if (!IsValid(handle)) return;

    uint16_t index = handle.index;
    colliders_[index] = nullptr;
    onCollision_[index] = nullptr;
    onEnter_[index] = nullptr;
    onExit_[index] = nullptr;
    flags_[index] = 0;

    FreeIndex(index);
    --activeCount_;
}

//----------------------------------------------------------------------------
bool CollisionManager3D::IsValid(Collider3DHandle handle) const noexcept
{
    if (!handle.IsValid()) return false;
    if (handle.index >= generations_.size()) return false;
    return generations_[handle.index] == handle.generation;
}

//----------------------------------------------------------------------------
void CollisionManager3D::Clear()
{
    posX_.clear(); posY_.clear(); posZ_.clear();
    halfW_.clear(); halfH_.clear(); halfD_.clear();
    radius_.clear();
    shape_.clear();
    layer_.clear(); mask_.clear(); flags_.clear();
    offsetX_.clear(); offsetY_.clear(); offsetZ_.clear();
    sizeW_.clear(); sizeH_.clear(); sizeD_.clear();
    colliders_.clear();
    onCollision_.clear(); onEnter_.clear(); onExit_.clear();
    generations_.clear();
    freeIndices_.clear();
    activeCount_ = 0;
    grid_.clear();
}

//----------------------------------------------------------------------------
uint16_t CollisionManager3D::AllocateIndex()
{
    if (!freeIndices_.empty()) {
        uint16_t index = freeIndices_.back();
        freeIndices_.pop_back();
        return index;
    }

    // 新しいスロットを追加
    uint16_t index = static_cast<uint16_t>(colliders_.size());
    posX_.push_back(0); posY_.push_back(0); posZ_.push_back(0);
    halfW_.push_back(0); halfH_.push_back(0); halfD_.push_back(0);
    radius_.push_back(0);
    shape_.push_back(0);
    layer_.push_back(CollisionConstants3D::kDefaultLayer);
    mask_.push_back(CollisionConstants3D::kDefaultMask);
    flags_.push_back(0);
    offsetX_.push_back(0); offsetY_.push_back(0); offsetZ_.push_back(0);
    sizeW_.push_back(0); sizeH_.push_back(0); sizeD_.push_back(0);
    colliders_.push_back(nullptr);
    onCollision_.push_back(nullptr);
    onEnter_.push_back(nullptr);
    onExit_.push_back(nullptr);
    generations_.push_back(0);

    return index;
}

//----------------------------------------------------------------------------
void CollisionManager3D::FreeIndex(uint16_t index)
{
    ++generations_[index];
    freeIndices_.push_back(index);
}

//----------------------------------------------------------------------------
void CollisionManager3D::SetPosition(Collider3DHandle handle, const Vector3& pos)
{
    if (!IsValid(handle)) return;
    uint16_t i = handle.index;
    posX_[i] = pos.x + offsetX_[i];
    posY_[i] = pos.y + offsetY_[i];
    posZ_[i] = pos.z + offsetZ_[i];
}

//----------------------------------------------------------------------------
void CollisionManager3D::SetAABBSize(Collider3DHandle handle, const Vector3& size)
{
    if (!IsValid(handle)) return;
    uint16_t i = handle.index;
    sizeW_[i] = size.x;
    sizeH_[i] = size.y;
    sizeD_[i] = size.z;
    halfW_[i] = size.x * 0.5f;
    halfH_[i] = size.y * 0.5f;
    halfD_[i] = size.z * 0.5f;
}

//----------------------------------------------------------------------------
void CollisionManager3D::SetSphereRadius(Collider3DHandle handle, float r)
{
    if (!IsValid(handle)) return;
    radius_[handle.index] = r;
}

//----------------------------------------------------------------------------
void CollisionManager3D::SetOffset(Collider3DHandle handle, const Vector3& offset)
{
    if (!IsValid(handle)) return;
    uint16_t i = handle.index;
    offsetX_[i] = offset.x;
    offsetY_[i] = offset.y;
    offsetZ_[i] = offset.z;
}

//----------------------------------------------------------------------------
void CollisionManager3D::SetLayer(Collider3DHandle handle, uint8_t layer)
{
    if (!IsValid(handle)) return;
    layer_[handle.index] = layer;
}

//----------------------------------------------------------------------------
void CollisionManager3D::SetMask(Collider3DHandle handle, uint8_t mask)
{
    if (!IsValid(handle)) return;
    mask_[handle.index] = mask;
}

//----------------------------------------------------------------------------
void CollisionManager3D::SetEnabled(Collider3DHandle handle, bool enabled)
{
    if (!IsValid(handle)) return;
    if (enabled) {
        flags_[handle.index] |= kFlagEnabled;
    } else {
        flags_[handle.index] &= ~kFlagEnabled;
    }
}

//----------------------------------------------------------------------------
void CollisionManager3D::SetTrigger(Collider3DHandle handle, bool trigger)
{
    if (!IsValid(handle)) return;
    if (trigger) {
        flags_[handle.index] |= kFlagTrigger;
    } else {
        flags_[handle.index] &= ~kFlagTrigger;
    }
}

//----------------------------------------------------------------------------
void CollisionManager3D::SetOnCollision(Collider3DHandle handle, CollisionCallback3D cb)
{
    if (!IsValid(handle)) return;
    onCollision_[handle.index] = std::move(cb);
}

//----------------------------------------------------------------------------
void CollisionManager3D::SetOnCollisionEnter(Collider3DHandle handle, CollisionCallback3D cb)
{
    if (!IsValid(handle)) return;
    onEnter_[handle.index] = std::move(cb);
}

//----------------------------------------------------------------------------
void CollisionManager3D::SetOnCollisionExit(Collider3DHandle handle, CollisionCallback3D cb)
{
    if (!IsValid(handle)) return;
    onExit_[handle.index] = std::move(cb);
}

//----------------------------------------------------------------------------
AABB3D CollisionManager3D::GetAABB(Collider3DHandle handle) const
{
    if (!IsValid(handle)) return AABB3D();
    uint16_t i = handle.index;
    AABB3D aabb;
    aabb.minX = posX_[i] - halfW_[i];
    aabb.minY = posY_[i] - halfH_[i];
    aabb.minZ = posZ_[i] - halfD_[i];
    aabb.maxX = posX_[i] + halfW_[i];
    aabb.maxY = posY_[i] + halfH_[i];
    aabb.maxZ = posZ_[i] + halfD_[i];
    return aabb;
}

//----------------------------------------------------------------------------
BoundingSphere3D CollisionManager3D::GetBoundingSphere(Collider3DHandle handle) const
{
    if (!IsValid(handle)) return BoundingSphere3D();
    uint16_t i = handle.index;
    return BoundingSphere3D(Vector3(posX_[i], posY_[i], posZ_[i]), radius_[i]);
}

//----------------------------------------------------------------------------
Vector3 CollisionManager3D::GetSize(Collider3DHandle handle) const
{
    if (!IsValid(handle)) return Vector3::Zero;
    uint16_t i = handle.index;
    return Vector3(sizeW_[i], sizeH_[i], sizeD_[i]);
}

//----------------------------------------------------------------------------
float CollisionManager3D::GetRadius(Collider3DHandle handle) const
{
    if (!IsValid(handle)) return 0.0f;
    return radius_[handle.index];
}

//----------------------------------------------------------------------------
Vector3 CollisionManager3D::GetOffset(Collider3DHandle handle) const
{
    if (!IsValid(handle)) return Vector3::Zero;
    uint16_t i = handle.index;
    return Vector3(offsetX_[i], offsetY_[i], offsetZ_[i]);
}

//----------------------------------------------------------------------------
uint8_t CollisionManager3D::GetLayer(Collider3DHandle handle) const
{
    if (!IsValid(handle)) return 0;
    return layer_[handle.index];
}

//----------------------------------------------------------------------------
uint8_t CollisionManager3D::GetMask(Collider3DHandle handle) const
{
    if (!IsValid(handle)) return 0;
    return mask_[handle.index];
}

//----------------------------------------------------------------------------
bool CollisionManager3D::IsEnabled(Collider3DHandle handle) const
{
    if (!IsValid(handle)) return false;
    return (flags_[handle.index] & kFlagEnabled) != 0;
}

//----------------------------------------------------------------------------
bool CollisionManager3D::IsTrigger(Collider3DHandle handle) const
{
    if (!IsValid(handle)) return false;
    return (flags_[handle.index] & kFlagTrigger) != 0;
}

//----------------------------------------------------------------------------
ColliderShape3D CollisionManager3D::GetShape(Collider3DHandle handle) const
{
    if (!IsValid(handle)) return ColliderShape3D::AABB;
    return static_cast<ColliderShape3D>(shape_[handle.index]);
}

//----------------------------------------------------------------------------
Collider3D* CollisionManager3D::GetCollider(Collider3DHandle handle) const
{
    if (!IsValid(handle)) return nullptr;
    return colliders_[handle.index];
}

//----------------------------------------------------------------------------
void CollisionManager3D::Update(float deltaTime)
{
    accumulator_ += deltaTime;
    while (accumulator_ >= kFixedDeltaTime) {
        FixedUpdate();
        accumulator_ -= kFixedDeltaTime;
    }
}

//----------------------------------------------------------------------------
void CollisionManager3D::FixedUpdate()
{
    if (activeCount_ == 0) return;

    RebuildGrid();

    currentPairs_.clear();
    testedPairs_.clear();

    // グリッドベースの衝突判定
    for (auto& [cell, indices] : grid_) {
        size_t count = indices.size();
        for (size_t i = 0; i < count; ++i) {
            uint16_t idxA = indices[i];
            if ((flags_[idxA] & kFlagEnabled) == 0) continue;

            for (size_t j = i + 1; j < count; ++j) {
                uint16_t idxB = indices[j];
                if ((flags_[idxB] & kFlagEnabled) == 0) continue;

                // レイヤーマスクチェック
                if ((layer_[idxA] & mask_[idxB]) == 0 ||
                    (layer_[idxB] & mask_[idxA]) == 0) {
                    continue;
                }

                uint32_t pairKey = MakePairKey(idxA, idxB);

                // 重複チェック
                if (std::find(testedPairs_.begin(), testedPairs_.end(), pairKey) != testedPairs_.end()) {
                    continue;
                }
                testedPairs_.push_back(pairKey);

                if (TestCollision(idxA, idxB)) {
                    currentPairs_.push_back(pairKey);
                }
            }
        }
    }

    // 衝突ペアをソート
    std::sort(currentPairs_.begin(), currentPairs_.end());

    // Enter/Stay/Exit判定
    size_t prevIdx = 0, currIdx = 0;
    while (prevIdx < previousPairs_.size() || currIdx < currentPairs_.size()) {
        uint32_t prevKey = (prevIdx < previousPairs_.size())
            ? previousPairs_[prevIdx] : UINT32_MAX;
        uint32_t currKey = (currIdx < currentPairs_.size())
            ? currentPairs_[currIdx] : UINT32_MAX;

        if (prevKey < currKey) {
            // Exit
            uint16_t a = static_cast<uint16_t>(prevKey >> 16);
            uint16_t b = static_cast<uint16_t>(prevKey & 0xFFFF);
            if (onExit_[a] && colliders_[a] && colliders_[b]) {
                onExit_[a](colliders_[a], colliders_[b]);
            }
            if (onExit_[b] && colliders_[a] && colliders_[b]) {
                onExit_[b](colliders_[b], colliders_[a]);
            }
            ++prevIdx;
        } else if (currKey < prevKey) {
            // Enter
            uint16_t a = static_cast<uint16_t>(currKey >> 16);
            uint16_t b = static_cast<uint16_t>(currKey & 0xFFFF);
            if (onEnter_[a] && colliders_[a] && colliders_[b]) {
                onEnter_[a](colliders_[a], colliders_[b]);
            }
            if (onEnter_[b] && colliders_[a] && colliders_[b]) {
                onEnter_[b](colliders_[b], colliders_[a]);
            }
            if (onCollision_[a] && colliders_[a] && colliders_[b]) {
                onCollision_[a](colliders_[a], colliders_[b]);
            }
            if (onCollision_[b] && colliders_[a] && colliders_[b]) {
                onCollision_[b](colliders_[b], colliders_[a]);
            }
            ++currIdx;
        } else {
            // Stay
            uint16_t a = static_cast<uint16_t>(currKey >> 16);
            uint16_t b = static_cast<uint16_t>(currKey & 0xFFFF);
            if (onCollision_[a] && colliders_[a] && colliders_[b]) {
                onCollision_[a](colliders_[a], colliders_[b]);
            }
            if (onCollision_[b] && colliders_[a] && colliders_[b]) {
                onCollision_[b](colliders_[b], colliders_[a]);
            }
            ++prevIdx;
            ++currIdx;
        }
    }

    std::swap(previousPairs_, currentPairs_);
}

//----------------------------------------------------------------------------
bool CollisionManager3D::TestCollision(uint16_t indexA, uint16_t indexB) const
{
    ColliderShape3D shapeA = static_cast<ColliderShape3D>(shape_[indexA]);
    ColliderShape3D shapeB = static_cast<ColliderShape3D>(shape_[indexB]);

    // AABB vs AABB
    if (shapeA == ColliderShape3D::AABB && shapeB == ColliderShape3D::AABB) {
        float aMinX = posX_[indexA] - halfW_[indexA];
        float aMaxX = posX_[indexA] + halfW_[indexA];
        float aMinY = posY_[indexA] - halfH_[indexA];
        float aMaxY = posY_[indexA] + halfH_[indexA];
        float aMinZ = posZ_[indexA] - halfD_[indexA];
        float aMaxZ = posZ_[indexA] + halfD_[indexA];

        float bMinX = posX_[indexB] - halfW_[indexB];
        float bMaxX = posX_[indexB] + halfW_[indexB];
        float bMinY = posY_[indexB] - halfH_[indexB];
        float bMaxY = posY_[indexB] + halfH_[indexB];
        float bMinZ = posZ_[indexB] - halfD_[indexB];
        float bMaxZ = posZ_[indexB] + halfD_[indexB];

        return aMinX < bMaxX && aMaxX > bMinX &&
               aMinY < bMaxY && aMaxY > bMinY &&
               aMinZ < bMaxZ && aMaxZ > bMinZ;
    }

    // Sphere vs Sphere
    if (shapeA == ColliderShape3D::Sphere && shapeB == ColliderShape3D::Sphere) {
        float dx = posX_[indexA] - posX_[indexB];
        float dy = posY_[indexA] - posY_[indexB];
        float dz = posZ_[indexA] - posZ_[indexB];
        float distSq = dx * dx + dy * dy + dz * dz;
        float radiusSum = radius_[indexA] + radius_[indexB];
        return distSq < radiusSum * radiusSum;
    }

    // AABB vs Sphere (or Sphere vs AABB)
    uint16_t aabbIdx, sphereIdx;
    if (shapeA == ColliderShape3D::AABB) {
        aabbIdx = indexA;
        sphereIdx = indexB;
    } else {
        aabbIdx = indexB;
        sphereIdx = indexA;
    }

    float closestX = Clamp(posX_[sphereIdx],
                           posX_[aabbIdx] - halfW_[aabbIdx],
                           posX_[aabbIdx] + halfW_[aabbIdx]);
    float closestY = Clamp(posY_[sphereIdx],
                           posY_[aabbIdx] - halfH_[aabbIdx],
                           posY_[aabbIdx] + halfH_[aabbIdx]);
    float closestZ = Clamp(posZ_[sphereIdx],
                           posZ_[aabbIdx] - halfD_[aabbIdx],
                           posZ_[aabbIdx] + halfD_[aabbIdx]);

    float dx = posX_[sphereIdx] - closestX;
    float dy = posY_[sphereIdx] - closestY;
    float dz = posZ_[sphereIdx] - closestZ;
    float distSq = dx * dx + dy * dy + dz * dz;
    float r = radius_[sphereIdx];
    return distSq < r * r;
}

//----------------------------------------------------------------------------
CollisionManager3D::Cell CollisionManager3D::ToCell(float x, float y, float z) const noexcept
{
    return Cell{
        static_cast<int>(std::floor(x / cellSize_)),
        static_cast<int>(std::floor(y / cellSize_)),
        static_cast<int>(std::floor(z / cellSize_))
    };
}

//----------------------------------------------------------------------------
void CollisionManager3D::RebuildGrid()
{
    for (auto& [cell, indices] : grid_) {
        indices.clear();
    }

    size_t count = flags_.size();
    for (size_t i = 0; i < count; ++i) {
        if ((flags_[i] & kFlagEnabled) == 0) continue;
        if (!colliders_[i]) continue;

        // コライダーが占めるセル範囲を計算
        float halfSize = 0.0f;
        ColliderShape3D shape = static_cast<ColliderShape3D>(shape_[i]);
        if (shape == ColliderShape3D::Sphere) {
            halfSize = radius_[i];
        } else {
            // std::max({...})はWindowsマクロと競合するため手動で計算
            halfSize = halfW_[i];
            if (halfH_[i] > halfSize) halfSize = halfH_[i];
            if (halfD_[i] > halfSize) halfSize = halfD_[i];
        }

        Cell minCell = ToCell(posX_[i] - halfSize, posY_[i] - halfSize, posZ_[i] - halfSize);
        Cell maxCell = ToCell(posX_[i] + halfSize, posY_[i] + halfSize, posZ_[i] + halfSize);

        for (int cx = minCell.x; cx <= maxCell.x; ++cx) {
            for (int cy = minCell.y; cy <= maxCell.y; ++cy) {
                for (int cz = minCell.z; cz <= maxCell.z; ++cz) {
                    grid_[Cell{cx, cy, cz}].push_back(static_cast<uint16_t>(i));
                }
            }
        }
    }
}

//----------------------------------------------------------------------------
void CollisionManager3D::QueryAABB(const AABB3D& aabb, std::vector<Collider3D*>& results,
                                   uint8_t layerMask)
{
    results.clear();
    queryBuffer_.clear();

    Cell minCell = ToCell(aabb.minX, aabb.minY, aabb.minZ);
    Cell maxCell = ToCell(aabb.maxX, aabb.maxY, aabb.maxZ);

    for (int cx = minCell.x; cx <= maxCell.x; ++cx) {
        for (int cy = minCell.y; cy <= maxCell.y; ++cy) {
            for (int cz = minCell.z; cz <= maxCell.z; ++cz) {
                auto it = grid_.find(Cell{cx, cy, cz});
                if (it == grid_.end()) continue;

                for (uint16_t idx : it->second) {
                    if (std::find(queryBuffer_.begin(), queryBuffer_.end(), idx) != queryBuffer_.end()) {
                        continue;
                    }

                    if ((flags_[idx] & kFlagEnabled) == 0) continue;
                    if ((layer_[idx] & layerMask) == 0) continue;

                    // AABB交差判定
                    AABB3D colAABB;
                    colAABB.minX = posX_[idx] - halfW_[idx];
                    colAABB.maxX = posX_[idx] + halfW_[idx];
                    colAABB.minY = posY_[idx] - halfH_[idx];
                    colAABB.maxY = posY_[idx] + halfH_[idx];
                    colAABB.minZ = posZ_[idx] - halfD_[idx];
                    colAABB.maxZ = posZ_[idx] + halfD_[idx];

                    if (aabb.Intersects(colAABB)) {
                        queryBuffer_.push_back(idx);
                        results.push_back(colliders_[idx]);
                    }
                }
            }
        }
    }
}

//----------------------------------------------------------------------------
void CollisionManager3D::QuerySphere(const BoundingSphere3D& sphere, std::vector<Collider3D*>& results,
                                     uint8_t layerMask)
{
    results.clear();
    queryBuffer_.clear();

    float r = sphere.radius;
    Cell minCell = ToCell(sphere.center.x - r, sphere.center.y - r, sphere.center.z - r);
    Cell maxCell = ToCell(sphere.center.x + r, sphere.center.y + r, sphere.center.z + r);

    for (int cx = minCell.x; cx <= maxCell.x; ++cx) {
        for (int cy = minCell.y; cy <= maxCell.y; ++cy) {
            for (int cz = minCell.z; cz <= maxCell.z; ++cz) {
                auto it = grid_.find(Cell{cx, cy, cz});
                if (it == grid_.end()) continue;

                for (uint16_t idx : it->second) {
                    if (std::find(queryBuffer_.begin(), queryBuffer_.end(), idx) != queryBuffer_.end()) {
                        continue;
                    }

                    if ((flags_[idx] & kFlagEnabled) == 0) continue;
                    if ((layer_[idx] & layerMask) == 0) continue;

                    // 球交差判定
                    ColliderShape3D shape = static_cast<ColliderShape3D>(shape_[idx]);
                    bool intersects = false;

                    if (shape == ColliderShape3D::Sphere) {
                        BoundingSphere3D colSphere(Vector3(posX_[idx], posY_[idx], posZ_[idx]), radius_[idx]);
                        intersects = sphere.Intersects(colSphere);
                    } else {
                        AABB3D colAABB;
                        colAABB.minX = posX_[idx] - halfW_[idx];
                        colAABB.maxX = posX_[idx] + halfW_[idx];
                        colAABB.minY = posY_[idx] - halfH_[idx];
                        colAABB.maxY = posY_[idx] + halfH_[idx];
                        colAABB.minZ = posZ_[idx] - halfD_[idx];
                        colAABB.maxZ = posZ_[idx] + halfD_[idx];
                        intersects = sphere.Intersects(colAABB);
                    }

                    if (intersects) {
                        queryBuffer_.push_back(idx);
                        results.push_back(colliders_[idx]);
                    }
                }
            }
        }
    }
}

//----------------------------------------------------------------------------
std::optional<RaycastHit3D> CollisionManager3D::Raycast(
    const Vector3& origin, const Vector3& direction, float maxDistance,
    uint8_t layerMask)
{
    // 簡易実装: 全コライダーを走査
    RaycastHit3D closest;
    closest.distance = maxDistance;
    bool found = false;

    Vector3 dir = direction;
    dir.Normalize();

    size_t count = colliders_.size();
    for (size_t i = 0; i < count; ++i) {
        if ((flags_[i] & kFlagEnabled) == 0) continue;
        if (!colliders_[i]) continue;
        if ((layer_[i] & layerMask) == 0) continue;

        ColliderShape3D shape = static_cast<ColliderShape3D>(shape_[i]);

        if (shape == ColliderShape3D::Sphere) {
            // レイ vs 球
            Vector3 center(posX_[i], posY_[i], posZ_[i]);
            Vector3 oc = origin - center;
            float r = radius_[i];

            float a = dir.Dot(dir);
            float b = 2.0f * oc.Dot(dir);
            float c = oc.Dot(oc) - r * r;
            float discriminant = b * b - 4 * a * c;

            if (discriminant >= 0) {
                float t = (-b - std::sqrt(discriminant)) / (2.0f * a);
                if (t >= 0 && t < closest.distance) {
                    closest.distance = t;
                    closest.point = origin + dir * t;
                    closest.normal = closest.point - center;
                    closest.normal.Normalize();
                    closest.collider = colliders_[i];
                    found = true;
                }
            }
        } else {
            // レイ vs AABB（スラブ法）
            AABB3D aabb;
            aabb.minX = posX_[i] - halfW_[i];
            aabb.maxX = posX_[i] + halfW_[i];
            aabb.minY = posY_[i] - halfH_[i];
            aabb.maxY = posY_[i] + halfH_[i];
            aabb.minZ = posZ_[i] - halfD_[i];
            aabb.maxZ = posZ_[i] + halfD_[i];

            float tMin = 0.0f;
            float tMax = closest.distance;

            // X軸
            if (std::abs(dir.x) > 0.0001f) {
                float t1 = (aabb.minX - origin.x) / dir.x;
                float t2 = (aabb.maxX - origin.x) / dir.x;
                if (t1 > t2) std::swap(t1, t2);
                if (t1 > tMin) tMin = t1;
                if (t2 < tMax) tMax = t2;
                if (tMin > tMax) continue;
            } else if (origin.x < aabb.minX || origin.x > aabb.maxX) {
                continue;
            }

            // Y軸
            if (std::abs(dir.y) > 0.0001f) {
                float t1 = (aabb.minY - origin.y) / dir.y;
                float t2 = (aabb.maxY - origin.y) / dir.y;
                if (t1 > t2) std::swap(t1, t2);
                if (t1 > tMin) tMin = t1;
                if (t2 < tMax) tMax = t2;
                if (tMin > tMax) continue;
            } else if (origin.y < aabb.minY || origin.y > aabb.maxY) {
                continue;
            }

            // Z軸
            if (std::abs(dir.z) > 0.0001f) {
                float t1 = (aabb.minZ - origin.z) / dir.z;
                float t2 = (aabb.maxZ - origin.z) / dir.z;
                if (t1 > t2) std::swap(t1, t2);
                if (t1 > tMin) tMin = t1;
                if (t2 < tMax) tMax = t2;
                if (tMin > tMax) continue;
            } else if (origin.z < aabb.minZ || origin.z > aabb.maxZ) {
                continue;
            }

            if (tMin >= 0 && tMin < closest.distance) {
                closest.distance = tMin;
                closest.point = origin + dir * tMin;
                // 法線計算（簡易）
                Vector3 center = aabb.GetCenter();
                Vector3 toHit = closest.point - center;
                Vector3 halfSize = aabb.GetSize() * 0.5f;
                if (std::abs(toHit.x / halfSize.x) > std::abs(toHit.y / halfSize.y) &&
                    std::abs(toHit.x / halfSize.x) > std::abs(toHit.z / halfSize.z)) {
                    closest.normal = Vector3(toHit.x > 0 ? 1.0f : -1.0f, 0, 0);
                } else if (std::abs(toHit.y / halfSize.y) > std::abs(toHit.z / halfSize.z)) {
                    closest.normal = Vector3(0, toHit.y > 0 ? 1.0f : -1.0f, 0);
                } else {
                    closest.normal = Vector3(0, 0, toHit.z > 0 ? 1.0f : -1.0f);
                }
                closest.collider = colliders_[i];
                found = true;
            }
        }
    }

    if (found) {
        return closest;
    }
    return std::nullopt;
}
