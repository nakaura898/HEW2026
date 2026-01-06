//----------------------------------------------------------------------------
//! @file   mesh_manager.cpp
//! @brief  メッシュマネージャー実装
//----------------------------------------------------------------------------
#include "mesh_manager.h"
#include "mesh_loader.h"
#include "common/logging/logging.h"
#include "common/utility/hash.h"
#include <cassert>
#include <cmath>
#include <cfloat>

//============================================================================
// シングルトン管理
//============================================================================

MeshManager& MeshManager::Get()
{
    assert(instance_ && "MeshManager::Create() must be called first");
    return *instance_;
}

void MeshManager::Create()
{
    if (!instance_) {
        instance_ = std::unique_ptr<MeshManager>(new MeshManager());
        LOG_INFO("[MeshManager] Created");
    }
}

void MeshManager::Destroy()
{
    if (instance_) {
        instance_->Shutdown();
        instance_.reset();
        LOG_INFO("[MeshManager] Destroyed");
    }
}

MeshManager::~MeshManager()
{
    if (initialized_) {
        Shutdown();
    }
}

//============================================================================
// 初期化・終了
//============================================================================

void MeshManager::Initialize(IReadableFileSystem* fileSystem)
{
    if (initialized_) {
        LOG_WARN("[MeshManager] Already initialized");
        return;
    }

    fileSystem_ = fileSystem;

    // スロット配列を初期化
    slots_.reserve(256);

    // グローバルスコープを作成
    scopes_[kGlobalScope] = ScopeData{};

    initialized_ = true;
    LOG_INFO("[MeshManager] Initialized");
}

void MeshManager::Shutdown()
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

    // キャッシュをクリア
    handleCache_.clear();

    // ローダーレジストリをクリア
    MeshLoaderRegistry::Get().Clear();

    initialized_ = false;
    LOG_INFO("[MeshManager] Shutdown");
}

//============================================================================
// スコープ管理
//============================================================================

MeshManager::ScopeId MeshManager::BeginScope()
{
    ScopeId scopeId = nextScopeId_++;
    scopes_[scopeId] = ScopeData{};
    currentScope_ = scopeId;

    LOG_INFO("[MeshManager] BeginScope: " + std::to_string(scopeId));
    return scopeId;
}

void MeshManager::EndScope(ScopeId scopeId)
{
    auto it = scopes_.find(scopeId);
    if (it == scopes_.end()) {
        LOG_WARN("[MeshManager] EndScope: Invalid scope ID " + std::to_string(scopeId));
        return;
    }

    // このスコープの全メッシュの参照カウントを減少
    for (const auto& handle : it->second.meshes) {
        DecrementRefCount(handle);
    }

    scopes_.erase(it);

    // 現在のスコープを更新
    if (currentScope_ == scopeId) {
        currentScope_ = kGlobalScope;
    }

    // GC実行
    GarbageCollect();

    LOG_INFO("[MeshManager] EndScope: " + std::to_string(scopeId));
}

//============================================================================
// ハンドルベースAPI
//============================================================================

MeshHandle MeshManager::Load(const std::string& path, const MeshLoadOptions& options)
{
    return LoadInScope(path, options, currentScope_);
}

MeshHandle MeshManager::LoadGlobal(const std::string& path, const MeshLoadOptions& options)
{
    return LoadInScope(path, options, kGlobalScope);
}

Mesh* MeshManager::Get(MeshHandle handle) const noexcept
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

    return slot.mesh.get();
}

bool MeshManager::IsValid(MeshHandle handle) const noexcept
{
    return Get(handle) != nullptr;
}

