//----------------------------------------------------------------------------
//! @file   collision_manager.cpp
//! @brief  衝突判定マネージャー実装（DOD設計）
//----------------------------------------------------------------------------

#include "collision_manager.h"
#include "engine/component/collider2d.h"
#include <algorithm>
#include <cmath>

void CollisionManager::Initialize(int cellSize)
{
    cellSize_ = cellSize > 0 ? cellSize : 256;
    Clear();
}

void CollisionManager::Shutdown()
{
    Clear();
}

ColliderHandle CollisionManager::Register(Collider2D* collider)
{
    if (!collider) return ColliderHandle{};

    uint16_t index = AllocateIndex();

    // 配列サイズ確保
    size_t requiredSize = index + 1;
    if (posX_.size() < requiredSize) {
        posX_.resize(requiredSize);
        posY_.resize(requiredSize);
        halfW_.resize(requiredSize);
        halfH_.resize(requiredSize);
        layer_.resize(requiredSize);
        mask_.resize(requiredSize);
        flags_.resize(requiredSize);
        offsetX_.resize(requiredSize);
        offsetY_.resize(requiredSize);
        sizeW_.resize(requiredSize);
        sizeH_.resize(requiredSize);
        colliders_.resize(requiredSize);
        onCollision_.resize(requiredSize);
        onEnter_.resize(requiredSize);
        onExit_.resize(requiredSize);
        generations_.resize(requiredSize, 0);
    }

    // デフォルト値で初期化
    posX_[index] = 0.0f;
    posY_[index] = 0.0f;
    halfW_[index] = 0.0f;
    halfH_[index] = 0.0f;
    layer_[index] = 1;
    mask_[index] = 0xFF;
    flags_[index] = kFlagEnabled;
    offsetX_[index] = 0.0f;
    offsetY_[index] = 0.0f;
    sizeW_[index] = 0.0f;
    sizeH_[index] = 0.0f;
    colliders_[index] = collider;
    onCollision_[index] = nullptr;
    onEnter_[index] = nullptr;
    onExit_[index] = nullptr;

    ++activeCount_;

    return ColliderHandle{ index, generations_[index] };
}

void CollisionManager::Unregister(ColliderHandle handle)
{
    if (!IsValid(handle)) return;

    uint16_t index = handle.index;

    // 世代をインクリメント（古いハンドルを無効化）
    ++generations_[index];

    // データをクリア
    colliders_[index] = nullptr;
    onCollision_[index] = nullptr;
    onEnter_[index] = nullptr;
    onExit_[index] = nullptr;
    flags_[index] = 0;

    FreeIndex(index);
    --activeCount_;
}

bool CollisionManager::IsValid(ColliderHandle handle) const noexcept
{
    if (handle.index >= generations_.size()) return false;
    return generations_[handle.index] == handle.generation &&
           colliders_[handle.index] != nullptr;
}

void CollisionManager::Clear()
{
    posX_.clear();
    posY_.clear();
    halfW_.clear();
    halfH_.clear();
    layer_.clear();
    mask_.clear();
    flags_.clear();
    offsetX_.clear();
    offsetY_.clear();
    sizeW_.clear();
    sizeH_.clear();
    colliders_.clear();
    onCollision_.clear();
    onEnter_.clear();
    onExit_.clear();
    generations_.clear();
    freeIndices_.clear();
    activeCount_ = 0;
    grid_.clear();
    previousPairs_.clear();
    currentPairs_.clear();
    testedPairs_.clear();
}

uint16_t CollisionManager::AllocateIndex()
{
    if (!freeIndices_.empty()) {
        uint16_t index = freeIndices_.back();
        freeIndices_.pop_back();
        return index;
    }
    return static_cast<uint16_t>(posX_.size());
}

void CollisionManager::FreeIndex(uint16_t index)
{
    freeIndices_.push_back(index);
}

//----------------------------------------------------------------------------
// データ設定
//----------------------------------------------------------------------------

void CollisionManager::SetPosition(ColliderHandle handle, float x, float y)
{
    if (!IsValid(handle)) return;
    uint16_t i = handle.index;
    posX_[i] = x + offsetX_[i];
    posY_[i] = y + offsetY_[i];
}

