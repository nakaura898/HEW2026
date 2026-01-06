//----------------------------------------------------------------------------
//! @file   collision_manager.h
//! @brief  衝突判定マネージャー（DOD設計）
//!
//! @note スレッドセーフ性:
//!       【警告】このクラスはスレッドセーフではありません。
//!       - 全メソッド: メインスレッドからのみ呼び出し可能
//!       - ワーカースレッドからの呼び出しは未定義動作
//!
//! @note コールバック実行タイミング:
//!       衝突コールバック（onEnter_, onCollision_, onExit_）は、
//!       FixedUpdate()の衝突検出完了後に遅延実行されます。
//!       これにより、コールバック内でのコライダー削除が安全に行えます。
//!       削除されたコライダーは世代チェックによりスキップされます。
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

class Collider2D;
class GameObject;

//============================================================================
// 定数定義
//============================================================================
namespace CollisionConstants {
    static constexpr uint16_t kInvalidIndex = UINT16_MAX;   //!< 無効なインデックス
    static constexpr uint8_t kDefaultLayer = 0x01;          //!< デフォルトレイヤー
    static constexpr uint8_t kDefaultMask = 0xFF;           //!< デフォルトマスク（全レイヤーと衝突）
    static constexpr int kDefaultCellSize = 256;            //!< デフォルトセルサイズ
}

//============================================================================
//! @brief コライダーハンドル
//!
//! Collider2Dが保持する軽量な識別子。
//! 実データはCollisionManagerが所有する。
//============================================================================
struct ColliderHandle {
    uint16_t index = CollisionConstants::kInvalidIndex;  //!< データ配列へのインデックス
    uint16_t generation = 0;                              //!< 世代（再利用検出用）

    [[nodiscard]] bool IsValid() const noexcept {
        return index != CollisionConstants::kInvalidIndex;
    }
    bool operator==(const ColliderHandle& other) const noexcept {
        return index == other.index && generation == other.generation;
    }
};

//============================================================================
//! @brief AABB（軸平行境界ボックス）
//============================================================================
struct AABB {
    float minX = 0.0f;
    float minY = 0.0f;
    float maxX = 0.0f;
    float maxY = 0.0f;

    AABB() = default;
    AABB(float x, float y, float w, float h)
        : minX(x), minY(y), maxX(x + w), maxY(y + h) {}

    //! @brief 他のAABBと交差しているか判定
    [[nodiscard]] bool Intersects(const AABB& other) const noexcept {
        return minX < other.maxX && maxX > other.minX &&
               minY < other.maxY && maxY > other.minY;
    }

    //! @brief 点を含んでいるか判定
    [[nodiscard]] bool Contains(float px, float py) const noexcept {
        return px >= minX && px < maxX && py >= minY && py < maxY;
    }

    //! @brief 中心座標を取得
    [[nodiscard]] Vector2 GetCenter() const noexcept {
        return Vector2((minX + maxX) * 0.5f, (minY + maxY) * 0.5f);
    }

    //! @brief サイズを取得
    [[nodiscard]] Vector2 GetSize() const noexcept {
        return Vector2(maxX - minX, maxY - minY);
    }
};

//============================================================================
//! @brief 衝突コールバック型
//============================================================================
using CollisionCallback = std::function<void(Collider2D*, Collider2D*)>;

//============================================================================
//! @brief 衝突イベント種別
//============================================================================
enum class CollisionEventType : uint8_t {
    Enter,  //!< 衝突開始
    Stay,   //!< 衝突継続
    Exit    //!< 衝突終了
};

//============================================================================
//! @brief キューイングされた衝突イベント
//!
//! コールバック発火を遅延させるためのイベント情報。
//! 世代情報を保持し、イベント発生後に削除されたコライダーをスキップ可能。
//============================================================================
struct CollisionEvent {
    CollisionEventType type;        //!< イベント種別
    uint16_t indexA;                //!< コライダーAのインデックス
    uint16_t indexB;                //!< コライダーBのインデックス
    uint16_t generationA;           //!< イベント発生時のA世代
    uint16_t generationB;           //!< イベント発生時のB世代
};

//============================================================================
//! @brief レイキャストヒット情報
//============================================================================
struct RaycastHit {
    Collider2D* collider = nullptr;  //!< ヒットしたコライダー
    float distance = 0.0f;           //!< 始点からの距離
    Vector2 point;                   //!< ヒット座標
};