void MeshManager::GarbageCollect()
{
    size_t freed = 0;

    for (size_t i = 0; i < slots_.size(); ++i) {
        auto& slot = slots_[i];
        if (slot.inUse && slot.refCount == 0) {
            // 統計更新
            if (slot.mesh) {
                stats_.totalVertices -= slot.mesh->GetVertexCount();
                stats_.totalIndices -= slot.mesh->GetIndexCount();
                stats_.totalMemoryBytes -= slot.mesh->GpuSize();
            }

            slot.mesh.reset();
            slot.inUse = false;
            slot.generation++;  // 世代を進める

            freeIndices_.push(static_cast<uint16_t>(i));
            freed++;
        }
    }

    if (freed > 0) {
        stats_.meshCount -= freed;
        LOG_INFO("[MeshManager] GC: freed " + std::to_string(freed) + " meshes");
    }
}

//============================================================================
// プリミティブメッシュ生成
//============================================================================

MeshHandle MeshManager::CreateBox(const Vector3& size)
{
    std::vector<MeshVertex> vertices;
    std::vector<uint32_t> indices;

    float hx = size.x * 0.5f;
    float hy = size.y * 0.5f;
    float hz = size.z * 0.5f;

    // 6面 x 4頂点 = 24頂点
    // 各面に法線とUVを設定

    // Front face (Z+)
    vertices.push_back({Vector3(-hx, -hy, hz), Vector3(0, 0, 1), Vector4(1, 0, 0, 1), Vector2(0, 1), Colors::White});
    vertices.push_back({Vector3( hx, -hy, hz), Vector3(0, 0, 1), Vector4(1, 0, 0, 1), Vector2(1, 1), Colors::White});
    vertices.push_back({Vector3( hx,  hy, hz), Vector3(0, 0, 1), Vector4(1, 0, 0, 1), Vector2(1, 0), Colors::White});
    vertices.push_back({Vector3(-hx,  hy, hz), Vector3(0, 0, 1), Vector4(1, 0, 0, 1), Vector2(0, 0), Colors::White});

    // Back face (Z-)
    vertices.push_back({Vector3( hx, -hy, -hz), Vector3(0, 0, -1), Vector4(-1, 0, 0, 1), Vector2(0, 1), Colors::White});
    vertices.push_back({Vector3(-hx, -hy, -hz), Vector3(0, 0, -1), Vector4(-1, 0, 0, 1), Vector2(1, 1), Colors::White});
    vertices.push_back({Vector3(-hx,  hy, -hz), Vector3(0, 0, -1), Vector4(-1, 0, 0, 1), Vector2(1, 0), Colors::White});
    vertices.push_back({Vector3( hx,  hy, -hz), Vector3(0, 0, -1), Vector4(-1, 0, 0, 1), Vector2(0, 0), Colors::White});

    // Top face (Y+)
    vertices.push_back({Vector3(-hx, hy,  hz), Vector3(0, 1, 0), Vector4(1, 0, 0, 1), Vector2(0, 1), Colors::White});
    vertices.push_back({Vector3( hx, hy,  hz), Vector3(0, 1, 0), Vector4(1, 0, 0, 1), Vector2(1, 1), Colors::White});
    vertices.push_back({Vector3( hx, hy, -hz), Vector3(0, 1, 0), Vector4(1, 0, 0, 1), Vector2(1, 0), Colors::White});
    vertices.push_back({Vector3(-hx, hy, -hz), Vector3(0, 1, 0), Vector4(1, 0, 0, 1), Vector2(0, 0), Colors::White});

    // Bottom face (Y-)
    vertices.push_back({Vector3(-hx, -hy, -hz), Vector3(0, -1, 0), Vector4(1, 0, 0, 1), Vector2(0, 1), Colors::White});
    vertices.push_back({Vector3( hx, -hy, -hz), Vector3(0, -1, 0), Vector4(1, 0, 0, 1), Vector2(1, 1), Colors::White});
    vertices.push_back({Vector3( hx, -hy,  hz), Vector3(0, -1, 0), Vector4(1, 0, 0, 1), Vector2(1, 0), Colors::White});
    vertices.push_back({Vector3(-hx, -hy,  hz), Vector3(0, -1, 0), Vector4(1, 0, 0, 1), Vector2(0, 0), Colors::White});

    // Right face (X+)
    vertices.push_back({Vector3(hx, -hy,  hz), Vector3(1, 0, 0), Vector4(0, 0, -1, 1), Vector2(0, 1), Colors::White});
    vertices.push_back({Vector3(hx, -hy, -hz), Vector3(1, 0, 0), Vector4(0, 0, -1, 1), Vector2(1, 1), Colors::White});
    vertices.push_back({Vector3(hx,  hy, -hz), Vector3(1, 0, 0), Vector4(0, 0, -1, 1), Vector2(1, 0), Colors::White});
    vertices.push_back({Vector3(hx,  hy,  hz), Vector3(1, 0, 0), Vector4(0, 0, -1, 1), Vector2(0, 0), Colors::White});

    // Left face (X-)
    vertices.push_back({Vector3(-hx, -hy, -hz), Vector3(-1, 0, 0), Vector4(0, 0, 1, 1), Vector2(0, 1), Colors::White});
    vertices.push_back({Vector3(-hx, -hy,  hz), Vector3(-1, 0, 0), Vector4(0, 0, 1, 1), Vector2(1, 1), Colors::White});
    vertices.push_back({Vector3(-hx,  hy,  hz), Vector3(-1, 0, 0), Vector4(0, 0, 1, 1), Vector2(1, 0), Colors::White});
    vertices.push_back({Vector3(-hx,  hy, -hz), Vector3(-1, 0, 0), Vector4(0, 0, 1, 1), Vector2(0, 0), Colors::White});

    // インデックス（各面6インデックス）
    for (uint32_t face = 0; face < 6; ++face) {
        uint32_t base = face * 4;
        indices.push_back(base + 0);
        indices.push_back(base + 1);
        indices.push_back(base + 2);
        indices.push_back(base + 0);
        indices.push_back(base + 2);
        indices.push_back(base + 3);
    }

    MeshDesc desc;
    desc.vertices = std::move(vertices);
    desc.indices = std::move(indices);
    desc.name = "Box";

    auto mesh = Mesh::Create(desc);
    return RegisterPrimitive(std::move(mesh), "primitive://box");
}

