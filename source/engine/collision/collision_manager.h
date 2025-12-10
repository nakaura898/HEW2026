//----------------------------------------------------------------------------
//! @file   collision_manager.h
//! @brief  衝突判定マネージャー（DOD設計）
//----------------------------------------------------------------------------
#pragma once

#include "common/utility/non_copyable.h"
#include "engine/scene/math_types.h"
#include <vector>
#include <unordered_map>
#include <functional>
#include <cstdint>

class Collider2D;
class GameObject;

//============================================================================
//! @brief コライダーハンドル
//!
//! Collider2Dが保持する軽量な識別子。
//! 実データはCollisionManagerが所有する。
//============================================================================
struct ColliderHandle {
    uint16_t index = UINT16_MAX;      //!< データ配列へのインデックス
    uint16_t generation = 0;          //!< 世代（再利用検出用）

    [[nodiscard]] bool IsValid() const noexcept { return index != UINT16_MAX; }
    bool operator==(const ColliderHandle& other) const noexcept {
        return index == other.index && generation == other.generation;
    }
};

//============================================================================
//! @brief AABB（軸平行境界ボックス）
//============================================================================
struct AABB {
    float minX, minY;
    float maxX, maxY;

    AABB() : minX(0), minY(0), maxX(0), maxY(0) {}
    AABB(float x, float y, float w, float h)
        : minX(x), minY(y), maxX(x + w), maxY(y + h) {}

    [[nodiscard]] bool Intersects(const AABB& other) const noexcept {
        return minX < other.maxX && maxX > other.minX &&
               minY < other.maxY && maxY > other.minY;
    }

    [[nodiscard]] bool Contains(float px, float py) const noexcept {
        return px >= minX && px < maxX && py >= minY && py < maxY;
    }

    [[nodiscard]] Vector2 GetCenter() const noexcept {
        return Vector2((minX + maxX) * 0.5f, (minY + maxY) * 0.5f);
    }

    // 旧APIとの互換性
    Vector2 position;  // 使用非推奨
    Vector2 size;      // 使用非推奨
};

//============================================================================
//! @brief 衝突コールバック型
//============================================================================
using CollisionCallback = std::function<void(Collider2D*, Collider2D*)>;

//============================================================================
//! @brief 衝突判定マネージャー（DOD設計）
//!
//! Structure of Arrays（SoA）でデータを保持し、
//! キャッシュ効率の良い衝突判定を行う。
//============================================================================
class CollisionManager final : private NonCopyableNonMovable {
public:
    static CollisionManager& Get() noexcept {
        static CollisionManager instance;
        return instance;
    }

    //------------------------------------------------------------------------
    // 初期化・終了
    //------------------------------------------------------------------------

    void Initialize(int cellSize = 256);
    void Shutdown();

    //------------------------------------------------------------------------
    // コライダー登録（Collider2Dから呼ばれる）
    //------------------------------------------------------------------------

    //! @brief コライダーを登録し、ハンドルを返す
    [[nodiscard]] ColliderHandle Register(Collider2D* collider);

    //! @brief コライダーを解除
    void Unregister(ColliderHandle handle);

    //! @brief ハンドルが有効か確認
    [[nodiscard]] bool IsValid(ColliderHandle handle) const noexcept;

    //! @brief 全コライダーをクリア
    void Clear();

    //------------------------------------------------------------------------
    // データ設定（Collider2Dのセッターから呼ばれる）
    //------------------------------------------------------------------------

    void SetPosition(ColliderHandle handle, float x, float y);
    void SetSize(ColliderHandle handle, float w, float h);
    void SetOffset(ColliderHandle handle, float x, float y);
    void SetLayer(ColliderHandle handle, uint8_t layer);
    void SetMask(ColliderHandle handle, uint8_t mask);
    void SetEnabled(ColliderHandle handle, bool enabled);
    void SetTrigger(ColliderHandle handle, bool trigger);

    void SetOnCollision(ColliderHandle handle, CollisionCallback cb);
    void SetOnCollisionEnter(ColliderHandle handle, CollisionCallback cb);
    void SetOnCollisionExit(ColliderHandle handle, CollisionCallback cb);

    //------------------------------------------------------------------------
    // データ取得
    //------------------------------------------------------------------------

    [[nodiscard]] AABB GetAABB(ColliderHandle handle) const;
    [[nodiscard]] uint8_t GetLayer(ColliderHandle handle) const;
    [[nodiscard]] uint8_t GetMask(ColliderHandle handle) const;
    [[nodiscard]] bool IsEnabled(ColliderHandle handle) const;
    [[nodiscard]] Collider2D* GetCollider(ColliderHandle handle) const;

    //------------------------------------------------------------------------
    // 更新
    //------------------------------------------------------------------------

