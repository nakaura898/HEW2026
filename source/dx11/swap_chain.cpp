//----------------------------------------------------------------------------
//! @file   swap_chain.cpp
//! @brief  スワップチェーン管理実装
//----------------------------------------------------------------------------
#include "swap_chain.h"
#include "graphics_device.h"
#include "gpu/format.h"
#include "common/logging/logging.h"

namespace
{
    //! 既存のID3D11Texture2DをラップしてmutraTextureを作成
    [[nodiscard]] TexturePtr WrapBackBuffer(ComPtr<ID3D11Texture2D> texture)
    {
        RETURN_NULL_IF_NULL(texture.Get(), "[SwapChain] textureがnullです");

        D3D11_TEXTURE2D_DESC desc;
        texture->GetDesc(&desc);

        ID3D11Device* device = GetD3D11Device();
        RETURN_NULL_IF_NULL(device, "[SwapChain] D3D11Deviceがnullです");

        // sRGBフォーマットでRTVを作成（ガンマ補正を有効化）
        D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
        rtvDesc.Format = Format(desc.Format).addSrgb();
        rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
        rtvDesc.Texture2D.MipSlice = 0;

        ComPtr<ID3D11RenderTargetView> rtv;
        HRESULT hr = device->CreateRenderTargetView(texture.Get(), &rtvDesc, &rtv);
        RETURN_NULL_IF_FAILED(hr, "[SwapChain] RTV作成失敗");

        TextureDesc mutraDesc;
        mutraDesc.width = desc.Width;
        mutraDesc.height = desc.Height;
        mutraDesc.depth = 1;
        mutraDesc.mipLevels = desc.MipLevels;
        mutraDesc.arraySize = desc.ArraySize;
        mutraDesc.format = desc.Format;
        mutraDesc.usage = desc.Usage;
        mutraDesc.bindFlags = desc.BindFlags;
        mutraDesc.cpuAccess = desc.CPUAccessFlags;
        mutraDesc.dimension = TextureDimension::Tex2D;

        return std::make_shared<Texture>(
            std::move(texture), nullptr, std::move(rtv),
            nullptr, nullptr, mutraDesc);
    }
} // namespace

//===========================================================================
// コンストラクタ
//===========================================================================
SwapChain::SwapChain(
    HWND hwnd,
    const DXGI_SWAP_CHAIN_DESC1& desc,
    const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* fullscreenDesc)
{
    auto* device = GetD3D11Device();
    THROW_IF_NULL(device, "[SwapChain] D3D11Deviceがnullです");
    THROW_IF_NULL(hwnd, "[SwapChain] ウィンドウハンドルがnullです");
    THROW_IF_FALSE(desc.Width != 0 && desc.Height != 0, "[SwapChain] サイズが無効です");

    // DXGIデバイスを取得
    ComPtr<IDXGIDevice> dxgiDevice;
    HRESULT hr = device->QueryInterface(IID_PPV_ARGS(&dxgiDevice));
    THROW_IF_FAILED(hr, "[SwapChain] IDXGIDeviceの取得に失敗しました");

    // DXGIアダプターを取得
    ComPtr<IDXGIAdapter> dxgiAdapter;
    hr = dxgiDevice->GetAdapter(&dxgiAdapter);
    THROW_IF_FAILED(hr, "[SwapChain] IDXGIAdapterの取得に失敗しました");

    // DXGIファクトリを取得
    ComPtr<IDXGIFactory2> dxgiFactory2;
    hr = dxgiAdapter->GetParent(IID_PPV_ARGS(&dxgiFactory2));
    THROW_IF_FAILED(hr, "[SwapChain] IDXGIFactory2の取得に失敗しました");

    // CreateSwapChainForHwnd
    ComPtr<IDXGISwapChain1> swapChain1;
    hr = dxgiFactory2->CreateSwapChainForHwnd(
        device,
        hwnd,
        &desc,
        fullscreenDesc,
        nullptr,
        &swapChain1);
    THROW_IF_FAILED(hr, "[SwapChain] CreateSwapChainForHwndに失敗しました");

    // IDXGISwapChain3に変換
    hr = swapChain1.As(&swapChain_);
    THROW_IF_FAILED(hr, "[SwapChain] IDXGISwapChain3の取得に失敗しました");

    // Alt+Enterによるフルスクリーン切り替えを無効化
    dxgiFactory2->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

    // バックバッファを取得
    ComPtr<ID3D11Texture2D> backBufferTexture;
    hr = swapChain_->GetBuffer(0, IID_PPV_ARGS(&backBufferTexture));
    THROW_IF_FAILED(hr, "[SwapChain] バックバッファの取得に失敗しました");

    backBuffer_ = WrapBackBuffer(std::move(backBufferTexture));

    // フレーム遅延待機オブジェクトを取得
    if (desc.Flags & DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT) {
        waitableObject_ = swapChain_->GetFrameLatencyWaitableObject();
    }
}