MeshHandle MeshManager::CreateSphere(float radius, uint32_t segments)
{
    std::vector<MeshVertex> vertices;
    std::vector<uint32_t> indices;

    const float pi = 3.14159265358979323846f;

    // 頂点生成
    for (uint32_t lat = 0; lat <= segments; ++lat) {
        float theta = static_cast<float>(lat) * pi / segments;
        float sinTheta = std::sin(theta);
        float cosTheta = std::cos(theta);

        for (uint32_t lon = 0; lon <= segments; ++lon) {
            float phi = static_cast<float>(lon) * 2.0f * pi / segments;
            float sinPhi = std::sin(phi);
            float cosPhi = std::cos(phi);

            Vector3 normal(sinTheta * cosPhi, cosTheta, sinTheta * sinPhi);
            Vector3 position = normal * radius;
            Vector2 texCoord(static_cast<float>(lon) / segments,
                             static_cast<float>(lat) / segments);

            // タンジェント計算
            Vector3 tangent(-sinPhi, 0, cosPhi);

            MeshVertex vertex;
            vertex.position = position;
            vertex.normal = normal;
            vertex.tangent = Vector4(tangent.x, tangent.y, tangent.z, 1.0f);
            vertex.texCoord = texCoord;
            vertex.color = Colors::White;

            vertices.push_back(vertex);
        }
    }

    // インデックス生成
    for (uint32_t lat = 0; lat < segments; ++lat) {
        for (uint32_t lon = 0; lon < segments; ++lon) {
            uint32_t current = lat * (segments + 1) + lon;
            uint32_t next = current + segments + 1;

            indices.push_back(current);
            indices.push_back(next);
            indices.push_back(current + 1);

            indices.push_back(current + 1);
            indices.push_back(next);
            indices.push_back(next + 1);
        }
    }

    MeshDesc desc;
    desc.vertices = std::move(vertices);
    desc.indices = std::move(indices);
    desc.name = "Sphere";

    auto mesh = Mesh::Create(desc);
    return RegisterPrimitive(std::move(mesh), "primitive://sphere");
}