//============================================================================
//! @brief 衝突判定マネージャー（DOD設計）
//!
//! Structure of Arrays（SoA）でデータを保持し、
//! キャッシュ効率の良い衝突判定を行う。
//============================================================================
class CollisionManager final : private NonCopyableNonMovable {
public:
    //! @brief シングルトンインスタンス取得
    static CollisionManager& Get()
    {
        assert(instance_ && "CollisionManager::Create() must be called first");
        return *instance_;
    }

    //! @brief インスタンス生成
    static void Create()
    {
        if (!instance_) {
            instance_ = std::unique_ptr<CollisionManager>(new CollisionManager());
        }
    }

    //! @brief インスタンス破棄
    static void Destroy()
    {
        instance_.reset();
    }

    //! @brief デストラクタ
    ~CollisionManager() = default;

    //------------------------------------------------------------------------
    // 初期化・終了
    //------------------------------------------------------------------------

    void Initialize(int cellSize = CollisionConstants::kDefaultCellSize);
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
    [[nodiscard]] Vector2 GetSize(ColliderHandle handle) const;
    [[nodiscard]] Vector2 GetOffset(ColliderHandle handle) const;
    [[nodiscard]] uint8_t GetLayer(ColliderHandle handle) const;
    [[nodiscard]] uint8_t GetMask(ColliderHandle handle) const;
    [[nodiscard]] bool IsEnabled(ColliderHandle handle) const;
    [[nodiscard]] bool IsTrigger(ColliderHandle handle) const;
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

    void SetCellSize(int size) noexcept {
        cellSize_ = size > 0 ? size : CollisionConstants::kDefaultCellSize;
    }
    [[nodiscard]] int GetCellSize() const noexcept { return cellSize_; }
    [[nodiscard]] size_t GetColliderCount() const noexcept { return activeCount_; }

    //------------------------------------------------------------------------
    // クエリ
    //------------------------------------------------------------------------

    //! @brief AABB範囲内のコライダーを検索
    void QueryAABB(const AABB& aabb, std::vector<Collider2D*>& results,
                   uint8_t layerMask = CollisionConstants::kDefaultMask);

    //! @brief 点と交差するコライダーを検索
    void QueryPoint(const Vector2& point, std::vector<Collider2D*>& results,
                    uint8_t layerMask = CollisionConstants::kDefaultMask);

    //! @brief 線分と交差するコライダーを検索
    //! @param start 線分の始点
    //! @param end 線分の終点
    //! @param results 結果を格納するベクター
    //! @param layerMask 検索対象のレイヤーマスク
    void QueryLineSegment(const Vector2& start, const Vector2& end,
                          std::vector<Collider2D*>& results,
                          uint8_t layerMask = CollisionConstants::kDefaultMask);

    //! @brief レイキャストで最初にヒットしたコライダーを取得
    //! @param start 線分の始点
    //! @param end 線分の終点
    //! @param layerMask 検索対象のレイヤーマスク
    //! @return ヒット情報（ヒットしなかった場合はnullopt）
    [[nodiscard]] std::optional<RaycastHit> RaycastFirst(
        const Vector2& start, const Vector2& end,
        uint8_t layerMask = CollisionConstants::kDefaultMask);

private:
    CollisionManager() = default;
    CollisionManager(const CollisionManager&) = delete;
    CollisionManager& operator=(const CollisionManager&) = delete;

    static inline std::unique_ptr<CollisionManager> instance_ = nullptr;

    //! @brief 固定タイムステップの衝突判定（内部用）
    void FixedUpdate();

    //! @brief キューイングされたイベントを処理
    //! @note FixedUpdate()終了後に呼び出される。世代チェックにより
    //!       コールバック中に削除されたコライダーは安全にスキップされる。
    void ProcessEventQueue();

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
            // 負の座標を正しく処理するハッシュ関数
            auto h1 = std::hash<int>{}(c.x);
            auto h2 = std::hash<int>{}(c.y);
            return h1 ^ (h2 * 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
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
    int cellSize_ = CollisionConstants::kDefaultCellSize;
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

    // クエリ用バッファ（再利用でアロケーション削減）
    mutable std::vector<uint16_t> queryBuffer_;

    // 遅延イベントキュー（コールバックを衝突検出完了後に発火）
    std::vector<CollisionEvent> eventQueue_;
    bool processingEvents_ = false;  //!< 再入防止フラグ
};
