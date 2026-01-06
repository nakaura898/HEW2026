//----------------------------------------------------------------------------
//! @file   mesh_renderer.h
//! @brief  メッシュレンダラーコンポーネント
//----------------------------------------------------------------------------
#pragma once

#include "component.h"
#include "engine/mesh/mesh_handle.h"
#include "engine/material/material_handle.h"
#include <vector>

//============================================================================
//! @brief メッシュレンダラーコンポーネント
//!
//! 3Dメッシュを描画するためのコンポーネント。
//! Transform（3D）と組み合わせて使用する。
//!
//! @code
//!   auto* mr = obj->AddComponent<MeshRenderer>();
//!   mr->SetMesh(MeshManager::Get().Load("models:/player.gltf"));
//!   mr->SetMaterial(MaterialManager::Get().CreateDefault());
//! @endcode
//============================================================================
class MeshRenderer : public Component
{
public:
    MeshRenderer() = default;

    //------------------------------------------------------------------------
    //! @brief コンストラクタ
    //! @param mesh メッシュハンドル
    //! @param material マテリアルハンドル
    //------------------------------------------------------------------------
    explicit MeshRenderer(MeshHandle mesh, MaterialHandle material = MaterialHandle::Invalid())
        : mesh_(mesh)
    {
        if (material.IsValid()) {
            materials_.push_back(material);
        }
    }

    //------------------------------------------------------------------------
    // メッシュ
    //------------------------------------------------------------------------

    //! @brief メッシュハンドルを取得
    [[nodiscard]] MeshHandle GetMesh() const noexcept { return mesh_; }

    //! @brief メッシュハンドルを設定
    void SetMesh(MeshHandle mesh) noexcept { mesh_ = mesh; }

    //------------------------------------------------------------------------
    // マテリアル
    //------------------------------------------------------------------------

    //! @brief マテリアル数を取得
    [[nodiscard]] size_t GetMaterialCount() const noexcept { return materials_.size(); }

    //! @brief マテリアルを取得
    //! @param index マテリアルインデックス
    //! @return マテリアルハンドル（範囲外の場合Invalid）
    [[nodiscard]] MaterialHandle GetMaterial(size_t index = 0) const noexcept {
        return index < materials_.size() ? materials_[index] : MaterialHandle::Invalid();
    }

    //! @brief マテリアルを設定（単一）
    //! @param material マテリアルハンドル
    void SetMaterial(MaterialHandle material) noexcept {
        materials_.clear();
        if (material.IsValid()) {
            materials_.push_back(material);
        }
    }

    //! @brief マテリアルを設定（インデックス指定）
    //! @param index マテリアルインデックス
    //! @param material マテリアルハンドル
    void SetMaterial(size_t index, MaterialHandle material) noexcept {
        if (index >= materials_.size()) {
            materials_.resize(index + 1, MaterialHandle::Invalid());
        }
        materials_[index] = material;
    }

    //! @brief 全マテリアルを取得
    [[nodiscard]] const std::vector<MaterialHandle>& GetMaterials() const noexcept {
        return materials_;
    }

    //! @brief 全マテリアルを設定
    void SetMaterials(const std::vector<MaterialHandle>& materials) noexcept {
        materials_ = materials;
    }

    //------------------------------------------------------------------------
    // 描画設定
    //------------------------------------------------------------------------

    //! @brief 描画有効フラグを取得
    [[nodiscard]] bool IsVisible() const noexcept { return visible_; }

    //! @brief 描画有効フラグを設定
    void SetVisible(bool visible) noexcept { visible_ = visible; }

    //! @brief シャドウキャストを取得
    [[nodiscard]] bool IsCastShadow() const noexcept { return castShadow_; }

    //! @brief シャドウキャストを設定
    void SetCastShadow(bool cast) noexcept { castShadow_ = cast; }

    //! @brief シャドウレシーブを取得
    [[nodiscard]] bool IsReceiveShadow() const noexcept { return receiveShadow_; }

    //! @brief シャドウレシーブを設定
    void SetReceiveShadow(bool receive) noexcept { receiveShadow_ = receive; }

    //------------------------------------------------------------------------
    // レンダリングレイヤー
    //------------------------------------------------------------------------

    //! @brief レンダリングレイヤーを取得
    [[nodiscard]] uint32_t GetRenderLayer() const noexcept { return renderLayer_; }

    //! @brief レンダリングレイヤーを設定
    void SetRenderLayer(uint32_t layer) noexcept { renderLayer_ = layer; }

private:
    MeshHandle mesh_;                            //!< メッシュハンドル
    std::vector<MaterialHandle> materials_;      //!< マテリアルハンドル配列（サブメッシュ対応）

    bool visible_ = true;                        //!< 描画有効フラグ
    bool castShadow_ = true;                     //!< シャドウキャスト
    bool receiveShadow_ = true;                  //!< シャドウレシーブ

    uint32_t renderLayer_ = 0;                   //!< レンダリングレイヤー（ビットマスク）
};
