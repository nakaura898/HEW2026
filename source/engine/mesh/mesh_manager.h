//----------------------------------------------------------------------------
//! @file   mesh_manager.h
//! @brief  メッシュマネージャー
//----------------------------------------------------------------------------
#pragma once

#include "mesh.h"
#include "mesh_handle.h"
#include "mesh_loader.h"
#include "common/utility/non_copyable.h"
#include <memory>
#include <string>
#include <vector>
#include <queue>
#include <unordered_map>

class IReadableFileSystem;

//============================================================================
//! @brief メッシュキャッシュ統計情報
//============================================================================
struct MeshCacheStats
{
    size_t meshCount = 0;             //!< メッシュキャッシュ数
    size_t hitCount = 0;              //!< キャッシュヒット回数
    size_t missCount = 0;             //!< キャッシュミス回数
    size_t totalVertices = 0;         //!< 総頂点数
    size_t totalIndices = 0;          //!< 総インデックス数
    size_t totalMemoryBytes = 0;      //!< 総GPUメモリ使用量

    [[nodiscard]] double HitRate() const noexcept {
        size_t total = hitCount + missCount;
        return total > 0 ? static_cast<double>(hitCount) / total : 0.0;
    }
};

//============================================================================
//! @brief メッシュマネージャー（シングルトン）
//!
//! @details メッシュのロード、キャッシュを一元管理する。
//!          Handle + RefCount + GC方式でメッシュのライフサイクルを自動管理。
//!
//! @note 使用例:
//! @code
//!   // 初期化
//!   MeshManager::Get().Initialize(fileSystem);
//!
//!   // シーン開始時にスコープ開始
//!   auto scopeId = MeshManager::Get().BeginScope();
//!
//!   // メッシュ読み込み
//!   MeshHandle mesh = MeshManager::Get().Load("assets:/models/player.gltf");
//!
//!   // 使用時
//!   if (Mesh* ptr = MeshManager::Get().Get(mesh)) {
//!       renderer->SetMesh(ptr);
//!   }
//!
//!   // シーン終了時にスコープ終了 → 自動GC
//!   MeshManager::Get().EndScope(scopeId);
//!
//!   // 終了
//!   MeshManager::Get().Shutdown();
//! @endcode
//============================================================================
class MeshManager final : private NonCopyableNonMovable
{
public:
    //------------------------------------------------------------------------
    //! @name スコープ管理型
    //------------------------------------------------------------------------
    //!@{
    using ScopeId = uint32_t;
    static constexpr ScopeId kGlobalScope = 0;  //!< 永続スコープ
    //!@}

    //! シングルトンインスタンス取得
    [[nodiscard]] static MeshManager& Get();

    //! インスタンス生成
    static void Create();

    //! インスタンス破棄
    static void Destroy();

    //! デストラクタ
    ~MeshManager();

    //----------------------------------------------------------
    //! @name 初期化・終了
    //----------------------------------------------------------
    //!@{

    //! @brief 初期化
    //! @param fileSystem ファイルシステム（必須）
    void Initialize(IReadableFileSystem* fileSystem);

    //! 終了
    void Shutdown();

    //! 初期化済みかどうか
    [[nodiscard]] bool IsInitialized() const noexcept { return initialized_; }

    //!@}
    //----------------------------------------------------------
    //! @name スコープ管理
    //----------------------------------------------------------
    //!@{

    //! @brief スコープ開始
    //! @return 新しいスコープID
    [[nodiscard]] ScopeId BeginScope();

    //! @brief スコープ終了
    //! @param scopeId BeginScope()で取得したスコープID
    void EndScope(ScopeId scopeId);

    //! @brief 現在のスコープIDを取得
    [[nodiscard]] ScopeId GetCurrentScope() const noexcept { return currentScope_; }

    //!@}
    //----------------------------------------------------------
    //! @name ハンドルベースAPI
    //----------------------------------------------------------
    //!@{

    //! @brief メッシュをロード（現在スコープに紐付け）
    //! @param path ファイルパス（assets:/形式またはフルパス）
    //! @param options ロードオプション
    //! @return メッシュハンドル（失敗時Invalid）
    [[nodiscard]] MeshHandle Load(
        const std::string& path,
        const MeshLoadOptions& options = {});

    //! @brief メッシュをグローバルスコープでロード（永続）
    //! @param path ファイルパス
    //! @param options ロードオプション
    //! @return メッシュハンドル（失敗時Invalid）
    [[nodiscard]] MeshHandle LoadGlobal(
        const std::string& path,
        const MeshLoadOptions& options = {});

    //! @brief ハンドルからMesh*を取得
    //! @param handle メッシュハンドル
    //! @return Mesh*（無効なハンドルの場合nullptr）
    [[nodiscard]] Mesh* Get(MeshHandle handle) const noexcept;

    //! @brief ハンドルが有効かチェック
    //! @param handle メッシュハンドル
    //! @return 有効な場合true
    [[nodiscard]] bool IsValid(MeshHandle handle) const noexcept;

    //! @brief refcount=0のメッシュを解放
    void GarbageCollect();

    //!@}
    //----------------------------------------------------------
    //! @name プリミティブメッシュ生成
    //----------------------------------------------------------
    //!@{

