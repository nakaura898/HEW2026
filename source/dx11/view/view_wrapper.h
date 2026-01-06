//----------------------------------------------------------------------------
//! @file   view_wrapper.h
//! @brief  D3D11ビューの統合テンプレートラッパー
//!
//! @details 4つのビュー型（RTV, DSV, SRV, UAV）を単一テンプレートで統合。
//!          ViewTraits特殊化でビュー固有の生成ロジックを抽象化。
//!
//! @note 後方互換性のため、既存クラス名をtype aliasとして提供:
//!       - RenderTargetView
//!       - DepthStencilView
//!       - ShaderResourceView
//!       - UnorderedAccessView
//----------------------------------------------------------------------------
#pragma once

#include "dx11/gpu_common.h"
#include "dx11/graphics_device.h"
#include "common/logging/logging.h"
#include <memory>
#include <type_traits>

//============================================================================
//! @brief ビュー型特性（テンプレート特殊化で各ビューの差異を吸収）
//============================================================================
template<typename ViewType>
struct ViewTraits;

//----------------------------------------------------------------------------
//! @brief RenderTargetView特性
//----------------------------------------------------------------------------
template<>
struct ViewTraits<ID3D11RenderTargetView>
{
    using DescType = D3D11_RENDER_TARGET_VIEW_DESC;
    static constexpr const char* Name = "RTV";
    static constexpr bool SupportsBuffer = true;
    static constexpr bool SupportsTexture3D = true;

    static HRESULT Create(ID3D11Device5* device, ID3D11Resource* resource,
                          const DescType* desc, ID3D11RenderTargetView** out)
    {
        return device->CreateRenderTargetView(resource, desc, out);
    }
};

//----------------------------------------------------------------------------
//! @brief DepthStencilView特性
//----------------------------------------------------------------------------
template<>
struct ViewTraits<ID3D11DepthStencilView>
{
    using DescType = D3D11_DEPTH_STENCIL_VIEW_DESC;
    static constexpr const char* Name = "DSV";
    static constexpr bool SupportsBuffer = false;  // DSVはバッファ非対応
    static constexpr bool SupportsTexture3D = false;  // DSVはTexture3D非対応

    static HRESULT Create(ID3D11Device5* device, ID3D11Resource* resource,
                          const DescType* desc, ID3D11DepthStencilView** out)
    {
        return device->CreateDepthStencilView(resource, desc, out);
    }
};

//----------------------------------------------------------------------------
//! @brief ShaderResourceView特性
//----------------------------------------------------------------------------
template<>
struct ViewTraits<ID3D11ShaderResourceView>
{
    using DescType = D3D11_SHADER_RESOURCE_VIEW_DESC;
    static constexpr const char* Name = "SRV";
    static constexpr bool SupportsBuffer = true;
    static constexpr bool SupportsTexture3D = true;

    static HRESULT Create(ID3D11Device5* device, ID3D11Resource* resource,
                          const DescType* desc, ID3D11ShaderResourceView** out)
    {
        return device->CreateShaderResourceView(resource, desc, out);
    }
};

//----------------------------------------------------------------------------
//! @brief UnorderedAccessView特性
//----------------------------------------------------------------------------
template<>
struct ViewTraits<ID3D11UnorderedAccessView>
{
    using DescType = D3D11_UNORDERED_ACCESS_VIEW_DESC;
    static constexpr const char* Name = "UAV";
    static constexpr bool SupportsBuffer = true;
    static constexpr bool SupportsTexture3D = true;

    static HRESULT Create(ID3D11Device5* device, ID3D11Resource* resource,
                          const DescType* desc, ID3D11UnorderedAccessView** out)
    {
        return device->CreateUnorderedAccessView(resource, desc, out);
    }
};

//============================================================================
//! @brief D3D11ビュー統合ラッパー
//!
//! @tparam ViewType D3D11ビュー型（ID3D11RenderTargetView等）
//!
//! @details RTV/DSV/SRV/UAVを単一テンプレートで実装。
//!          ViewTraits特殊化で各ビュー固有の生成ロジックを抽象化。
//============================================================================
template<typename ViewType>
class ViewWrapper final : private NonCopyable
{
public:
    using Traits = ViewTraits<ViewType>;
    using DescType = typename Traits::DescType;
    using Ptr = std::unique_ptr<ViewWrapper>;

