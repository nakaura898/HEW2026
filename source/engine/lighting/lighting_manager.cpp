//----------------------------------------------------------------------------
//! @file   lighting_manager.cpp
//! @brief  ライティングマネージャー実装
//----------------------------------------------------------------------------
#include "lighting_manager.h"
#include "dx11/graphics_context.h"
#include "common/logging/logging.h"
#include <cassert>
#include <cmath>

//============================================================================
// シングルトン管理
//============================================================================

LightingManager& LightingManager::Get()
{
    assert(instance_ && "LightingManager::Create() must be called first");
    return *instance_;
}

void LightingManager::Create()
{
    if (!instance_) {
        instance_ = std::unique_ptr<LightingManager>(new LightingManager());
        LOG_INFO("[LightingManager] Created");
    }
}

void LightingManager::Destroy()
{
    if (instance_) {
        instance_->Shutdown();
        instance_.reset();
        LOG_INFO("[LightingManager] Destroyed");
    }
}

LightingManager::~LightingManager()
{
    if (initialized_) {
        Shutdown();
    }
}

//============================================================================
// 初期化・終了
//============================================================================

void LightingManager::Initialize()
{
    if (initialized_) {
        LOG_WARN("[LightingManager] Already initialized");
        return;
    }

    // ライトスロットをクリア
    for (auto& slot : lightSlots_) {
        slot.active = false;
        slot.enabled = true;
        slot.data = {};
    }

    // 定数バッファ作成
    constantBuffer_ = Buffer::CreateConstant(sizeof(LightingConstants));
    if (!constantBuffer_) {
        LOG_ERROR("[LightingManager] Failed to create constant buffer");
        return;
    }

    // 初期値で定数バッファを更新
    constants_ = {};
    constants_.ambientColor = ambientColor_;
    constants_.cameraPosition = Vector4(cameraPosition_.x, cameraPosition_.y, cameraPosition_.z, 1.0f);
    constants_.numLights = 0;

    initialized_ = true;
    dirty_ = true;

    LOG_INFO("[LightingManager] Initialized");
}

void LightingManager::Shutdown()
{
    if (!initialized_) {
        return;
    }

    ClearAllLights();
    constantBuffer_.reset();

    initialized_ = false;
    LOG_INFO("[LightingManager] Shutdown");
}

//============================================================================
// ライト管理
//============================================================================

LightId LightingManager::AddDirectionalLight(const Vector3& direction, const Color& color, float intensity)
{
    LightId id = FindFreeSlot();
    if (id == kInvalidLightId) {
        LOG_WARN("[LightingManager] No free light slots available");
        return kInvalidLightId;
    }

    // 方向を正規化
    Vector3 dir = direction;
    dir.Normalize();

    lightSlots_[id].data = LightBuilder::Directional(dir, color, intensity);
    lightSlots_[id].active = true;
    lightSlots_[id].enabled = true;

    RecalculateActiveLightCount();
    dirty_ = true;

    LOG_INFO("[LightingManager] Added directional light (id=" + std::to_string(id) + ")");
    return id;
}

LightId LightingManager::AddPointLight(const Vector3& position, const Color& color, float intensity, float range)
{
    LightId id = FindFreeSlot();
    if (id == kInvalidLightId) {
        LOG_WARN("[LightingManager] No free light slots available");
        return kInvalidLightId;
    }

    lightSlots_[id].data = LightBuilder::Point(position, color, intensity, range);
    lightSlots_[id].active = true;
    lightSlots_[id].enabled = true;

    RecalculateActiveLightCount();
    dirty_ = true;

    LOG_INFO("[LightingManager] Added point light (id=" + std::to_string(id) + ")");
    return id;
}

LightId LightingManager::AddSpotLight(
    const Vector3& position,
    const Vector3& direction,
    const Color& color,
    float intensity,
    float range,
    float innerAngleDegrees,
    float outerAngleDegrees)
{
    LightId id = FindFreeSlot();
    if (id == kInvalidLightId) {
        LOG_WARN("[LightingManager] No free light slots available");
        return kInvalidLightId;
    }

    // 方向を正規化
    Vector3 dir = direction;
    dir.Normalize();

    lightSlots_[id].data = LightBuilder::Spot(position, dir, color, intensity, range,
                                              innerAngleDegrees, outerAngleDegrees);
    lightSlots_[id].active = true;
    lightSlots_[id].enabled = true;

    RecalculateActiveLightCount();
    dirty_ = true;

    LOG_INFO("[LightingManager] Added spot light (id=" + std::to_string(id) + ")");
    return id;
}

void LightingManager::RemoveLight(LightId lightId)
{
    if (lightId >= kMaxLights) {
        return;
    }

    lightSlots_[lightId].active = false;
    lightSlots_[lightId].data = {};

    RecalculateActiveLightCount();
    dirty_ = true;

    LOG_INFO("[LightingManager] Removed light (id=" + std::to_string(lightId) + ")");
}