MeshHandle MeshManager::CreatePlane(float width, float depth, uint32_t subdivisionsX, uint32_t subdivisionsZ)
{
    std::vector<MeshVertex> vertices;
    std::vector<uint32_t> indices;

    float hw = width * 0.5f;
    float hd = depth * 0.5f;

    float dx = width / subdivisionsX;
    float dz = depth / subdivisionsZ;

    // 頂点生成
    for (uint32_t z = 0; z <= subdivisionsZ; ++z) {
        for (uint32_t x = 0; x <= subdivisionsX; ++x) {
            MeshVertex vertex;
            vertex.position = Vector3(-hw + x * dx, 0, -hd + z * dz);
            vertex.normal = Vector3(0, 1, 0);
            vertex.tangent = Vector4(1, 0, 0, 1);
            vertex.texCoord = Vector2(static_cast<float>(x) / subdivisionsX,
                                      static_cast<float>(z) / subdivisionsZ);
            vertex.color = Colors::White;

            vertices.push_back(vertex);
        }
    }

    // インデックス生成
    for (uint32_t z = 0; z < subdivisionsZ; ++z) {
        for (uint32_t x = 0; x < subdivisionsX; ++x) {
            uint32_t current = z * (subdivisionsX + 1) + x;
            uint32_t next = current + subdivisionsX + 1;

            indices.push_back(current);
            indices.push_back(next);
            indices.push_back(current + 1);

            indices.push_back(current + 1);
            indices.push_back(next);
            indices.push_back(next + 1);
        }
    }

    MeshDesc desc;
    desc.vertices = std::move(vertices);
    desc.indices = std::move(indices);
    desc.name = "Plane";

    auto mesh = Mesh::Create(desc);
    return RegisterPrimitive(std::move(mesh), "primitive://plane");
}