void CollisionManager::SetSize(ColliderHandle handle, float w, float h)
{
    if (!IsValid(handle)) return;
    uint16_t i = handle.index;
    sizeW_[i] = w;
    sizeH_[i] = h;
    halfW_[i] = w * 0.5f;
    halfH_[i] = h * 0.5f;
}

void CollisionManager::SetOffset(ColliderHandle handle, float x, float y)
{
    if (!IsValid(handle)) return;
    uint16_t i = handle.index;
    offsetX_[i] = x;
    offsetY_[i] = y;
}

void CollisionManager::SetLayer(ColliderHandle handle, uint8_t layer)
{
    if (!IsValid(handle)) return;
    layer_[handle.index] = layer;
}

void CollisionManager::SetMask(ColliderHandle handle, uint8_t mask)
{
    if (!IsValid(handle)) return;
    mask_[handle.index] = mask;
}

void CollisionManager::SetEnabled(ColliderHandle handle, bool enabled)
{
    if (!IsValid(handle)) return;
    if (enabled) {
        flags_[handle.index] |= kFlagEnabled;
    } else {
        flags_[handle.index] &= ~kFlagEnabled;
    }
}

void CollisionManager::SetTrigger(ColliderHandle handle, bool trigger)
{
    if (!IsValid(handle)) return;
    if (trigger) {
        flags_[handle.index] |= kFlagTrigger;
    } else {
        flags_[handle.index] &= ~kFlagTrigger;
    }
}

void CollisionManager::SetOnCollision(ColliderHandle handle, CollisionCallback cb)
{
    if (!IsValid(handle)) return;
    onCollision_[handle.index] = std::move(cb);
}

void CollisionManager::SetOnCollisionEnter(ColliderHandle handle, CollisionCallback cb)
{
    if (!IsValid(handle)) return;
    onEnter_[handle.index] = std::move(cb);
}

void CollisionManager::SetOnCollisionExit(ColliderHandle handle, CollisionCallback cb)
{
    if (!IsValid(handle)) return;
    onExit_[handle.index] = std::move(cb);
}

//----------------------------------------------------------------------------
// データ取得
//----------------------------------------------------------------------------

AABB CollisionManager::GetAABB(ColliderHandle handle) const
{
    if (!IsValid(handle)) return AABB{};
    uint16_t i = handle.index;
    AABB aabb;
    aabb.minX = posX_[i] - halfW_[i];
    aabb.minY = posY_[i] - halfH_[i];
    aabb.maxX = posX_[i] + halfW_[i];
    aabb.maxY = posY_[i] + halfH_[i];
    return aabb;
}

uint8_t CollisionManager::GetLayer(ColliderHandle handle) const
{
    if (!IsValid(handle)) return 0;
    return layer_[handle.index];
}

uint8_t CollisionManager::GetMask(ColliderHandle handle) const
{
    if (!IsValid(handle)) return 0;
    return mask_[handle.index];
}

bool CollisionManager::IsEnabled(ColliderHandle handle) const
{
    if (!IsValid(handle)) return false;
    return (flags_[handle.index] & kFlagEnabled) != 0;
}

Collider2D* CollisionManager::GetCollider(ColliderHandle handle) const
{
    if (!IsValid(handle)) return nullptr;
    return colliders_[handle.index];
}

//----------------------------------------------------------------------------
// 更新
//----------------------------------------------------------------------------

void CollisionManager::Update(float deltaTime)
{
    accumulator_ += deltaTime;

    // 固定タイムステップで衝突判定を実行
    while (accumulator_ >= kFixedDeltaTime) {
        FixedUpdate();
        accumulator_ -= kFixedDeltaTime;
    }
}

