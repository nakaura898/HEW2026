//----------------------------------------------------------------------------
//! @file   graphics_device.cpp
//! @brief  D3D11デバイスマネージャー実装
//----------------------------------------------------------------------------
#include "dx11/graphics_device.h"
#include "dx11/graphics_context.h"
#include "common/logging/logging.h"

//----------------------------------------------------------------------------
GraphicsDevice& GraphicsDevice::Get() noexcept
{
    static GraphicsDevice instance;
    return instance;
}

//----------------------------------------------------------------------------
bool GraphicsDevice::Initialize(bool enableDebug)
{
    UINT createFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    if (enableDebug) {
        createFlags |= D3D11_CREATE_DEVICE_DEBUG;
    }

    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_1,
        D3D_FEATURE_LEVEL_11_0,
    };

    Microsoft::WRL::ComPtr<ID3D11Device> device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL featureLevel;

    HRESULT hr = D3D11CreateDevice(
        nullptr,                    // アダプタ（nullでデフォルト）
        D3D_DRIVER_TYPE_HARDWARE,   // ハードウェアアクセラレーション
        nullptr,                    // ソフトウェアラスタライザ
        createFlags,
        featureLevels,
        _countof(featureLevels),
        D3D11_SDK_VERSION,
        &device,
        &featureLevel,
        &context
    );
    RETURN_FALSE_IF_FAILED(hr, "[GraphicsDevice] D3D11CreateDeviceに失敗しました");

    // ID3D11Device5 にアップグレード
    hr = device.As(&device_);
    RETURN_FALSE_IF_FAILED(hr, "[GraphicsDevice] ID3D11Device5へのアップグレードに失敗しました");

    // GraphicsContext を初期化
  //  if (!GraphicsContext::Get().Initialize()) {
  //      device_.Reset();
  //      RETURN_FALSE_IF_FALSE(false, "[GraphicsDevice] GraphicsContextの初期化に失敗しました");
  //  }

    return true;
}

//----------------------------------------------------------------------------
void GraphicsDevice::Shutdown() noexcept
{
    // GraphicsContextは既にApplication::Shutdown()で解放済み
    // ここでは冗長な呼び出しは行わない

#ifdef _DEBUG
    // デバッグビルド時、解放前にライブオブジェクトをレポート
    if (device_) {
        ComPtr<ID3D11Debug> debug;
        if (SUCCEEDED(device_.As(&debug))) {
            LOG_INFO("[GraphicsDevice] ライブオブジェクトレポート:");
            // RLDO_IGNORE_INTERNAL: デバッグレイヤーの内部オブジェクトを除外
            debug->ReportLiveDeviceObjects(
                static_cast<D3D11_RLDO_FLAGS>(D3D11_RLDO_DETAIL | D3D11_RLDO_IGNORE_INTERNAL));
        }
    }
#endif

    device_.Reset();
}

//----------------------------------------------------------------------------
ID3D11Device5* GetD3D11Device() noexcept
{
    return GraphicsDevice::Get().Device();
}