MeshHandle MeshManager::CreateCylinder(float radius, float height, uint32_t segments)
{
    std::vector<MeshVertex> vertices;
    std::vector<uint32_t> indices;

    const float pi = 3.14159265358979323846f;
    float hh = height * 0.5f;

    // 側面の頂点
    for (uint32_t i = 0; i <= segments; ++i) {
        float angle = static_cast<float>(i) * 2.0f * pi / segments;
        float c = std::cos(angle);
        float s = std::sin(angle);

        Vector3 normal(c, 0, s);
        float u = static_cast<float>(i) / segments;

        // 上端
        MeshVertex topVertex;
        topVertex.position = Vector3(radius * c, hh, radius * s);
        topVertex.normal = normal;
        topVertex.tangent = Vector4(-s, 0, c, 1);
        topVertex.texCoord = Vector2(u, 0);
        topVertex.color = Colors::White;
        vertices.push_back(topVertex);

        // 下端
        MeshVertex bottomVertex;
        bottomVertex.position = Vector3(radius * c, -hh, radius * s);
        bottomVertex.normal = normal;
        bottomVertex.tangent = Vector4(-s, 0, c, 1);
        bottomVertex.texCoord = Vector2(u, 1);
        bottomVertex.color = Colors::White;
        vertices.push_back(bottomVertex);
    }

    // 側面インデックス
    for (uint32_t i = 0; i < segments; ++i) {
        uint32_t i0 = i * 2;
        uint32_t i1 = i0 + 1;
        uint32_t i2 = i0 + 2;
        uint32_t i3 = i0 + 3;

        indices.push_back(i0);
        indices.push_back(i1);
        indices.push_back(i2);

        indices.push_back(i2);
        indices.push_back(i1);
        indices.push_back(i3);
    }

    // 上面中心
    uint32_t topCenterIdx = static_cast<uint32_t>(vertices.size());
    MeshVertex topCenter;
    topCenter.position = Vector3(0, hh, 0);
    topCenter.normal = Vector3(0, 1, 0);
    topCenter.tangent = Vector4(1, 0, 0, 1);
    topCenter.texCoord = Vector2(0.5f, 0.5f);
    topCenter.color = Colors::White;
    vertices.push_back(topCenter);

    // 上面の頂点
    uint32_t topStart = static_cast<uint32_t>(vertices.size());
    for (uint32_t i = 0; i <= segments; ++i) {
        float angle = static_cast<float>(i) * 2.0f * pi / segments;
        float c = std::cos(angle);
        float s = std::sin(angle);

        MeshVertex v;
        v.position = Vector3(radius * c, hh, radius * s);
        v.normal = Vector3(0, 1, 0);
        v.tangent = Vector4(1, 0, 0, 1);
        v.texCoord = Vector2(c * 0.5f + 0.5f, s * 0.5f + 0.5f);
        v.color = Colors::White;
        vertices.push_back(v);
    }

    // 上面インデックス
    for (uint32_t i = 0; i < segments; ++i) {
        indices.push_back(topCenterIdx);
        indices.push_back(topStart + i + 1);
        indices.push_back(topStart + i);
    }

    // 下面中心
    uint32_t bottomCenterIdx = static_cast<uint32_t>(vertices.size());
    MeshVertex bottomCenter;
    bottomCenter.position = Vector3(0, -hh, 0);
    bottomCenter.normal = Vector3(0, -1, 0);
    bottomCenter.tangent = Vector4(1, 0, 0, 1);
    bottomCenter.texCoord = Vector2(0.5f, 0.5f);
    bottomCenter.color = Colors::White;
    vertices.push_back(bottomCenter);

    // 下面の頂点
    uint32_t bottomStart = static_cast<uint32_t>(vertices.size());
    for (uint32_t i = 0; i <= segments; ++i) {
        float angle = static_cast<float>(i) * 2.0f * pi / segments;
        float c = std::cos(angle);
        float s = std::sin(angle);

        MeshVertex v;
        v.position = Vector3(radius * c, -hh, radius * s);
        v.normal = Vector3(0, -1, 0);
        v.tangent = Vector4(1, 0, 0, 1);
        v.texCoord = Vector2(c * 0.5f + 0.5f, s * 0.5f + 0.5f);
        v.color = Colors::White;
        vertices.push_back(v);
    }

    // 下面インデックス
    for (uint32_t i = 0; i < segments; ++i) {
        indices.push_back(bottomCenterIdx);
        indices.push_back(bottomStart + i);
        indices.push_back(bottomStart + i + 1);
    }

    MeshDesc desc;
    desc.vertices = std::move(vertices);
    desc.indices = std::move(indices);
    desc.name = "Cylinder";

    auto mesh = Mesh::Create(desc);
    return RegisterPrimitive(std::move(mesh), "primitive://cylinder");
}

//============================================================================
// キャッシュ管理
//============================================================================

void MeshManager::ClearCache()
{
    handleCache_.clear();
    LOG_INFO("[MeshManager] Cache cleared");
}

MeshCacheStats MeshManager::GetCacheStats() const
{
    stats_.meshCount = 0;
    stats_.totalVertices = 0;
    stats_.totalIndices = 0;
    stats_.totalMemoryBytes = 0;

    for (const auto& slot : slots_) {
        if (slot.inUse && slot.mesh) {
            stats_.meshCount++;
            stats_.totalVertices += slot.mesh->GetVertexCount();
            stats_.totalIndices += slot.mesh->GetIndexCount();
            stats_.totalMemoryBytes += slot.mesh->GpuSize();
        }
    }

    return stats_;
}

//============================================================================
// 内部ヘルパー
//============================================================================

uint64_t MeshManager::ComputeCacheKey(const std::string& path) const
{
    return HashUtil::Fnv1aString(path);
}