void CollisionManager::FixedUpdate()
{
    // グリッド再構築
    RebuildGrid();

    // ペア入れ替え
    std::swap(previousPairs_, currentPairs_);
    currentPairs_.clear();
    testedPairs_.clear();

    // グリッドセルごとに衝突判定
    for (auto& [cell, indexList] : grid_) {
        size_t count = indexList.size();
        if (count < 2) continue;

        for (size_t i = 0; i + 1 < count; ++i) {
            for (size_t j = i + 1; j < count; ++j) {
                uint16_t idxA = indexList[i];
                uint16_t idxB = indexList[j];

                // 有効性チェック
                if ((flags_[idxA] & kFlagEnabled) == 0) continue;
                if ((flags_[idxB] & kFlagEnabled) == 0) continue;

                // 重複スキップ用キー
                uint32_t pairKey = MakePairKey(idxA, idxB);

                // レイヤーマスクチェック
                bool canCollide = (mask_[idxA] & layer_[idxB]) != 0 ||
                                  (mask_[idxB] & layer_[idxA]) != 0;
                if (!canCollide) continue;

                // AABB交差判定（インライン展開）
                float minAX = posX_[idxA] - halfW_[idxA];
                float maxAX = posX_[idxA] + halfW_[idxA];
                float minAY = posY_[idxA] - halfH_[idxA];
                float maxAY = posY_[idxA] + halfH_[idxA];

                float minBX = posX_[idxB] - halfW_[idxB];
                float maxBX = posX_[idxB] + halfW_[idxB];
                float minBY = posY_[idxB] - halfH_[idxB];
                float maxBY = posY_[idxB] + halfH_[idxB];

                bool intersects = minAX < maxBX && maxAX > minBX &&
                                  minAY < maxBY && maxAY > minBY;

                if (intersects) {
                    testedPairs_.push_back(pairKey);  // O(1)
                    currentPairs_.push_back(pairKey); // O(1)
                }
            }
        }
    }

    // ソート + 重複削除（まとめて処理）
    std::sort(currentPairs_.begin(), currentPairs_.end());
    currentPairs_.erase(
        std::unique(currentPairs_.begin(), currentPairs_.end()),
        currentPairs_.end()
    );

    // Enter/Stay/Exit判定（マージ比較）
    size_t prevIdx = 0, currIdx = 0;
    size_t prevSize = previousPairs_.size();
    size_t currSize = currentPairs_.size();

    while (prevIdx < prevSize || currIdx < currSize) {
        if (prevIdx >= prevSize) {
            // Enter
            uint32_t key = currentPairs_[currIdx++];
            uint16_t a = GetFirstIndex(key);
            uint16_t b = GetSecondIndex(key);
            Collider2D* colA = colliders_[a];
            Collider2D* colB = colliders_[b];
            if (colA && colB) {
                if (onEnter_[a]) onEnter_[a](colA, colB);
                if (onEnter_[b]) onEnter_[b](colB, colA);
                if (onCollision_[a]) onCollision_[a](colA, colB);
                if (onCollision_[b]) onCollision_[b](colB, colA);
            }
        }
        else if (currIdx >= currSize) {
            // Exit
            uint32_t key = previousPairs_[prevIdx++];
            uint16_t a = GetFirstIndex(key);
            uint16_t b = GetSecondIndex(key);
            Collider2D* colA = colliders_[a];
            Collider2D* colB = colliders_[b];
            if (colA && colB) {
                if (onExit_[a]) onExit_[a](colA, colB);
                if (onExit_[b]) onExit_[b](colB, colA);
            }
        }
        else {
            uint32_t prevKey = previousPairs_[prevIdx];
            uint32_t currKey = currentPairs_[currIdx];

            if (prevKey < currKey) {
                // Exit
                uint16_t a = GetFirstIndex(prevKey);
                uint16_t b = GetSecondIndex(prevKey);
                Collider2D* colA = colliders_[a];
                Collider2D* colB = colliders_[b];
                if (colA && colB) {
                    if (onExit_[a]) onExit_[a](colA, colB);
                    if (onExit_[b]) onExit_[b](colB, colA);
                }
                ++prevIdx;
            }
            else if (prevKey > currKey) {
                // Enter
                uint16_t a = GetFirstIndex(currKey);
                uint16_t b = GetSecondIndex(currKey);
                Collider2D* colA = colliders_[a];
                Collider2D* colB = colliders_[b];
                if (colA && colB) {
                    if (onEnter_[a]) onEnter_[a](colA, colB);
                    if (onEnter_[b]) onEnter_[b](colB, colA);
                    if (onCollision_[a]) onCollision_[a](colA, colB);
                    if (onCollision_[b]) onCollision_[b](colB, colA);
                }
                ++currIdx;
            }
            else {
                // Stay
                uint16_t a = GetFirstIndex(currKey);
                uint16_t b = GetSecondIndex(currKey);
                Collider2D* colA = colliders_[a];
                Collider2D* colB = colliders_[b];
                if (colA && colB) {
                    if (onCollision_[a]) onCollision_[a](colA, colB);
                    if (onCollision_[b]) onCollision_[b](colB, colA);
                }
                ++prevIdx;
                ++currIdx;
            }
        }
    }
}