    //! @brief ボックスメッシュを生成
    //! @param size サイズ（幅、高さ、奥行き）
    //! @return メッシュハンドル
    [[nodiscard]] MeshHandle CreateBox(const Vector3& size);

    //! @brief 球メッシュを生成
    //! @param radius 半径
    //! @param segments 分割数（経度・緯度）
    //! @return メッシュハンドル
    [[nodiscard]] MeshHandle CreateSphere(float radius, uint32_t segments = 16);

    //! @brief 平面メッシュを生成
    //! @param width 幅
    //! @param depth 奥行き
    //! @param subdivisionsX X方向分割数
    //! @param subdivisionsZ Z方向分割数
    //! @return メッシュハンドル
    [[nodiscard]] MeshHandle CreatePlane(
        float width,
        float depth,
        uint32_t subdivisionsX = 1,
        uint32_t subdivisionsZ = 1);

    //! @brief シリンダーメッシュを生成
    //! @param radius 半径
    //! @param height 高さ
    //! @param segments 円周分割数
    //! @return メッシュハンドル
    [[nodiscard]] MeshHandle CreateCylinder(
        float radius,
        float height,
        uint32_t segments = 16);

    //!@}
    //----------------------------------------------------------
    //! @name キャッシュ管理
    //----------------------------------------------------------
    //!@{

    //! キャッシュをクリア
    void ClearCache();

    //! キャッシュ統計を取得
    [[nodiscard]] MeshCacheStats GetCacheStats() const;

    //!@}

private:
    MeshManager() = default;

    static inline std::unique_ptr<MeshManager> instance_ = nullptr;

    //------------------------------------------------------------------------
    //! @name 内部構造体
    //------------------------------------------------------------------------
    //!@{

    //! @brief メッシュスロット
    struct MeshSlot
    {
        MeshPtr mesh;               //!< 実際のメッシュ
        uint32_t refCount = 0;      //!< 参照カウント
        uint16_t generation = 0;    //!< 世代番号
        bool inUse = false;         //!< スロット使用中フラグ
    };

    //! @brief スコープデータ
    struct ScopeData
    {
        std::vector<MeshHandle> meshes;  //!< このスコープで使用中のメッシュハンドル
    };

    //!@}
    //------------------------------------------------------------------------
    //! @name 内部ヘルパー
    //------------------------------------------------------------------------
    //!@{

    //! @brief キャッシュキーを計算
    [[nodiscard]] uint64_t ComputeCacheKey(const std::string& path) const;

    //! @brief スロットを割り当て
    [[nodiscard]] MeshHandle AllocateSlot(MeshPtr mesh);

    //! @brief 指定スコープにメッシュを追加
    void AddToScope(MeshHandle handle, ScopeId scope);

    //! @brief 参照カウントを増加
    void IncrementRefCount(MeshHandle handle);

    //! @brief 参照カウントを減少
    void DecrementRefCount(MeshHandle handle);

    //! @brief 指定スコープでメッシュをロード
    [[nodiscard]] MeshHandle LoadInScope(
        const std::string& path,
        const MeshLoadOptions& options,
        ScopeId scope);

    //! @brief プリミティブメッシュを登録
    [[nodiscard]] MeshHandle RegisterPrimitive(MeshPtr mesh, const std::string& name);

    //!@}
    //------------------------------------------------------------------------
    //! @name メンバ変数
    //------------------------------------------------------------------------

    bool initialized_ = false;
    IReadableFileSystem* fileSystem_ = nullptr;

    // 統計情報
    mutable MeshCacheStats stats_;

    //--- スロットベースストレージ ---
    std::vector<MeshSlot> slots_;                          //!< メッシュスロット配列
    std::queue<uint16_t> freeIndices_;                     //!< 空きスロットインデックス
    std::unordered_map<uint64_t, MeshHandle> handleCache_; //!< パス→ハンドルキャッシュ

    //--- スコープ管理 ---
    ScopeId currentScope_ = kGlobalScope;                  //!< 現在のスコープ
    ScopeId nextScopeId_ = 1;                              //!< 次のスコープID
    std::unordered_map<ScopeId, ScopeData> scopes_;        //!< スコープデータ
};

//============================================================================
//! @brief メッシュスコープガード（RAII）
//============================================================================
class MeshScopeGuard final
{
public:
    MeshScopeGuard() : scope_(MeshManager::Get().BeginScope()) {}

    ~MeshScopeGuard() {
        if (scope_ != MeshManager::kGlobalScope) {
            MeshManager::Get().EndScope(scope_);
        }
    }

    MeshScopeGuard(const MeshScopeGuard&) = delete;
    MeshScopeGuard& operator=(const MeshScopeGuard&) = delete;

    MeshScopeGuard(MeshScopeGuard&& other) noexcept : scope_(other.scope_) {
        other.scope_ = MeshManager::kGlobalScope;
    }

    MeshScopeGuard& operator=(MeshScopeGuard&& other) noexcept {
        if (this != &other) {
            if (scope_ != MeshManager::kGlobalScope) {
                MeshManager::Get().EndScope(scope_);
            }
            scope_ = other.scope_;
            other.scope_ = MeshManager::kGlobalScope;
        }
        return *this;
    }

    [[nodiscard]] MeshManager::ScopeId GetId() const noexcept { return scope_; }

private:
    MeshManager::ScopeId scope_;
};

