//----------------------------------------------------------------------------
//! @file   material_manager.cpp
//! @brief  マテリアルマネージャー実装
//----------------------------------------------------------------------------
#include "material_manager.h"
#include "common/logging/logging.h"
#include <cassert>

//============================================================================
// シングルトン管理
//============================================================================

MaterialManager& MaterialManager::Get()
{
    assert(instance_ && "MaterialManager::Create() must be called first");
    return *instance_;
}

void MaterialManager::Create()
{
    if (!instance_) {
        instance_ = std::unique_ptr<MaterialManager>(new MaterialManager());
        LOG_INFO("[MaterialManager] Created");
    }
}

void MaterialManager::Destroy()
{
    if (instance_) {
        instance_->Shutdown();
        instance_.reset();
        LOG_INFO("[MaterialManager] Destroyed");
    }
}

MaterialManager::~MaterialManager()
{
    if (initialized_) {
        Shutdown();
    }
}

//============================================================================
// 初期化・終了
//============================================================================

void MaterialManager::Initialize()
{
    if (initialized_) {
        LOG_WARN("[MaterialManager] Already initialized");
        return;
    }

    // スロット配列を初期化
    slots_.reserve(128);

    // グローバルスコープを作成
    scopes_[kGlobalScope] = ScopeData{};

    initialized_ = true;

    // デフォルトマテリアルを作成
    defaultMaterial_ = CreateDefault();

    LOG_INFO("[MaterialManager] Initialized");
}

void MaterialManager::Shutdown()
{
    if (!initialized_) {
        return;
    }

    // 全スコープをクリア
    scopes_.clear();

    // 全スロットをクリア
    slots_.clear();
    while (!freeIndices_.empty()) {
        freeIndices_.pop();
    }

    defaultMaterial_ = MaterialHandle::Invalid();

    initialized_ = false;
    LOG_INFO("[MaterialManager] Shutdown");
}

//============================================================================
// スコープ管理
//============================================================================

MaterialManager::ScopeId MaterialManager::BeginScope()
{
    ScopeId scopeId = nextScopeId_++;
    scopes_[scopeId] = ScopeData{};
    currentScope_ = scopeId;

    LOG_INFO("[MaterialManager] BeginScope: " + std::to_string(scopeId));
    return scopeId;
}

void MaterialManager::EndScope(ScopeId scopeId)
{
    auto it = scopes_.find(scopeId);
    if (it == scopes_.end()) {
        LOG_WARN("[MaterialManager] EndScope: Invalid scope ID " + std::to_string(scopeId));
        return;
    }

    // このスコープの全マテリアルの参照カウントを減少
    for (const auto& handle : it->second.materials) {
        DecrementRefCount(handle);
    }

    scopes_.erase(it);

    // 現在のスコープを更新
    if (currentScope_ == scopeId) {
        currentScope_ = kGlobalScope;
    }

    // GC実行
    GarbageCollect();

    LOG_INFO("[MaterialManager] EndScope: " + std::to_string(scopeId));
}

//============================================================================
// マテリアル作成
//============================================================================

MaterialHandle MaterialManager::Create(const MaterialDesc& desc)
{
    return CreateInScope(desc, currentScope_);
}

MaterialHandle MaterialManager::CreateGlobal(const MaterialDesc& desc)
{
    return CreateInScope(desc, kGlobalScope);
}

MaterialHandle MaterialManager::CreateDefault()
{
    MaterialDesc desc;
    desc.params.albedoColor = Colors::White;
    desc.params.metallic = 0.0f;
    desc.params.roughness = 0.5f;
    desc.params.ao = 1.0f;
    desc.name = "Default";

    return CreateInScope(desc, kGlobalScope);
}

//============================================================================
// マテリアルアクセス
//============================================================================

Material* MaterialManager::Get(MaterialHandle handle) const noexcept
{
    if (!handle.IsValid()) {
        return nullptr;
    }

    uint16_t index = handle.GetIndex();
    if (index >= slots_.size()) {
        return nullptr;
    }

    const auto& slot = slots_[index];
    if (!slot.inUse || slot.generation != handle.GetGeneration()) {
        return nullptr;
    }

    return slot.material.get();
}

bool MaterialManager::IsValid(MaterialHandle handle) const noexcept
{
    return Get(handle) != nullptr;
}

void MaterialManager::GarbageCollect()
{
    size_t freed = 0;

    for (size_t i = 0; i < slots_.size(); ++i) {
        auto& slot = slots_[i];
        if (slot.inUse && slot.refCount == 0) {
            slot.material.reset();
            slot.inUse = false;
            slot.generation++;

            freeIndices_.push(static_cast<uint16_t>(i));
            freed++;
        }
    }

    if (freed > 0) {
        stats_.materialCount -= freed;
        LOG_INFO("[MaterialManager] GC: freed " + std::to_string(freed) + " materials");
    }
}

