//----------------------------------------------------------------------------
//! @file   shadow_map.cpp
//! @brief  シャドウマップ管理クラス実装
//----------------------------------------------------------------------------

#include "shadow_map.h"
#include "dx11/graphics_context.h"
#include "common/logging/logging.h"

//============================================================================
// ファクトリ
//============================================================================

std::unique_ptr<ShadowMap> ShadowMap::Create(const ShadowMapSettings& settings)
{
    auto shadowMap = std::unique_ptr<ShadowMap>(new ShadowMap());
    shadowMap->settings_ = settings;

    // シャドウマップ用デプステクスチャ作成
    // DXGI_FORMAT_R32_TYPELESS を使ってDSVとSRV両方に対応
    shadowMap->depthTexture_ = Texture::CreateDepthStencil(
        settings.resolution,
        settings.resolution,
        DXGI_FORMAT_D32_FLOAT,
        true  // withSrv = true
    );

    if (!shadowMap->depthTexture_) {
        LOG_ERROR("[ShadowMap] デプステクスチャ作成失敗");
        return nullptr;
    }

    // 初期行列
    shadowMap->viewMatrix_ = Matrix::Identity;
    shadowMap->projectionMatrix_ = Matrix::Identity;

    LOG_INFO("[ShadowMap] 作成完了 (" + std::to_string(settings.resolution) + "x" + std::to_string(settings.resolution) + ")");
    return shadowMap;
}

//============================================================================
// ライト設定
//============================================================================

void ShadowMap::SetDirectionalLight(const Vector3& lightDir, const Vector3& sceneCenter)
{
    // ライト方向を正規化
    Vector3 dir = lightDir;
    dir.Normalize();

    // ライト位置をシーン中心から遠ざける
    float distance = settings_.orthoSize * 2.0f;
    Vector3 lightPos = sceneCenter - dir * distance;

    // アップベクトル計算（ライト方向がY軸に近い場合はZ軸を使用）
    Vector3 up = Vector3::Up;
    if (std::abs(dir.Dot(Vector3::Up)) > 0.99f) {
        up = Vector3::Forward;
    }

    // ビュー行列を作成
    viewMatrix_ = Matrix::CreateLookAt(lightPos, sceneCenter, up);

    // 正射影行列を作成（ディレクショナルライト用）
    float halfSize = settings_.orthoSize;
    projectionMatrix_ = Matrix::CreateOrthographic(
        halfSize * 2.0f,
        halfSize * 2.0f,
        settings_.nearPlane,
        settings_.farPlane
    );
}

//============================================================================
// レンダリング
//============================================================================

void ShadowMap::BeginShadowPass()
{
    auto& ctx = GraphicsContext::Get();
    auto* d3dCtx = ctx.GetContext();
    if (!d3dCtx) {
        return;
    }

    // 現在のレンダーターゲットを保存
    d3dCtx->OMGetRenderTargets(1, &prevRtv_, &prevDsv_);

    // シャドウマップをレンダーターゲットに設定（カラーなし、デプスのみ）
    ID3D11RenderTargetView* nullRtv = nullptr;
    d3dCtx->OMSetRenderTargets(1, &nullRtv, depthTexture_->Dsv());

    // ビューポート設定
    D3D11_VIEWPORT vp = {};
    vp.Width = static_cast<float>(settings_.resolution);
    vp.Height = static_cast<float>(settings_.resolution);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    d3dCtx->RSSetViewports(1, &vp);

    // デプスバッファをクリア
    d3dCtx->ClearDepthStencilView(depthTexture_->Dsv(), D3D11_CLEAR_DEPTH, 1.0f, 0);
}

void ShadowMap::EndShadowPass()
{
    auto& ctx = GraphicsContext::Get();
    auto* d3dCtx = ctx.GetContext();
    if (!d3dCtx) {
        return;
    }

    // レンダーターゲットを復元
    d3dCtx->OMSetRenderTargets(1, &prevRtv_, prevDsv_);

    // 参照解放
    if (prevRtv_) {
        prevRtv_->Release();
        prevRtv_ = nullptr;
    }
    if (prevDsv_) {
        prevDsv_->Release();
        prevDsv_ = nullptr;
    }
}