//===========================================================================
// デストラクタ
//===========================================================================
SwapChain::~SwapChain()
{
    LOG_INFO("[SwapChain] 解放開始");

    // 1. バックバッファを解放
    if (backBuffer_) {
        LOG_INFO("[SwapChain] backBuffer解放");
        backBuffer_.reset();
    }

    // 2. スワップチェーンを解放
    if (swapChain_) {
        LOG_INFO("[SwapChain] swapChain解放");
        swapChain_.Reset();
    }

    // 3. ウェイトハンドルを閉じる
    if (waitableObject_) {
        CloseHandle(waitableObject_);
        waitableObject_ = nullptr;
    }

    LOG_INFO("[SwapChain] 解放完了");
}

//===========================================================================
// ムーブコンストラクタ
//===========================================================================
SwapChain::SwapChain(SwapChain&& other) noexcept
    : swapChain_(std::move(other.swapChain_))
    , backBuffer_(std::move(other.backBuffer_))
    , waitableObject_(other.waitableObject_)
{
    other.waitableObject_ = nullptr;
}

//===========================================================================
// ムーブ代入演算子
//===========================================================================
SwapChain& SwapChain::operator=(SwapChain&& other) noexcept
{
    if (this != &other) {
        if (waitableObject_) {
            CloseHandle(waitableObject_);
        }

        swapChain_ = std::move(other.swapChain_);
        backBuffer_ = std::move(other.backBuffer_);
        waitableObject_ = other.waitableObject_;

        other.waitableObject_ = nullptr;
    }
    return *this;
}

//===========================================================================
// 画面表示
//===========================================================================
bool SwapChain::Present(VSyncMode mode)
{
    if (!swapChain_) return false;

    // フレーム遅延待機
    if (waitableObject_) {
        WaitForSingleObjectEx(waitableObject_, 1000, TRUE);
    }

    HRESULT hr = swapChain_->Present(static_cast<UINT>(mode), 0);
    if (FAILED(hr)) {
        if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET) {
            LOG_ERROR("[SwapChain] デバイスが失われました");
        }
        else {
            LOG_HRESULT(hr, "[SwapChain] Presentに失敗しました");
        }
        return false;
    }

    return true;
}

//===========================================================================
// リサイズ
//===========================================================================
bool SwapChain::Resize(uint32_t width, uint32_t height)
{
    if (!swapChain_) return false;
    if (width == 0 || height == 0) return false;

    // バックバッファを解放（リサイズ前に必須）
    backBuffer_.reset();

    // 0とDXGI_FORMAT_UNKNOWNで既存設定を維持
    DXGI_SWAP_CHAIN_DESC1 desc;
    swapChain_->GetDesc1(&desc);

    HRESULT hr = swapChain_->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, desc.Flags);
    RETURN_FALSE_IF_FAILED(hr, "[SwapChain] ResizeBuffersに失敗しました");

    // バックバッファを再取得
    ComPtr<ID3D11Texture2D> backBufferTexture;
    hr = swapChain_->GetBuffer(0, IID_PPV_ARGS(&backBufferTexture));
    RETURN_FALSE_IF_FAILED(hr, "[SwapChain] リサイズ後のバックバッファ取得に失敗しました");

    backBuffer_ = WrapBackBuffer(std::move(backBufferTexture));
    return backBuffer_ != nullptr;
}

//===========================================================================
// フルスクリーン設定
//===========================================================================
bool SwapChain::SetFullscreen(bool fullscreen)
{
    if (!swapChain_) return false;

    HRESULT hr = swapChain_->SetFullscreenState(fullscreen ? TRUE : FALSE, nullptr);
    RETURN_FALSE_IF_FAILED(hr, "[SwapChain] SetFullscreenStateに失敗しました");

    return true;
}

//===========================================================================
// フルスクリーン状態取得
//===========================================================================
bool SwapChain::IsFullscreen() const
{
    if (!swapChain_) return false;

    BOOL fullscreen = FALSE;
    ComPtr<IDXGIOutput> output;
    swapChain_->GetFullscreenState(&fullscreen, &output);

    return fullscreen == TRUE;
}
