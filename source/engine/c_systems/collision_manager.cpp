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
    cellSize_ = cellSize > 0 ? cellSize : CollisionConstants::kDefaultCellSize;
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
    layer_[index] = CollisionConstants::kDefaultLayer;
    mask_[index] = CollisionConstants::kDefaultMask;
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
    eventQueue_.clear();
    processingEvents_ = false;
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

Vector2 CollisionManager::GetSize(ColliderHandle handle) const
{
    if (!IsValid(handle)) return Vector2::Zero;
    uint16_t i = handle.index;
    return Vector2(sizeW_[i], sizeH_[i]);
}

Vector2 CollisionManager::GetOffset(ColliderHandle handle) const
{
    if (!IsValid(handle)) return Vector2::Zero;
    uint16_t i = handle.index;
    return Vector2(offsetX_[i], offsetY_[i]);
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

bool CollisionManager::IsTrigger(ColliderHandle handle) const
{
    if (!IsValid(handle)) return false;
    return (flags_[handle.index] & kFlagTrigger) != 0;
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

    // Enter/Stay/Exit判定（マージ比較）- イベントをキューに追加
    size_t prevIdx = 0, currIdx = 0;
    size_t prevSize = previousPairs_.size();
    size_t currSize = currentPairs_.size();

    while (prevIdx < prevSize || currIdx < currSize) {
        if (prevIdx >= prevSize) {
            // Enter + Stay
            uint32_t key = currentPairs_[currIdx++];
            uint16_t a = GetFirstIndex(key);
            uint16_t b = GetSecondIndex(key);
            eventQueue_.push_back({CollisionEventType::Enter, a, b,
                                   generations_[a], generations_[b]});
            eventQueue_.push_back({CollisionEventType::Stay, a, b,
                                   generations_[a], generations_[b]});
        }
        else if (currIdx >= currSize) {
            // Exit
            uint32_t key = previousPairs_[prevIdx++];
            uint16_t a = GetFirstIndex(key);
            uint16_t b = GetSecondIndex(key);
            eventQueue_.push_back({CollisionEventType::Exit, a, b,
                                   generations_[a], generations_[b]});
        }
        else {
            uint32_t prevKey = previousPairs_[prevIdx];
            uint32_t currKey = currentPairs_[currIdx];

            if (prevKey < currKey) {
                // Exit
                uint16_t a = GetFirstIndex(prevKey);
                uint16_t b = GetSecondIndex(prevKey);
                eventQueue_.push_back({CollisionEventType::Exit, a, b,
                                       generations_[a], generations_[b]});
                ++prevIdx;
            }
            else if (prevKey > currKey) {
                // Enter + Stay
                uint16_t a = GetFirstIndex(currKey);
                uint16_t b = GetSecondIndex(currKey);
                eventQueue_.push_back({CollisionEventType::Enter, a, b,
                                       generations_[a], generations_[b]});
                eventQueue_.push_back({CollisionEventType::Stay, a, b,
                                       generations_[a], generations_[b]});
                ++currIdx;
            }
            else {
                // Stay
                uint16_t a = GetFirstIndex(currKey);
                uint16_t b = GetSecondIndex(currKey);
                eventQueue_.push_back({CollisionEventType::Stay, a, b,
                                       generations_[a], generations_[b]});
                ++prevIdx;
                ++currIdx;
            }
        }
    }

    // 衝突検出完了後にイベントを処理
    ProcessEventQueue();
}

//----------------------------------------------------------------------------
// イベントキュー処理
//----------------------------------------------------------------------------

void CollisionManager::ProcessEventQueue()
{
    // 再入防止（コールバック内でUpdate()が呼ばれた場合を防ぐ）
    if (processingEvents_) return;
    processingEvents_ = true;

    for (const CollisionEvent& evt : eventQueue_) {
        // 世代チェック（コールバック中に削除された場合をスキップ）
        if (evt.indexA >= generations_.size() ||
            evt.indexB >= generations_.size() ||
            generations_[evt.indexA] != evt.generationA ||
            generations_[evt.indexB] != evt.generationB) {
            continue;
        }

        Collider2D* colA = colliders_[evt.indexA];
        Collider2D* colB = colliders_[evt.indexB];
        if (!colA || !colB) continue;

        // 1つ目のコールバックを発火
        switch (evt.type) {
        case CollisionEventType::Enter:
            if (onEnter_[evt.indexA]) onEnter_[evt.indexA](colA, colB);
            break;
        case CollisionEventType::Stay:
            if (onCollision_[evt.indexA]) onCollision_[evt.indexA](colA, colB);
            break;
        case CollisionEventType::Exit:
            if (onExit_[evt.indexA]) onExit_[evt.indexA](colA, colB);
            break;
        }

        // 1つ目のコールバック内でBが削除された可能性があるため再検証
        if (generations_[evt.indexB] != evt.generationB) continue;
        colB = colliders_[evt.indexB];
        if (!colB) continue;

        // 同様にAも再検証（1つ目のコールバックがAを削除した場合）
        if (generations_[evt.indexA] != evt.generationA) continue;
        colA = colliders_[evt.indexA];
        if (!colA) continue;

        // 2つ目のコールバックを発火
        switch (evt.type) {
        case CollisionEventType::Enter:
            if (onEnter_[evt.indexB]) onEnter_[evt.indexB](colB, colA);
            break;
        case CollisionEventType::Stay:
            if (onCollision_[evt.indexB]) onCollision_[evt.indexB](colB, colA);
            break;
        case CollisionEventType::Exit:
            if (onExit_[evt.indexB]) onExit_[evt.indexB](colB, colA);
            break;
        }
    }

    eventQueue_.clear();
    processingEvents_ = false;
}

//----------------------------------------------------------------------------
// クエリ
//----------------------------------------------------------------------------

void CollisionManager::QueryAABB(const AABB& aabb, std::vector<Collider2D*>& results, uint8_t layerMask)
{
    results.clear();

    Cell c0 = ToCell(aabb.minX, aabb.minY);
    Cell c1 = ToCell(aabb.maxX - 0.001f, aabb.maxY - 0.001f);

    // 重複チェック用バッファ（メンバ変数を再利用でアロケーション削減）
    queryBuffer_.clear();

    for (int cy = c0.y; cy <= c1.y; ++cy) {
        for (int cx = c0.x; cx <= c1.x; ++cx) {
            auto it = grid_.find({cx, cy});
            if (it == grid_.end()) continue;

            for (uint16_t idx : it->second) {
                if ((flags_[idx] & kFlagEnabled) == 0) continue;
                if ((layer_[idx] & layerMask) == 0) continue;

                // 重複チェック（push_back + 後でソート）
                queryBuffer_.push_back(idx);
            }
        }
    }

    // 重複削除
    std::sort(queryBuffer_.begin(), queryBuffer_.end());
    queryBuffer_.erase(std::unique(queryBuffer_.begin(), queryBuffer_.end()), queryBuffer_.end());

    // AABB判定
    for (uint16_t idx : queryBuffer_) {
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

void CollisionManager::QueryLineSegment(const Vector2& start, const Vector2& end,
                                        std::vector<Collider2D*>& results, uint8_t layerMask)
{
    results.clear();

    // 線分のバウンディングボックスを計算
    float minX = (std::min)(start.x, end.x);
    float maxX = (std::max)(start.x, end.x);
    float minY = (std::min)(start.y, end.y);
    float maxY = (std::max)(start.y, end.y);

    Cell c0 = ToCell(minX, minY);
    Cell c1 = ToCell(maxX, maxY);

    // 重複チェック用
    std::vector<uint16_t> checked;

    // 線分が通過する可能性のあるセルを走査
    for (int cy = c0.y; cy <= c1.y; ++cy) {
        for (int cx = c0.x; cx <= c1.x; ++cx) {
            auto it = grid_.find({cx, cy});
            if (it == grid_.end()) continue;

            for (uint16_t idx : it->second) {
                if ((flags_[idx] & kFlagEnabled) == 0) continue;
                if ((layer_[idx] & layerMask) == 0) continue;
                checked.push_back(idx);
            }
        }
    }

    // 重複削除
    std::sort(checked.begin(), checked.end());
    checked.erase(std::unique(checked.begin(), checked.end()), checked.end());

    // 線分とAABBの交差判定（Liang-Barsky アルゴリズム）
    for (uint16_t idx : checked) {
        float boxMinX = posX_[idx] - halfW_[idx];
        float boxMaxX = posX_[idx] + halfW_[idx];
        float boxMinY = posY_[idx] - halfH_[idx];
        float boxMaxY = posY_[idx] + halfH_[idx];

        // 線分のパラメトリック方程式: P(t) = start + t * (end - start), t ∈ [0, 1]
        float dx = end.x - start.x;
        float dy = end.y - start.y;

        float tMin = 0.0f;
        float tMax = 1.0f;

        // X軸方向
        if (std::abs(dx) < 1e-8f) {
            // 線分がX軸に平行
            if (start.x < boxMinX || start.x > boxMaxX) continue;
        } else {
            float t1 = (boxMinX - start.x) / dx;
            float t2 = (boxMaxX - start.x) / dx;
            if (t1 > t2) std::swap(t1, t2);
            tMin = (std::max)(tMin, t1);
            tMax = (std::min)(tMax, t2);
            if (tMin > tMax) continue;
        }

        // Y軸方向
        if (std::abs(dy) < 1e-8f) {
            // 線分がY軸に平行
            if (start.y < boxMinY || start.y > boxMaxY) continue;
        } else {
            float t1 = (boxMinY - start.y) / dy;
            float t2 = (boxMaxY - start.y) / dy;
            if (t1 > t2) std::swap(t1, t2);
            tMin = (std::max)(tMin, t1);
            tMax = (std::min)(tMax, t2);
            if (tMin > tMax) continue;
        }

        // 交差している
        results.push_back(colliders_[idx]);
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
        // ホットデータ(flags_)を先にチェックしてキャッシュ効率向上
        if ((flags_[i] & kFlagEnabled) == 0) continue;
        if (!colliders_[i]) continue;

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

//----------------------------------------------------------------------------
// レイキャスト
//----------------------------------------------------------------------------

std::optional<RaycastHit> CollisionManager::RaycastFirst(
    const Vector2& start, const Vector2& end, uint8_t layerMask)
{
    // 線分のバウンディングボックスを計算
    float minX = (std::min)(start.x, end.x);
    float maxX = (std::max)(start.x, end.x);
    float minY = (std::min)(start.y, end.y);
    float maxY = (std::max)(start.y, end.y);

    Cell c0 = ToCell(minX, minY);
    Cell c1 = ToCell(maxX, maxY);

    // 重複チェック用
    std::vector<uint16_t> checked;

    for (int cy = c0.y; cy <= c1.y; ++cy) {
        for (int cx = c0.x; cx <= c1.x; ++cx) {
            auto it = grid_.find({cx, cy});
            if (it == grid_.end()) continue;

            for (uint16_t idx : it->second) {
                if ((flags_[idx] & kFlagEnabled) == 0) continue;
                if ((layer_[idx] & layerMask) == 0) continue;
                checked.push_back(idx);
            }
        }
    }

    // 重複削除
    std::sort(checked.begin(), checked.end());
    checked.erase(std::unique(checked.begin(), checked.end()), checked.end());

    float dx = end.x - start.x;
    float dy = end.y - start.y;
    float lineLength = std::sqrt(dx * dx + dy * dy);

    std::optional<RaycastHit> closestHit;
    float closestT = 2.0f;  // 1.0より大きい初期値

    for (uint16_t idx : checked) {
        float boxMinX = posX_[idx] - halfW_[idx];
        float boxMaxX = posX_[idx] + halfW_[idx];
        float boxMinY = posY_[idx] - halfH_[idx];
        float boxMaxY = posY_[idx] + halfH_[idx];

        float tMin = 0.0f;
        float tMax = 1.0f;

        // X軸方向
        if (std::abs(dx) < 1e-8f) {
            if (start.x < boxMinX || start.x > boxMaxX) continue;
        } else {
            float t1 = (boxMinX - start.x) / dx;
            float t2 = (boxMaxX - start.x) / dx;
            if (t1 > t2) std::swap(t1, t2);
            tMin = (std::max)(tMin, t1);
            tMax = (std::min)(tMax, t2);
            if (tMin > tMax) continue;
        }

        // Y軸方向
        if (std::abs(dy) < 1e-8f) {
            if (start.y < boxMinY || start.y > boxMaxY) continue;
        } else {
            float t1 = (boxMinY - start.y) / dy;
            float t2 = (boxMaxY - start.y) / dy;
            if (t1 > t2) std::swap(t1, t2);
            tMin = (std::max)(tMin, t1);
            tMax = (std::min)(tMax, t2);
            if (tMin > tMax) continue;
        }

        // 交差している & より近い場合
        if (tMin < closestT) {
            closestT = tMin;
            RaycastHit hit;
            hit.collider = colliders_[idx];
            hit.distance = tMin * lineLength;
            hit.point = Vector2(start.x + dx * tMin, start.y + dy * tMin);
            closestHit = hit;
        }
    }

    return closestHit;
}
