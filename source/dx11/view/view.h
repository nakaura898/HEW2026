//----------------------------------------------------------------------------
//! @file   view.h
//! @brief  D3D11ビューの統合テンプレートラッパー
//!
//! @details タグ型を使った明確なビュー管理。
//!          View<SRV>, View<RTV>, View<DSV>, View<UAV> の形式で使用。
//!
//! @warning ビュー型は必ずView<Tag>形式で宣言すること！
//!          非推奨: ComPtr<ID3D11ShaderResourceView> srv_;
//!          推奨:   View<SRV> srv_;
//!
//! @code
//! View<SRV> srv = View<SRV>::Create(texture);  // ビュー作成
//! if (srv) {                                    // 有効性チェック
//!     context->PSSetShaderResources(0, 1, srv.GetAddressOf());
//! }
//! @endcode
//----------------------------------------------------------------------------
#pragma once

#include "dx11/gpu_common.h"
#include "dx11/graphics_device.h"
#include "common/logging/logging.h"
#include <type_traits>

//============================================================================
//! @name ビュータグ型
//! @brief View<Tag>のTagとして使用する空の型
//============================================================================
//!@{
struct SRV {};  //!< ShaderResourceView
struct RTV {};  //!< RenderTargetView
struct DSV {};  //!< DepthStencilView
struct UAV {};  //!< UnorderedAccessView
//!@}

//============================================================================
//! @brief タグ→D3D11型のマッピング
//============================================================================
template<typename Tag> struct ViewTraits;

template<> struct ViewTraits<SRV> {
    using ViewType = ID3D11ShaderResourceView;
    using ViewDesc = D3D11_SHADER_RESOURCE_VIEW_DESC;
    static constexpr const char* Name = "SRV";
    static constexpr bool SupportsBuffer = true;
    static constexpr bool SupportsTexture3D = true;

    static HRESULT Create(ID3D11Device5* device, ID3D11Resource* resource,
                          const ViewDesc* desc, ViewType** out) {
        return device->CreateShaderResourceView(resource, desc, out);
    }
};

template<> struct ViewTraits<RTV> {
    using ViewType = ID3D11RenderTargetView;
    using ViewDesc = D3D11_RENDER_TARGET_VIEW_DESC;
    static constexpr const char* Name = "RTV";
    static constexpr bool SupportsBuffer = true;
    static constexpr bool SupportsTexture3D = true;

    static HRESULT Create(ID3D11Device5* device, ID3D11Resource* resource,
                          const ViewDesc* desc, ViewType** out) {
        return device->CreateRenderTargetView(resource, desc, out);
    }
};

template<> struct ViewTraits<DSV> {
    using ViewType = ID3D11DepthStencilView;
    using ViewDesc = D3D11_DEPTH_STENCIL_VIEW_DESC;
    static constexpr const char* Name = "DSV";
    static constexpr bool SupportsBuffer = false;
    static constexpr bool SupportsTexture3D = false;

    static HRESULT Create(ID3D11Device5* device, ID3D11Resource* resource,
                          const ViewDesc* desc, ViewType** out) {
        return device->CreateDepthStencilView(resource, desc, out);
    }
};

template<> struct ViewTraits<UAV> {
    using ViewType = ID3D11UnorderedAccessView;
    using ViewDesc = D3D11_UNORDERED_ACCESS_VIEW_DESC;
    static constexpr const char* Name = "UAV";
    static constexpr bool SupportsBuffer = true;
    static constexpr bool SupportsTexture3D = true;

    static HRESULT Create(ID3D11Device5* device, ID3D11Resource* resource,
                          const ViewDesc* desc, ViewType** out) {
        return device->CreateUnorderedAccessView(resource, desc, out);
    }
};

//============================================================================
//! @brief D3D11ビュー統合ラッパー
//!
//! @tparam Tag ビュータグ型（SRV, RTV, DSV, UAV）
//!
//! @code
//! View<SRV> srv = View<SRV>::Create(texture);
//! View<DSV> dsv = View<DSV>::Create(texture, &dsvDesc);
//! @endcode
//!
//! @note スレッドセーフ性:
//!       - Create系メソッドはID3D11Device5を使用するため、
//!         デバイスがマルチスレッド対応の場合は複数スレッドから呼び出し可能。
//!       - 生成されたビューの使用（Get/Bind等）は通常シングルスレッド専用の
//!         immediate contextで行うため、同一ビューへの同時アクセスは避けること。
//============================================================================
template<typename Tag>
class View final : private NonCopyable
{
public:
    using D3DType = typename ViewTraits<Tag>::ViewType;
    using DescType = typename ViewTraits<Tag>::ViewDesc;