MeshHandle MeshManager::AllocateSlot(MeshPtr mesh)
{
    uint16_t index;

    if (!freeIndices_.empty()) {
        index = freeIndices_.front();
        freeIndices_.pop();
    }
    else {
        if (slots_.size() >= 65535) {
            LOG_ERROR("[MeshManager] Maximum slot count reached");
            return MeshHandle::Invalid();
        }
        index = static_cast<uint16_t>(slots_.size());
        slots_.push_back({});
    }

    auto& slot = slots_[index];
    slot.mesh = std::move(mesh);
    slot.refCount = 0;
    slot.generation = (slot.generation + 1) & 0x7FFF;  // 世代を更新（15bit上限）
    if (slot.generation == 0) slot.generation = 1;      // 0は無効値
    slot.inUse = true;

    return MeshHandle::Create(index, slot.generation);
}

void MeshManager::AddToScope(MeshHandle handle, ScopeId scope)
{
    auto it = scopes_.find(scope);
    if (it != scopes_.end()) {
        it->second.meshes.push_back(handle);
    }
}

void MeshManager::IncrementRefCount(MeshHandle handle)
{
    if (!handle.IsValid()) return;

    uint16_t index = handle.GetIndex();
    if (index < slots_.size() && slots_[index].inUse &&
        slots_[index].generation == handle.GetGeneration()) {
        slots_[index].refCount++;
    }
}

void MeshManager::DecrementRefCount(MeshHandle handle)
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

MeshHandle MeshManager::LoadInScope(const std::string& path, const MeshLoadOptions& options, ScopeId scope)
{
    // キャッシュチェック
    uint64_t cacheKey = ComputeCacheKey(path);
    auto cacheIt = handleCache_.find(cacheKey);

    if (cacheIt != handleCache_.end()) {
        MeshHandle handle = cacheIt->second;
        if (Get(handle) != nullptr) {
            stats_.hitCount++;
            IncrementRefCount(handle);
            AddToScope(handle, scope);
            return handle;
        }
        // 無効なキャッシュエントリを削除
        handleCache_.erase(cacheIt);
    }

    stats_.missCount++;

    // ローダーでロード
    MeshLoadResult result = MeshLoaderRegistry::Get().Load(path, options);
    if (!result.IsValid()) {
        LOG_ERROR("[MeshManager] Failed to load mesh: " + path);
        return MeshHandle::Invalid();
    }

    // 最初のメッシュをハンドルに割り当て
    MeshHandle handle = AllocateSlot(result.meshes[0]);
    if (!handle.IsValid()) {
        return MeshHandle::Invalid();
    }

    // キャッシュに追加
    handleCache_[cacheKey] = handle;

    // スコープに追加
    IncrementRefCount(handle);
    AddToScope(handle, scope);

    LOG_INFO("[MeshManager] Loaded: " + path);
    return handle;
}

MeshHandle MeshManager::RegisterPrimitive(MeshPtr mesh, const std::string& name)
{
    if (!mesh) {
        return MeshHandle::Invalid();
    }

    // キャッシュチェック
    uint64_t cacheKey = ComputeCacheKey(name);
    auto cacheIt = handleCache_.find(cacheKey);

    if (cacheIt != handleCache_.end()) {
        MeshHandle handle = cacheIt->second;
        if (Get(handle) != nullptr) {
            IncrementRefCount(handle);
            AddToScope(handle, kGlobalScope);
            return handle;
        }
        handleCache_.erase(cacheIt);
    }

    // スロットに割り当て
    MeshHandle handle = AllocateSlot(std::move(mesh));
    if (!handle.IsValid()) {
        return MeshHandle::Invalid();
    }

    // キャッシュに追加
    handleCache_[cacheKey] = handle;

    // グローバルスコープに追加（永続）
    IncrementRefCount(handle);
    AddToScope(handle, kGlobalScope);

    LOG_INFO("[MeshManager] Registered primitive: " + name);
    return handle;
}

