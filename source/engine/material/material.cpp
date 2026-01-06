//----------------------------------------------------------------------------
//! @file   material.cpp
//! @brief  PBRマテリアルクラス実装
//----------------------------------------------------------------------------
#include "material.h"
#include "dx11/graphics_context.h"
#include "common/logging/logging.h"

std::shared_ptr<Material> Material::Create(const MaterialDesc& desc)
{
    auto material = std::shared_ptr<Material>(new Material());
    material->params_ = desc.params;
    material->name_ = desc.name;

    // テクスチャハンドルをコピー
    for (size_t i = 0; i < static_cast<size_t>(MaterialTextureSlot::Count); ++i) {
        material->textures_[i] = desc.textures[i];
    }

    // テクスチャ使用フラグを設定
    material->params_.useAlbedoMap = desc.textures[0].IsValid() ? 1 : 0;
    material->params_.useNormalMap = desc.textures[1].IsValid() ? 1 : 0;
    material->params_.useMetallicMap = desc.textures[2].IsValid() ? 1 : 0;
    material->params_.useRoughnessMap = desc.textures[3].IsValid() ? 1 : 0;

    // 定数バッファ作成
    material->constantBuffer_ = Buffer::CreateConstant(sizeof(MaterialParams));
    if (!material->constantBuffer_) {
        LOG_ERROR("[Material::Create] Failed to create constant buffer");
        return nullptr;
    }

    // 初期値で更新
    material->UpdateConstantBuffer();

    LOG_INFO("[Material::Create] Created material '" + material->name_ + "'");
    return material;
}

std::shared_ptr<Material> Material::CreateDefault()
{
    MaterialDesc desc;
    desc.params.albedoColor = Colors::White;
    desc.params.metallic = 0.0f;
    desc.params.roughness = 0.5f;
    desc.params.ao = 1.0f;
    desc.name = "Default";
    return Create(desc);
}

void Material::SetAlbedoColor(const Color& color) noexcept
{
    params_.albedoColor = color;
    dirty_ = true;
}

void Material::SetMetallic(float value) noexcept
{
    params_.metallic = Clamp(value, 0.0f, 1.0f);
    dirty_ = true;
}

void Material::SetRoughness(float value) noexcept
{
    params_.roughness = Clamp(value, 0.04f, 1.0f);  // 最小0.04（完全鏡面を避ける）
    dirty_ = true;
}

void Material::SetAO(float value) noexcept
{
    params_.ao = Clamp(value, 0.0f, 1.0f);
    dirty_ = true;
}

void Material::SetEmissive(const Color& color, float strength) noexcept
{
    params_.emissiveColor = color;
    params_.emissiveStrength = strength;
    dirty_ = true;
}

void Material::SetTexture(MaterialTextureSlot slot, TextureHandle handle) noexcept
{
    size_t index = static_cast<size_t>(slot);
    if (index >= static_cast<size_t>(MaterialTextureSlot::Count)) {
        return;
    }

    textures_[index] = handle;

    // 使用フラグを更新
    switch (slot) {
        case MaterialTextureSlot::Albedo:
            params_.useAlbedoMap = handle.IsValid() ? 1 : 0;
            break;
        case MaterialTextureSlot::Normal:
            params_.useNormalMap = handle.IsValid() ? 1 : 0;
            break;
        case MaterialTextureSlot::Metallic:
            params_.useMetallicMap = handle.IsValid() ? 1 : 0;
            break;
        case MaterialTextureSlot::Roughness:
            params_.useRoughnessMap = handle.IsValid() ? 1 : 0;
            break;
        default:
            break;
    }

    dirty_ = true;
}

TextureHandle Material::GetTexture(MaterialTextureSlot slot) const noexcept
{
    size_t index = static_cast<size_t>(slot);
    if (index >= static_cast<size_t>(MaterialTextureSlot::Count)) {
        return TextureHandle::Invalid();
    }
    return textures_[index];
}

void Material::UpdateConstantBuffer()
{
    if (!dirty_ || !constantBuffer_) {
        return;
    }

    auto& ctx = GraphicsContext::Get();
    ctx.UpdateConstantBuffer(constantBuffer_.get(), params_);
    dirty_ = false;
}
