//----------------------------------------------------------------------------
//! @file   view.h
//! @brief  D3D11ビューの統合テンプレートラッパー
//!
//! @details タグ型を使った明確なビュー管理。
//!          View<SRV>, View<RTV>, View<DSV>, View<UAV> の形式で使用。
//!
//! @code
//! // 推奨: View<Tag> + Create()
//! View<SRV> srv;                                  // 空で宣言
//! auto ptr = View<SRV>::Create(texture);          // ComPtr取得
//! srv = View<SRV>(ptr);                           // ラッパーに格納
//!
//! // または直接ComPtrとして使用
//! ComPtr<ID3D11ShaderResourceView> srv = View<SRV>::Create(texture);
//! @endcode
//----------------------------------------------------------------------------
#pragma once

#include "dx11/gpu_common.h"
#include "dx11/graphics_device.h"
#include "common/logging/logging.h"
#include <memory>
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
template<typename Tag> struct ViewTypeMap;

template<> struct ViewTypeMap<SRV> {
    using Type = ID3D11ShaderResourceView;
    using DescType = D3D11_SHADER_RESOURCE_VIEW_DESC;
    static constexpr const char* Name = "SRV";
    static constexpr bool SupportsBuffer = true;
    static constexpr bool SupportsTexture3D = true;

    static HRESULT Create(ID3D11Device5* device, ID3D11Resource* resource,
                          const DescType* desc, Type** out) {
        return device->CreateShaderResourceView(resource, desc, out);
    }
};

template<> struct ViewTypeMap<RTV> {
    using Type = ID3D11RenderTargetView;
    using DescType = D3D11_RENDER_TARGET_VIEW_DESC;
    static constexpr const char* Name = "RTV";
    static constexpr bool SupportsBuffer = true;
    static constexpr bool SupportsTexture3D = true;

    static HRESULT Create(ID3D11Device5* device, ID3D11Resource* resource,
                          const DescType* desc, Type** out) {
        return device->CreateRenderTargetView(resource, desc, out);
    }
};

template<> struct ViewTypeMap<DSV> {
    using Type = ID3D11DepthStencilView;
    using DescType = D3D11_DEPTH_STENCIL_VIEW_DESC;
    static constexpr const char* Name = "DSV";
    static constexpr bool SupportsBuffer = false;
    static constexpr bool SupportsTexture3D = false;

    static HRESULT Create(ID3D11Device5* device, ID3D11Resource* resource,
                          const DescType* desc, Type** out) {
        return device->CreateDepthStencilView(resource, desc, out);
    }
};

template<> struct ViewTypeMap<UAV> {
    using Type = ID3D11UnorderedAccessView;
    using DescType = D3D11_UNORDERED_ACCESS_VIEW_DESC;
    static constexpr const char* Name = "UAV";
    static constexpr bool SupportsBuffer = true;
    static constexpr bool SupportsTexture3D = true;

    static HRESULT Create(ID3D11Device5* device, ID3D11Resource* resource,
                          const DescType* desc, Type** out) {
        return device->CreateUnorderedAccessView(resource, desc, out);
    }
};

//============================================================================
//! @brief D3D11ビュー統合ラッパー
//!
//! @tparam Tag ビュータグ型（SRV, RTV, DSV, UAV）
//!
//! @details タグベースの明確なビュー管理。
//!          View<SRV>, View<RTV>, View<DSV>, View<UAV> の形式で使用。
//!
//! @code
//! View<SRV> srv;                              // 空で宣言
//! srv = View<SRV>(View<SRV>::Create(tex));    // 後から代入
//!
//! // ComPtr直接取得
//! auto ptr = View<SRV>::Create(texture);
//! @endcode
//============================================================================
template<typename Tag>
class View final : private NonCopyable
{
public:
    using Map = ViewTypeMap<Tag>;
    using D3DType = typename Map::Type;
    using ViewDesc = typename Map::DescType;

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
    //! @name   ファクトリ（ComPtr直接返却）
    //----------------------------------------------------------
    //!@{

    //! Texture2Dからビュー作成
    [[nodiscard]] static ComPtr<D3DType> Create(
        ID3D11Texture2D* texture,
        const ViewDesc* desc = nullptr)
    {
        return CreateImpl(texture, desc, "Create(Texture2D)");
    }

