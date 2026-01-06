//----------------------------------------------------------------------------
//! @file   material.h
//! @brief  PBRマテリアルクラス
//----------------------------------------------------------------------------
#pragma once

#include "engine/texture/texture_handle.h"
#include "engine/math/math_types.h"
#include "engine/math/color.h"
#include "dx11/gpu/buffer.h"
#include "common/utility/non_copyable.h"
#include <memory>
#include <string>

//============================================================================
//! @brief マテリアルテクスチャスロット
//============================================================================
enum class MaterialTextureSlot : uint32_t
{
    Albedo = 0,      //!< ベースカラー (t0)
    Normal = 1,      //!< ノーマルマップ (t1)
    Metallic = 2,    //!< メタリックマップ (t2)
    Roughness = 3,   //!< ラフネスマップ (t3)
    AO = 4,          //!< アンビエントオクルージョン (t4)
    Count = 5
};

//============================================================================
//! @brief マテリアルパラメータ（定数バッファ用）
//!
//! @note 16バイトアラインメント必須
//============================================================================
struct alignas(16) MaterialParams
{
    Color albedoColor = Colors::White;     //!< ベースカラー (16 bytes)
    float metallic = 0.0f;                 //!< メタリック値 (4 bytes)
    float roughness = 0.5f;                //!< ラフネス値 (4 bytes)
    float ao = 1.0f;                       //!< AO強度 (4 bytes)
    float emissiveStrength = 0.0f;         //!< エミッシブ強度 (4 bytes)
    Color emissiveColor = Color(0,0,0,1);  //!< エミッシブカラー (16 bytes)
    uint32_t useAlbedoMap = 0;             //!< Albedoマップ使用フラグ (4 bytes)
    uint32_t useNormalMap = 0;             //!< Normalマップ使用フラグ (4 bytes)
    uint32_t useMetallicMap = 0;           //!< Metallicマップ使用フラグ (4 bytes)
    uint32_t useRoughnessMap = 0;          //!< Roughnessマップ使用フラグ (4 bytes)
};  // Total: 64 bytes

static_assert(sizeof(MaterialParams) == 64, "MaterialParams must be 64 bytes");

//============================================================================
//! @brief マテリアル記述子
//============================================================================
struct MaterialDesc
{
    MaterialParams params;
    TextureHandle textures[static_cast<size_t>(MaterialTextureSlot::Count)] = {};
    std::string name;
};

//============================================================================
//! @brief PBRマテリアルクラス
//!
//! @details シェーダーパラメータとテクスチャ参照を管理
//!          MaterialManagerが所有し、MaterialHandleで参照
//!
//! @note テクスチャはTextureHandle経由で参照（所有しない）
//============================================================================
class Material final : private NonCopyable
{
public:
    //! @brief マテリアル生成
    //! @param desc マテリアル記述子
    //! @return 生成されたマテリアル（失敗時nullptr）
    [[nodiscard]] static std::shared_ptr<Material> Create(const MaterialDesc& desc);

    //! @brief デフォルトマテリアル生成
    //! @return 白色、roughness=0.5のマテリアル
    [[nodiscard]] static std::shared_ptr<Material> CreateDefault();

    ~Material() = default;

    //----------------------------------------------------------
    //! @name パラメータ設定
    //----------------------------------------------------------
    //!@{

    void SetAlbedoColor(const Color& color) noexcept;
    void SetMetallic(float value) noexcept;
    void SetRoughness(float value) noexcept;
    void SetAO(float value) noexcept;
    void SetEmissive(const Color& color, float strength) noexcept;

    //!@}
    //----------------------------------------------------------
    //! @name パラメータ取得
    //----------------------------------------------------------
    //!@{

    [[nodiscard]] const Color& GetAlbedoColor() const noexcept { return params_.albedoColor; }
    [[nodiscard]] float GetMetallic() const noexcept { return params_.metallic; }
    [[nodiscard]] float GetRoughness() const noexcept { return params_.roughness; }
    [[nodiscard]] float GetAO() const noexcept { return params_.ao; }
    [[nodiscard]] const Color& GetEmissiveColor() const noexcept { return params_.emissiveColor; }
    [[nodiscard]] float GetEmissiveStrength() const noexcept { return params_.emissiveStrength; }

    [[nodiscard]] const MaterialParams& GetParams() const noexcept { return params_; }

    //!@}
    //----------------------------------------------------------
    //! @name テクスチャ管理
    //----------------------------------------------------------
    //!@{

    //! @brief テクスチャ設定
    //! @param slot テクスチャスロット
    //! @param handle テクスチャハンドル
    void SetTexture(MaterialTextureSlot slot, TextureHandle handle) noexcept;

    //! @brief テクスチャ取得
    //! @param slot テクスチャスロット
    //! @return テクスチャハンドル
    [[nodiscard]] TextureHandle GetTexture(MaterialTextureSlot slot) const noexcept;

    //!@}
    //----------------------------------------------------------
    //! @name バインディング
    //----------------------------------------------------------
    //!@{

    //! @brief 定数バッファ更新（dirty時のみ）
    void UpdateConstantBuffer();

    //! @brief 定数バッファ取得
    [[nodiscard]] Buffer* GetConstantBuffer() const noexcept { return constantBuffer_.get(); }

    //! @brief 変更フラグ設定
    void MarkDirty() noexcept { dirty_ = true; }

    //!@}
    //----------------------------------------------------------
    //! @name その他
    //----------------------------------------------------------
    //!@{

    [[nodiscard]] const std::string& GetName() const noexcept { return name_; }

    //!@}

private:
    Material() = default;

    MaterialParams params_;
    TextureHandle textures_[static_cast<size_t>(MaterialTextureSlot::Count)] = {};
    BufferPtr constantBuffer_;
    std::string name_;
    bool dirty_ = true;
};

using MaterialPtr = std::shared_ptr<Material>;