//----------------------------------------------------------------------------
// クエリ
//----------------------------------------------------------------------------

void CollisionManager::QueryAABB(const AABB& aabb, std::vector<Collider2D*>& results, uint8_t layerMask)
{
    results.clear();

    Cell c0 = ToCell(aabb.minX, aabb.minY);
    Cell c1 = ToCell(aabb.maxX - 0.001f, aabb.maxY - 0.001f);

    // 重複チェック用
    std::vector<uint16_t> checked;

    for (int cy = c0.y; cy <= c1.y; ++cy) {
        for (int cx = c0.x; cx <= c1.x; ++cx) {
            auto it = grid_.find({cx, cy});
            if (it == grid_.end()) continue;

            for (uint16_t idx : it->second) {
                if ((flags_[idx] & kFlagEnabled) == 0) continue;
                if ((layer_[idx] & layerMask) == 0) continue;

                // 重複チェック（push_back + 後でソート）
                checked.push_back(idx);
            }
        }
    }

    // 重複削除
    std::sort(checked.begin(), checked.end());
    checked.erase(std::unique(checked.begin(), checked.end()), checked.end());

    // AABB判定
    for (uint16_t idx : checked) {
        float minX = posX_[idx] - halfW_[idx];
        float maxX = posX_[idx] + halfW_[idx];
        float minY = posY_[idx] - halfH_[idx];
        float maxY = posY_[idx] + halfH_[idx];

        if (aabb.minX < maxX && aabb.maxX > minX &&
            aabb.minY < maxY && aabb.maxY > minY) {
            results.push_back(colliders_[idx]);
        }
    }
}

void CollisionManager::QueryPoint(const Vector2& point, std::vector<Collider2D*>& results, uint8_t layerMask)
{
    results.clear();

    Cell cell = ToCell(point.x, point.y);
    auto it = grid_.find(cell);
    if (it == grid_.end()) return;

    for (uint16_t idx : it->second) {
        if ((flags_[idx] & kFlagEnabled) == 0) continue;
        if ((layer_[idx] & layerMask) == 0) continue;

        float minX = posX_[idx] - halfW_[idx];
        float maxX = posX_[idx] + halfW_[idx];
        float minY = posY_[idx] - halfH_[idx];
        float maxY = posY_[idx] + halfH_[idx];

        if (point.x >= minX && point.x < maxX &&
            point.y >= minY && point.y < maxY) {
            results.push_back(colliders_[idx]);
        }
    }
}

//----------------------------------------------------------------------------
// グリッド
//----------------------------------------------------------------------------

CollisionManager::Cell CollisionManager::ToCell(float x, float y) const noexcept
{
    return {
        static_cast<int>(std::floor(x / static_cast<float>(cellSize_))),
        static_cast<int>(std::floor(y / static_cast<float>(cellSize_)))
    };
}

void CollisionManager::RebuildGrid()
{
    for (auto& [cell, indexList] : grid_) {
        indexList.clear();
    }

    size_t count = colliders_.size();
    for (size_t i = 0; i < count; ++i) {
        if (!colliders_[i]) continue;
        if ((flags_[i] & kFlagEnabled) == 0) continue;

        float minX = posX_[i] - halfW_[i];
        float maxX = posX_[i] + halfW_[i];
        float minY = posY_[i] - halfH_[i];
        float maxY = posY_[i] + halfH_[i];

        Cell c0 = ToCell(minX, minY);
        Cell c1 = ToCell(maxX - 0.001f, maxY - 0.001f);

        for (int cy = c0.y; cy <= c1.y; ++cy) {
            for (int cx = c0.x; cx <= c1.x; ++cx) {
                grid_[{cx, cy}].push_back(static_cast<uint16_t>(i));
            }
        }
    }
}