    //! Texture1Dからビュー作成
    [[nodiscard]] static ComPtr<D3DType> Create(
        ID3D11Texture1D* texture,
        const ViewDesc* desc = nullptr)
    {
        return CreateImpl(texture, desc, "Create(Texture1D)");
    }

    //! Texture3Dからビュー作成（DSV非対応）
    template<typename T = Tag>
    [[nodiscard]] static std::enable_if_t<ViewTypeMap<T>::SupportsTexture3D, ComPtr<D3DType>>
    Create(
        ID3D11Texture3D* texture,
        const ViewDesc* desc = nullptr)
    {
        return CreateImpl(texture, desc, "Create(Texture3D)");
    }

    //! バッファからビュー作成（DSV非対応）
    template<typename T = Tag>
    [[nodiscard]] static std::enable_if_t<ViewTypeMap<T>::SupportsBuffer, ComPtr<D3DType>>
    Create(
        ID3D11Buffer* buffer,
        const ViewDesc* desc = nullptr)
    {
        return CreateImpl(buffer, desc, "Create(Buffer)");
    }

    //! 任意のリソース + 記述子からビュー作成
    [[nodiscard]] static ComPtr<D3DType> Create(
        ID3D11Resource* resource,
        const ViewDesc& desc)
    {
        return CreateImpl(resource, &desc, "Create(Resource)");
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
    [[nodiscard]] ViewDesc GetDesc() const noexcept
    {
        ViewDesc desc = {};
        if (view_) {
            view_->GetDesc(&desc);
        }
        return desc;
    }

    //!@}
    //----------------------------------------------------------
    //! @name   後方互換（非推奨）
    //----------------------------------------------------------
    //!@{

    using ViewPtr = std::unique_ptr<View>;  //!< @deprecated ComPtr直接使用を推奨

    //! @deprecated Create()を使用してください
    [[nodiscard]] static ViewPtr CreateFromTexture2D(
        ID3D11Texture2D* texture,
        const ViewDesc* desc = nullptr)
    {
        auto view = Create(texture, desc);
        return view ? ViewPtr(new View(std::move(view))) : nullptr;
    }

    //! @deprecated Create()を使用してください
    [[nodiscard]] static ViewPtr CreateFromTexture1D(
        ID3D11Texture1D* texture,
        const ViewDesc* desc = nullptr)
    {
        auto view = Create(texture, desc);
        return view ? ViewPtr(new View(std::move(view))) : nullptr;
    }

    //! @deprecated Create()を使用してください
    template<typename T = Tag>
    [[nodiscard]] static std::enable_if_t<ViewTypeMap<T>::SupportsBuffer, ViewPtr>
    CreateFromBuffer(
        ID3D11Buffer* buffer,
        const ViewDesc* desc = nullptr)
    {
        auto view = Create(buffer, desc);
        return view ? ViewPtr(new View(std::move(view))) : nullptr;
    }

    //! @deprecated Create()を使用してください
    [[nodiscard]] static ComPtr<D3DType> CreateViewFromTexture2D(
        ID3D11Texture2D* texture,
        const ViewDesc* desc = nullptr)
    {
        return Create(texture, desc);
    }

    //!@}

private:
    //! ビュー作成の共通実装
    template<typename ResourceType>
    [[nodiscard]] static ComPtr<D3DType> CreateImpl(
        ResourceType* resource,
        const ViewDesc* desc,
        const char* methodName)
    {
        if (!resource) {
            LOG_ERROR(std::string(Map::Name) + "::" + methodName + " - resource is null");
            return nullptr;
        }

        auto* device = GetD3D11Device();
        if (!device) {
            LOG_ERROR(std::string(Map::Name) + "::" + methodName + " - device is null");
            return nullptr;
        }

        ComPtr<D3DType> view;
        HRESULT hr = Map::Create(device, resource, desc, view.GetAddressOf());
        if (FAILED(hr)) {
            LOG_ERROR(std::string(Map::Name) + "::" + methodName + " failed");
            return nullptr;
        }

        return view;
    }

    ComPtr<D3DType> view_;
};

//============================================================================
//! @name 後方互換性エイリアス（非推奨）
//============================================================================
//!@{
using ShaderResourceView = View<SRV>;
using RenderTargetView = View<RTV>;
using DepthStencilView = View<DSV>;
using UnorderedAccessView = View<UAV>;
//!@}
