//----------------------------------------------------------------------------
//! @file   material_manager.h
//! @brief  マテリアルマネージャー
//----------------------------------------------------------------------------
#pragma once

#include "material.h"
#include "material_handle.h"
#include "engine/texture/texture_handle.h"
#include "common/utility/non_copyable.h"
#include <memory>
#include <string>
#include <vector>
#include <queue>
#include <unordered_map>

//============================================================================
//! @brief マテリアルキャッシュ統計情報
//============================================================================
struct MaterialCacheStats
{
    size_t materialCount = 0;     //!< マテリアル数
    size_t hitCount = 0;          //!< キャッシュヒット回数
    size_t missCount = 0;         //!< キャッシュミス回数

    [[nodiscard]] double HitRate() const noexcept {
        size_t total = hitCount + missCount;
        return total > 0 ? static_cast<double>(hitCount) / total : 0.0;
    }
};

//============================================================================
//! @brief マテリアルマネージャー（シングルトン）
//!
//! @details マテリアルの作成、キャッシュを一元管理する。
//!          Handle + RefCount + GC方式でマテリアルのライフサイクルを自動管理。
//!          テクスチャはTextureHandle経由で参照（所有しない）
//!
//! @note 使用例:
//! @code
//!   // 初期化
//!   MaterialManager::Get().Initialize();
//!
//!   // シーン開始時にスコープ開始
//!   auto scopeId = MaterialManager::Get().BeginScope();
//!
//!   // マテリアル作成
//!   MaterialDesc desc;
//!   desc.params.albedoColor = Colors::Red;
//!   desc.params.roughness = 0.5f;
//!   desc.textures[0] = TextureManager::Get().Load("assets:/textures/albedo.png");
//!   MaterialHandle mat = MaterialManager::Get().Create(desc);
//!
//!   // 使用時
//!   if (Material* ptr = MaterialManager::Get().Get(mat)) {
//!       renderer->SetMaterial(ptr);
//!   }
//!
//!   // シーン終了時にスコープ終了 → 自動GC
//!   MaterialManager::Get().EndScope(scopeId);
//! @endcode
//============================================================================
class MaterialManager final : private NonCopyableNonMovable
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
    [[nodiscard]] static MaterialManager& Get();

    //! インスタンス生成
    static void Create();

    //! インスタンス破棄
    static void Destroy();

    //! デストラクタ
    ~MaterialManager();

    //----------------------------------------------------------
    //! @name 初期化・終了
    //----------------------------------------------------------
    //!@{

    //! @brief 初期化
    void Initialize();

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
    //! @name マテリアル作成
    //----------------------------------------------------------
    //!@{

    //! @brief マテリアルを作成（現在スコープに紐付け）
    //! @param desc マテリアル記述子
    //! @return マテリアルハンドル（失敗時Invalid）
    [[nodiscard]] MaterialHandle Create(const MaterialDesc& desc);

    //! @brief マテリアルをグローバルスコープで作成（永続）
    //! @param desc マテリアル記述子
    //! @return マテリアルハンドル（失敗時Invalid）
    [[nodiscard]] MaterialHandle CreateGlobal(const MaterialDesc& desc);

    //! @brief デフォルトマテリアルを作成
    //! @return マテリアルハンドル
    [[nodiscard]] MaterialHandle CreateDefault();

    //!@}
    //----------------------------------------------------------
    //! @name マテリアルアクセス
    //----------------------------------------------------------
    //!@{

    //! @brief ハンドルからMaterial*を取得
    //! @param handle マテリアルハンドル
    //! @return Material*（無効なハンドルの場合nullptr）
    [[nodiscard]] Material* Get(MaterialHandle handle) const noexcept;

    //! @brief ハンドルが有効かチェック
    //! @param handle マテリアルハンドル
    //! @return 有効な場合true
    [[nodiscard]] bool IsValid(MaterialHandle handle) const noexcept;

    //! @brief refcount=0のマテリアルを解放
    void GarbageCollect();

    //!@}
    //----------------------------------------------------------
    //! @name パラメータ変更
    //----------------------------------------------------------
    //!@{

    //! @brief アルベドカラーを設定
    void SetAlbedoColor(MaterialHandle handle, const Color& color);

    //! @brief メタリック値を設定
    void SetMetallic(MaterialHandle handle, float value);

    //! @brief ラフネス値を設定
    void SetRoughness(MaterialHandle handle, float value);

    //! @brief AO値を設定
    void SetAO(MaterialHandle handle, float value);

    //! @brief エミッシブを設定
    void SetEmissive(MaterialHandle handle, const Color& color, float strength);

    //! @brief テクスチャを設定
    void SetTexture(MaterialHandle handle, MaterialTextureSlot slot, TextureHandle texture);

    //! @brief テクスチャを取得
    [[nodiscard]] TextureHandle GetTexture(MaterialHandle handle, MaterialTextureSlot slot) const;

    //!@}
    //----------------------------------------------------------
    //! @name キャッシュ管理
    //----------------------------------------------------------
    //!@{

    //! キャッシュをクリア
    void ClearCache();

    //! キャッシュ統計を取得
    [[nodiscard]] MaterialCacheStats GetCacheStats() const;

    //!@}