    //! @brief 衝突判定を実行（固定タイムステップ）
    //! @param deltaTime フレームの経過時間
    void Update(float deltaTime);

    //! @brief 固定タイムステップの間隔を取得
    [[nodiscard]] static constexpr float GetFixedDeltaTime() noexcept { return kFixedDeltaTime; }

    //------------------------------------------------------------------------
    // 設定・統計
    //------------------------------------------------------------------------

    void SetCellSize(int size) noexcept { cellSize_ = size > 0 ? size : 256; }
    [[nodiscard]] int GetCellSize() const noexcept { return cellSize_; }
    [[nodiscard]] size_t GetColliderCount() const noexcept { return activeCount_; }

    //------------------------------------------------------------------------
    // クエリ
    //------------------------------------------------------------------------

    void QueryAABB(const AABB& aabb, std::vector<Collider2D*>& results, uint8_t layerMask = 0xFF);
    void QueryPoint(const Vector2& point, std::vector<Collider2D*>& results, uint8_t layerMask = 0xFF);

private:
    CollisionManager() = default;
    ~CollisionManager() = default;

    //! @brief 固定タイムステップの衝突判定（内部用）
    void FixedUpdate();

    //------------------------------------------------------------------------
    // インデックス管理
    //------------------------------------------------------------------------

    [[nodiscard]] uint16_t AllocateIndex();
    void FreeIndex(uint16_t index);

    [[nodiscard]] static uint32_t MakePairKey(uint16_t a, uint16_t b) noexcept {
        if (a > b) { uint16_t t = a; a = b; b = t; }
        return (static_cast<uint32_t>(a) << 16) | b;
    }
    [[nodiscard]] static uint16_t GetFirstIndex(uint32_t key) noexcept {
        return static_cast<uint16_t>(key >> 16);
    }
    [[nodiscard]] static uint16_t GetSecondIndex(uint32_t key) noexcept {
        return static_cast<uint16_t>(key & 0xFFFF);
    }

    //------------------------------------------------------------------------
    // グリッド
    //------------------------------------------------------------------------

    struct Cell {
        int x, y;
        bool operator==(const Cell& other) const noexcept {
            return x == other.x && y == other.y;
        }
    };

    struct CellHash {
        size_t operator()(const Cell& c) const noexcept {
            return static_cast<size_t>(c.x) * 73856093u ^
                   static_cast<size_t>(c.y) * 19349663u;
        }
    };

    [[nodiscard]] Cell ToCell(float x, float y) const noexcept;
    void RebuildGrid();

    //------------------------------------------------------------------------
    // Structure of Arrays（SoA）- コライダーデータ
    //------------------------------------------------------------------------

    // ホットデータ（毎フレームアクセス）- 連続配置でキャッシュ効率化
    std::vector<float> posX_;           //!< ワールド位置X
    std::vector<float> posY_;           //!< ワールド位置Y
    std::vector<float> halfW_;          //!< 半幅
    std::vector<float> halfH_;          //!< 半高さ
    std::vector<uint8_t> layer_;        //!< レイヤー
    std::vector<uint8_t> mask_;         //!< マスク
    std::vector<uint8_t> flags_;        //!< enabled(bit0), trigger(bit1)

    // ウォームデータ（登録時・イベント時）
    std::vector<float> offsetX_;        //!< オフセットX
    std::vector<float> offsetY_;        //!< オフセットY
    std::vector<float> sizeW_;          //!< 元サイズ幅
    std::vector<float> sizeH_;          //!< 元サイズ高さ

    // コールドデータ（イベント発火時のみ）
    std::vector<Collider2D*> colliders_;           //!< Collider2Dへの参照
    std::vector<CollisionCallback> onCollision_;
    std::vector<CollisionCallback> onEnter_;
    std::vector<CollisionCallback> onExit_;

    // 世代管理（ハンドル有効性チェック用）
    std::vector<uint16_t> generations_;

    // フリーリスト
    std::vector<uint16_t> freeIndices_;
    size_t activeCount_ = 0;

    // 空間ハッシュグリッド
    int cellSize_ = 256;
    std::unordered_map<Cell, std::vector<uint16_t>, CellHash> grid_;

    // 衝突ペア（ソート済み）
    std::vector<uint32_t> previousPairs_;
    std::vector<uint32_t> currentPairs_;
    std::vector<uint32_t> testedPairs_;

    // フラグビット定義
    static constexpr uint8_t kFlagEnabled = 0x01;
    static constexpr uint8_t kFlagTrigger = 0x02;

    // 固定タイムステップ
    static constexpr float kFixedDeltaTime = 1.0f / 60.0f;  //!< 60Hz
    float accumulator_ = 0.0f;
};