    //----------------------------------------------------------
    //! @name   直接生成（ComPtr返却、ラッパー不要時に使用）
    //----------------------------------------------------------
    //!@{

    //! Texture2DからComPtrを直接生成（効率的）
    [[nodiscard]] static ComPtr<ViewType> CreateViewFromTexture2D(
        ID3D11Texture2D* texture,
        const DescType* desc = nullptr)
    {
        return CreateViewDirect(texture, desc, "CreateViewFromTexture2D");
    }

    //! バッファからComPtrを直接生成（効率的、DSV非対応）
    template<typename T = ViewType>
    [[nodiscard]] static std::enable_if_t<ViewTraits<T>::SupportsBuffer, ComPtr<ViewType>>
    CreateViewFromBuffer(
        ID3D11Buffer* buffer,
        const DescType* desc = nullptr)
    {
        return CreateViewDirect(buffer, desc, "CreateViewFromBuffer");
    }

    //!@}
    //----------------------------------------------------------
    //! @name   生成（汎用、ラッパー返却）
    //----------------------------------------------------------
    //!@{

    //! 任意のリソースからビューを作成
    //! @param [in] resource D3D11リソース
    //! @param [in] desc     ビュー記述子
    [[nodiscard]] static Ptr Create(
        ID3D11Resource* resource,
        const DescType& desc)
    {
        if (!resource) {
            LOG_ERROR(std::string("ViewWrapper<") + Traits::Name + ">::Create - resource is null");
            return nullptr;
        }

        auto* device = GetD3D11Device();
        if (!device) {
            LOG_ERROR(std::string("ViewWrapper<") + Traits::Name + ">::Create - device is null");
            return nullptr;
        }

        auto wrapper = Ptr(new ViewWrapper());
        HRESULT hr = Traits::Create(device, resource, &desc, wrapper->view_.GetAddressOf());
        if (FAILED(hr)) {
            LOG_ERROR(std::string("ViewWrapper<") + Traits::Name + ">::Create failed");
            return nullptr;
        }

        return wrapper;
    }

    //! 既存のビューからラッパーを作成
    //! @param [in] view 既存のD3D11ビュー
    [[nodiscard]] static Ptr FromD3DView(ComPtr<ViewType> view)
    {
        if (!view) {
            LOG_ERROR(std::string("ViewWrapper<") + Traits::Name + ">::FromD3DView - view is null");
            return nullptr;
        }

        auto wrapper = Ptr(new ViewWrapper());
        wrapper->view_ = std::move(view);
        return wrapper;
    }

    //!@}
    //----------------------------------------------------------
    //! @name   生成（テクスチャ）
    //----------------------------------------------------------
    //!@{

    //! Texture1Dからビューを作成
    [[nodiscard]] static Ptr CreateFromTexture1D(
        ID3D11Texture1D* texture,
        const DescType* desc = nullptr)
    {
        return CreateFromResourceImpl(texture, desc, "CreateFromTexture1D");
    }

    //! Texture2Dからビューを作成
    [[nodiscard]] static Ptr CreateFromTexture2D(
        ID3D11Texture2D* texture,
        const DescType* desc = nullptr)
    {
        return CreateFromResourceImpl(texture, desc, "CreateFromTexture2D");
    }

    //! Texture3Dからビューを作成（DSV非対応）
    template<typename T = ViewType>
    [[nodiscard]] static std::enable_if_t<ViewTraits<T>::SupportsTexture3D, Ptr>
    CreateFromTexture3D(
        ID3D11Texture3D* texture,
        const DescType* desc = nullptr)
    {
        return CreateFromResourceImpl(texture, desc, "CreateFromTexture3D");
    }

    //!@}
    //----------------------------------------------------------
    //! @name   生成（バッファ）
    //----------------------------------------------------------
    //!@{