private:
    MaterialManager() = default;

    static inline std::unique_ptr<MaterialManager> instance_ = nullptr;

    //------------------------------------------------------------------------
    //! @name 内部構造体
    //------------------------------------------------------------------------
    //!@{

    //! @brief マテリアルスロット
    struct MaterialSlot
    {
        MaterialPtr material;       //!< 実際のマテリアル
        uint32_t refCount = 0;      //!< 参照カウント
        uint16_t generation = 0;    //!< 世代番号
        bool inUse = false;         //!< スロット使用中フラグ
    };

    //! @brief スコープデータ
    struct ScopeData
    {
        std::vector<MaterialHandle> materials;  //!< このスコープで使用中のマテリアルハンドル
    };

    //!@}
    //------------------------------------------------------------------------
    //! @name 内部ヘルパー
    //------------------------------------------------------------------------
    //!@{

    //! @brief スロットを割り当て
    [[nodiscard]] MaterialHandle AllocateSlot(MaterialPtr material);

    //! @brief 指定スコープにマテリアルを追加
    void AddToScope(MaterialHandle handle, ScopeId scope);

    //! @brief 参照カウントを増加
    void IncrementRefCount(MaterialHandle handle);

    //! @brief 参照カウントを減少
    void DecrementRefCount(MaterialHandle handle);

    //! @brief 指定スコープでマテリアルを作成
    [[nodiscard]] MaterialHandle CreateInScope(const MaterialDesc& desc, ScopeId scope);

    //!@}
    //------------------------------------------------------------------------
    //! @name メンバ変数
    //------------------------------------------------------------------------

    bool initialized_ = false;

    // 統計情報
    mutable MaterialCacheStats stats_;

    //--- スロットベースストレージ ---
    std::vector<MaterialSlot> slots_;      //!< マテリアルスロット配列
    std::queue<uint16_t> freeIndices_;     //!< 空きスロットインデックス

    //--- スコープ管理 ---
    ScopeId currentScope_ = kGlobalScope;  //!< 現在のスコープ
    ScopeId nextScopeId_ = 1;              //!< 次のスコープID
    std::unordered_map<ScopeId, ScopeData> scopes_;  //!< スコープデータ

    //--- デフォルトマテリアル ---
    MaterialHandle defaultMaterial_;       //!< デフォルトマテリアル
};

//============================================================================
//! @brief マテリアルスコープガード（RAII）
//============================================================================
class MaterialScopeGuard final
{
public:
    MaterialScopeGuard() : scope_(MaterialManager::Get().BeginScope()) {}

    ~MaterialScopeGuard() {
        if (scope_ != MaterialManager::kGlobalScope) {
            MaterialManager::Get().EndScope(scope_);
        }
    }

    MaterialScopeGuard(const MaterialScopeGuard&) = delete;
    MaterialScopeGuard& operator=(const MaterialScopeGuard&) = delete;

    MaterialScopeGuard(MaterialScopeGuard&& other) noexcept : scope_(other.scope_) {
        other.scope_ = MaterialManager::kGlobalScope;
    }

    MaterialScopeGuard& operator=(MaterialScopeGuard&& other) noexcept {
        if (this != &other) {
            if (scope_ != MaterialManager::kGlobalScope) {
                MaterialManager::Get().EndScope(scope_);
            }
            scope_ = other.scope_;
            other.scope_ = MaterialManager::kGlobalScope;
        }
        return *this;
    }

    [[nodiscard]] MaterialManager::ScopeId GetId() const noexcept { return scope_; }

private:
    MaterialManager::ScopeId scope_;
};