//============================================================================
// パラメータ変更
//============================================================================

void MaterialManager::SetAlbedoColor(MaterialHandle handle, const Color& color)
{
    if (Material* mat = Get(handle)) {
        mat->SetAlbedoColor(color);
    }
}

void MaterialManager::SetMetallic(MaterialHandle handle, float value)
{
    if (Material* mat = Get(handle)) {
        mat->SetMetallic(value);
    }
}

void MaterialManager::SetRoughness(MaterialHandle handle, float value)
{
    if (Material* mat = Get(handle)) {
        mat->SetRoughness(value);
    }
}

void MaterialManager::SetAO(MaterialHandle handle, float value)
{
    if (Material* mat = Get(handle)) {
        mat->SetAO(value);
    }
}

void MaterialManager::SetEmissive(MaterialHandle handle, const Color& color, float strength)
{
    if (Material* mat = Get(handle)) {
        mat->SetEmissive(color, strength);
    }
}

void MaterialManager::SetTexture(MaterialHandle handle, MaterialTextureSlot slot, TextureHandle texture)
{
    if (Material* mat = Get(handle)) {
        mat->SetTexture(slot, texture);
    }
}

TextureHandle MaterialManager::GetTexture(MaterialHandle handle, MaterialTextureSlot slot) const
{
    if (const Material* mat = Get(handle)) {
        return mat->GetTexture(slot);
    }
    return TextureHandle::Invalid();
}

//============================================================================
// キャッシュ管理
//============================================================================

void MaterialManager::ClearCache()
{
    // マテリアルは名前でキャッシュしていないので、特に何もしない
    LOG_INFO("[MaterialManager] Cache cleared");
}

MaterialCacheStats MaterialManager::GetCacheStats() const
{
    stats_.materialCount = 0;

    for (const auto& slot : slots_) {
        if (slot.inUse && slot.material) {
            stats_.materialCount++;
        }
    }

    return stats_;
}

//============================================================================
// 内部ヘルパー
//============================================================================

MaterialHandle MaterialManager::AllocateSlot(MaterialPtr material)
{
    uint16_t index;

    if (!freeIndices_.empty()) {
        index = freeIndices_.front();
        freeIndices_.pop();
    }
    else {
        if (slots_.size() >= 65535) {
            LOG_ERROR("[MaterialManager] Maximum slot count reached");
            return MaterialHandle::Invalid();
        }
        index = static_cast<uint16_t>(slots_.size());
        slots_.push_back({});
    }

    auto& slot = slots_[index];
    slot.material = std::move(material);
    slot.refCount = 0;
    slot.generation = (slot.generation + 1) & 0x7FFF;
    if (slot.generation == 0) slot.generation = 1;
    slot.inUse = true;

    return MaterialHandle::Create(index, slot.generation);
}

void MaterialManager::AddToScope(MaterialHandle handle, ScopeId scope)
{
    auto it = scopes_.find(scope);
    if (it != scopes_.end()) {
        it->second.materials.push_back(handle);
    }
}

void MaterialManager::IncrementRefCount(MaterialHandle handle)
{
    if (!handle.IsValid()) return;

    uint16_t index = handle.GetIndex();
    if (index < slots_.size() && slots_[index].inUse &&
        slots_[index].generation == handle.GetGeneration()) {
        slots_[index].refCount++;
    }
}

void MaterialManager::DecrementRefCount(MaterialHandle handle)
{
    if (!handle.IsValid()) return;

    uint16_t index = handle.GetIndex();
    if (index < slots_.size() && slots_[index].inUse &&
        slots_[index].generation == handle.GetGeneration()) {
        if (slots_[index].refCount > 0) {
            slots_[index].refCount--;
        }
    }
}

MaterialHandle MaterialManager::CreateInScope(const MaterialDesc& desc, ScopeId scope)
{
    // マテリアルを作成
    auto material = Material::Create(desc);
    if (!material) {
        LOG_ERROR("[MaterialManager] Failed to create material: " + desc.name);
        return MaterialHandle::Invalid();
    }

    // スロットに割り当て
    MaterialHandle handle = AllocateSlot(std::move(material));
    if (!handle.IsValid()) {
        return MaterialHandle::Invalid();
    }

    // スコープに追加
    IncrementRefCount(handle);
    AddToScope(handle, scope);

    stats_.materialCount++;
    LOG_INFO("[MaterialManager] Created material: " + desc.name);

    return handle;
}