    //----------------------------------------------------------
    //! @name   コンストラクタ
    //----------------------------------------------------------
    //!@{

    //! デフォルトコンストラクタ（空のビュー）
    View() = default;

    //! ComPtrから構築
    explicit View(ComPtr<D3DType> view) : view_(std::move(view)) {}

    //! ムーブコンストラクタ
    View(View&&) = default;
    View& operator=(View&&) = default;

    ~View() = default;

    //!@}
    //----------------------------------------------------------
    //! @name   ファクトリ
    //----------------------------------------------------------
    //!@{

    //! Texture2Dからビュー作成
    [[nodiscard]] static View Create(
        ID3D11Texture2D* texture,
        const DescType* desc = nullptr)
    {
        return View(CreateImpl(texture, desc, "Create(Texture2D)"));
    }

    //! Texture1Dからビュー作成
    [[nodiscard]] static View Create(
        ID3D11Texture1D* texture,
        const DescType* desc = nullptr)
    {
        return View(CreateImpl(texture, desc, "Create(Texture1D)"));
    }

    //! Texture3Dからビュー作成（DSV非対応）
    template<typename T = Tag>
    [[nodiscard]] static std::enable_if_t<ViewTraits<T>::SupportsTexture3D, View>
    Create(
        ID3D11Texture3D* texture,
        const DescType* desc = nullptr)
    {
        return View(CreateImpl(texture, desc, "Create(Texture3D)"));
    }

    //! バッファからビュー作成（DSV非対応）
    template<typename T = Tag>
    [[nodiscard]] static std::enable_if_t<ViewTraits<T>::SupportsBuffer, View>
    Create(
        ID3D11Buffer* buffer,
        const DescType* desc = nullptr)
    {
        return View(CreateImpl(buffer, desc, "Create(Buffer)"));
    }

    //! 任意のリソース + 記述子からビュー作成
    [[nodiscard]] static View Create(
        ID3D11Resource* resource,
        const DescType& desc)
    {
        return View(CreateImpl(resource, &desc, "Create(Resource)"));
    }

    //!@}
    //----------------------------------------------------------
    //! @name   アクセサ
    //----------------------------------------------------------
    //!@{

    [[nodiscard]] D3DType* Get() const noexcept { return view_.Get(); }
    [[nodiscard]] D3DType* const* GetAddressOf() const noexcept { return view_.GetAddressOf(); }
    [[nodiscard]] D3DType** ReleaseAndGetAddressOf() noexcept { return view_.ReleaseAndGetAddressOf(); }
    [[nodiscard]] bool IsValid() const noexcept { return view_ != nullptr; }

    //! bool変換（有効性チェック）
    explicit operator bool() const noexcept { return view_ != nullptr; }

    //! ComPtr取得（所有権維持）
    [[nodiscard]] const ComPtr<D3DType>& GetComPtr() const noexcept { return view_; }

    //! 所有権を放棄してComPtrを取得
    [[nodiscard]] ComPtr<D3DType> Detach() noexcept { return std::move(view_); }

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

private:
    //! ビュー作成の共通実装
    template<typename ResourceType>
    [[nodiscard]] static ComPtr<D3DType> CreateImpl(
        ResourceType* resource,
        const DescType* desc,
        const char* methodName)
    {
        if (!resource) {
            LOG_ERROR(std::string(ViewTraits<Tag>::Name) + "::" + methodName + " - resource is null");
            return nullptr;
        }

        auto* device = GetD3D11Device();
        if (!device) {
            LOG_ERROR(std::string(ViewTraits<Tag>::Name) + "::" + methodName + " - device is null");
            return nullptr;
        }

        ComPtr<D3DType> view;
        HRESULT hr = ViewTraits<Tag>::Create(device, resource, desc, view.GetAddressOf());
        if (FAILED(hr)) {
            LOG_ERROR(std::string(ViewTraits<Tag>::Name) + "::" + methodName + " failed");
            return nullptr;
        }

        return view;
    }

    ComPtr<D3DType> view_;
};