void LightingManager::ClearAllLights()
{
    for (auto& slot : lightSlots_) {
        slot.active = false;
        slot.data = {};
    }

    activeLightCount_ = 0;
    dirty_ = true;

    LOG_INFO("[LightingManager] Cleared all lights");
}

LightData* LightingManager::GetLight(LightId lightId)
{
    if (lightId >= kMaxLights || !lightSlots_[lightId].active) {
        return nullptr;
    }
    return &lightSlots_[lightId].data;
}

const LightData* LightingManager::GetLight(LightId lightId) const
{
    if (lightId >= kMaxLights || !lightSlots_[lightId].active) {
        return nullptr;
    }
    return &lightSlots_[lightId].data;
}

//============================================================================
// ライトパラメータ変更
//============================================================================

void LightingManager::SetLightPosition(LightId lightId, const Vector3& position)
{
    if (LightData* light = GetLight(lightId)) {
        light->position.x = position.x;
        light->position.y = position.y;
        light->position.z = position.z;
        dirty_ = true;
    }
}

void LightingManager::SetLightDirection(LightId lightId, const Vector3& direction)
{
    if (LightData* light = GetLight(lightId)) {
        Vector3 dir = direction;
        dir.Normalize();
        light->direction.x = dir.x;
        light->direction.y = dir.y;
        light->direction.z = dir.z;
        dirty_ = true;
    }
}

void LightingManager::SetLightColor(LightId lightId, const Color& color)
{
    if (LightData* light = GetLight(lightId)) {
        float intensity = light->color.A();  // 強度は維持
        light->color = Color(color.R(), color.G(), color.B(), intensity);
        dirty_ = true;
    }
}

void LightingManager::SetLightIntensity(LightId lightId, float intensity)
{
    if (LightData* light = GetLight(lightId)) {
        light->color = Color(light->color.R(), light->color.G(), light->color.B(), intensity);
        dirty_ = true;
    }
}

void LightingManager::SetLightRange(LightId lightId, float range)
{
    if (LightData* light = GetLight(lightId)) {
        light->direction.w = range;
        dirty_ = true;
    }
}

void LightingManager::SetLightEnabled(LightId lightId, bool enabled)
{
    if (lightId < kMaxLights && lightSlots_[lightId].active) {
        lightSlots_[lightId].enabled = enabled;
        RecalculateActiveLightCount();
        dirty_ = true;
    }
}

bool LightingManager::IsLightEnabled(LightId lightId) const
{
    if (lightId < kMaxLights && lightSlots_[lightId].active) {
        return lightSlots_[lightId].enabled;
    }
    return false;
}

//============================================================================
// 環境設定
//============================================================================

void LightingManager::SetAmbientColor(const Color& color) noexcept
{
    ambientColor_ = color;
    dirty_ = true;
}

void LightingManager::SetCameraPosition(const Vector3& position) noexcept
{
    cameraPosition_ = position;
    dirty_ = true;
}

//============================================================================
// 更新・バインディング
//============================================================================

void LightingManager::Update()
{
    if (!dirty_ || !constantBuffer_) {
        return;
    }

    // 定数バッファを更新
    constants_.cameraPosition = Vector4(cameraPosition_.x, cameraPosition_.y, cameraPosition_.z, 1.0f);
    constants_.ambientColor = ambientColor_;
    constants_.numLights = 0;

    // 有効なライトを定数バッファにコピー
    for (uint32_t i = 0; i < kMaxLights; ++i) {
        if (lightSlots_[i].active && lightSlots_[i].enabled) {
            if (constants_.numLights < kMaxLights) {
                constants_.lights[constants_.numLights] = lightSlots_[i].data;
                constants_.numLights++;
            }
        }
    }

    // GPUバッファを更新
    auto& ctx = GraphicsContext::Get();
    ctx.UpdateConstantBuffer(constantBuffer_.get(), constants_);

    dirty_ = false;
}

void LightingManager::Bind(uint32_t slot)
{
    if (!constantBuffer_) {
        return;
    }

    auto& ctx = GraphicsContext::Get();
    ctx.SetPSConstantBuffer(slot, constantBuffer_.get());
    ctx.SetVSConstantBuffer(slot, constantBuffer_.get());
}

//============================================================================
// 内部ヘルパー
//============================================================================

LightId LightingManager::FindFreeSlot() const
{
    for (uint32_t i = 0; i < kMaxLights; ++i) {
        if (!lightSlots_[i].active) {
            return i;
        }
    }
    return kInvalidLightId;
}

void LightingManager::RecalculateActiveLightCount()
{
    activeLightCount_ = 0;
    for (const auto& slot : lightSlots_) {
        if (slot.active && slot.enabled) {
            activeLightCount_++;
        }
    }
}

