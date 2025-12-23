//----------------------------------------------------------------------------
//! @file   depth_stencil_view.h
//! @brief  深度ステンシルビュー（DSV）ラッパークラス
//----------------------------------------------------------------------------
#pragma once

#include "dx11/gpu_common.h"
#include <memory>

//===========================================================================
//! 深度ステンシルビュー
//! @brief ID3D11DepthStencilViewのラッパー
//===========================================================================
class DepthStencilView final : private NonCopyable
{
public:
    //----------------------------------------------------------
    //! @name   生成
    //----------------------------------------------------------
    //!@{

    //! Texture1DからDSVを作成
    //! @param [in] texture D3D11テクスチャ
    //! @param [in] desc    DSV記述子（nullptrの場合はデフォルト）
    [[nodiscard]] static std::unique_ptr<DepthStencilView> CreateFromTexture1D(
        ID3D11Texture1D* texture,
        const D3D11_DEPTH_STENCIL_VIEW_DESC* desc = nullptr);

    //! Texture2DからDSVを作成
    //! @param [in] texture D3D11テクスチャ
    //! @param [in] desc    DSV記述子（nullptrの場合はデフォルト）
    [[nodiscard]] static std::unique_ptr<DepthStencilView> CreateFromTexture2D(
        ID3D11Texture2D* texture,
        const D3D11_DEPTH_STENCIL_VIEW_DESC* desc = nullptr);

    //! 任意のリソースからDSVを作成
    //! @param [in] resource D3D11リソース
    //! @param [in] desc     DSV記述子
    [[nodiscard]] static std::unique_ptr<DepthStencilView> Create(
        ID3D11Resource* resource,
        const D3D11_DEPTH_STENCIL_VIEW_DESC& desc);

    //! 既存のDSVからラッパーを作成
    [[nodiscard]] static std::unique_ptr<DepthStencilView> FromD3DView(
        ComPtr<ID3D11DepthStencilView> dsv);

    //!@}
    //----------------------------------------------------------
    //! @name   アクセサ
    //----------------------------------------------------------
    //!@{

    [[nodiscard]] ID3D11DepthStencilView* Get() const noexcept { return dsv_.Get(); }
    [[nodiscard]] ID3D11DepthStencilView* const* GetAddressOf() const noexcept { return dsv_.GetAddressOf(); }
    [[nodiscard]] bool IsValid() const noexcept { return dsv_ != nullptr; }

    //! 所有権を放棄してComPtrを取得
    [[nodiscard]] ComPtr<ID3D11DepthStencilView> Detach() noexcept { return std::move(dsv_); }

    //! 記述子を取得
    [[nodiscard]] D3D11_DEPTH_STENCIL_VIEW_DESC GetDesc() const noexcept;

    //!@}
    //----------------------------------------------------------
    //! @name   ライフサイクル
    //----------------------------------------------------------
    //!@{

    ~DepthStencilView() = default;
    DepthStencilView(DepthStencilView&&) = default;
    DepthStencilView& operator=(DepthStencilView&&) = default;

    //!@}

private:
    DepthStencilView() = default;

    ComPtr<ID3D11DepthStencilView> dsv_;
};