    //! バッファからビューを作成（DSV非対応）
    template<typename T = ViewType>
    [[nodiscard]] static std::enable_if_t<ViewTraits<T>::SupportsBuffer, Ptr>
    CreateFromBuffer(
        ID3D11Buffer* buffer,
        const DescType* desc = nullptr)
    {
        return CreateFromResourceImpl(buffer, desc, "CreateFromBuffer");
    }

    //!@}
    //----------------------------------------------------------
    //! @name   アクセサ
    //----------------------------------------------------------
    //!@{

    [[nodiscard]] ViewType* Get() const noexcept { return view_.Get(); }
    [[nodiscard]] ViewType* const* GetAddressOf() const noexcept { return view_.GetAddressOf(); }
    [[nodiscard]] bool IsValid() const noexcept { return view_ != nullptr; }

    //! 所有権を放棄してComPtrを取得
    [[nodiscard]] ComPtr<ViewType> Detach() noexcept { return std::move(view_); }

    //! 記述子を取得
    [[nodiscard]] DescType GetDesc() const noexcept
    {
        DescType desc = {};
        if (view_) {
            view_->GetDesc(&desc);
        }
        return desc;
    }

    //!@}
    //----------------------------------------------------------
    //! @name   ライフサイクル
    //----------------------------------------------------------
    //!@{

    ~ViewWrapper() = default;
    ViewWrapper(ViewWrapper&&) = default;
    ViewWrapper& operator=(ViewWrapper&&) = default;

    //!@}

private:
    ViewWrapper() = default;

    //! ComPtrを直接返す共通実装（ラッパー生成なし）
    template<typename ResourceType>
    [[nodiscard]] static ComPtr<ViewType> CreateViewDirect(
        ResourceType* resource,
        const DescType* desc,
        const char* methodName)
    {
        if (!resource) {
            LOG_ERROR(std::string("ViewWrapper<") + Traits::Name + ">::" + methodName + " - resource is null");
            return nullptr;
        }

        auto* device = GetD3D11Device();
        if (!device) {
            LOG_ERROR(std::string("ViewWrapper<") + Traits::Name + ">::" + methodName + " - device is null");
            return nullptr;
        }

        ComPtr<ViewType> view;
        HRESULT hr = Traits::Create(device, resource, desc, view.GetAddressOf());
        if (FAILED(hr)) {
            LOG_ERROR(std::string("ViewWrapper<") + Traits::Name + ">::" + methodName + " failed");
            return nullptr;
        }

        return view;
    }

    //! リソースからビュー作成の共通実装（ラッパー返却）
    template<typename ResourceType>
    [[nodiscard]] static Ptr CreateFromResourceImpl(
        ResourceType* resource,
        const DescType* desc,
        const char* methodName)
    {
        if (!resource) {
            LOG_ERROR(std::string("ViewWrapper<") + Traits::Name + ">::" + methodName + " - resource is null");
            return nullptr;
        }

        auto* device = GetD3D11Device();
        if (!device) {
            LOG_ERROR(std::string("ViewWrapper<") + Traits::Name + ">::" + methodName + " - device is null");
            return nullptr;
        }

        auto wrapper = Ptr(new ViewWrapper());
        HRESULT hr = Traits::Create(device, resource, desc, wrapper->view_.GetAddressOf());
        if (FAILED(hr)) {
            LOG_ERROR(std::string("ViewWrapper<") + Traits::Name + ">::" + methodName + " failed");
            return nullptr;
        }

        return wrapper;
    }

    ComPtr<ViewType> view_;
};

//============================================================================
//! @name 後方互換性エイリアス
//! @brief 既存コードとの互換性を維持するための型エイリアス
//============================================================================
//!@{
using RenderTargetView = ViewWrapper<ID3D11RenderTargetView>;
using DepthStencilView = ViewWrapper<ID3D11DepthStencilView>;
using ShaderResourceView = ViewWrapper<ID3D11ShaderResourceView>;
using UnorderedAccessView = ViewWrapper<ID3D11UnorderedAccessView>;
//!@}
