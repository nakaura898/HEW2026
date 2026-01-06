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

    return true;
}

//----------------------------------------------------------------------------
void GraphicsDevice::Shutdown() noexcept
{
    // GraphicsContextは既にApplication::Shutdown()で解放済み
    // ここでは冗長な呼び出しは行わない

    device_.Reset();

#ifdef _DEBUG
    // Note: デバイス解放後はライブオブジェクトレポートは不可
    // リソースリークの検出はDXGI Debug Layer経由で行う
    LOG_INFO("[GraphicsDevice] デバイス解放完了");
#endif
}

//----------------------------------------------------------------------------
ID3D11Device5* GetD3D11Device() noexcept
{
    return GraphicsDevice::Get().Device();
}
