//----------------------------------------------------------------------------
//! @file   shadow_map.h
//! @brief  シャドウマップ管理クラス
//----------------------------------------------------------------------------
#pragma once

#include "engine/math/math_types.h"
#include "dx11/gpu/texture.h"
#include <memory>
#include <cstdint>

//============================================================================
//! @brief シャドウマップ設定
//============================================================================
struct ShadowMapSettings
{
    uint32_t resolution = 2048;     //!< シャドウマップ解像度
    float nearPlane = 0.1f;         //!< ニアクリップ面
    float farPlane = 100.0f;        //!< ファークリップ面
    float orthoSize = 50.0f;        //!< 正射影サイズ（ディレクショナルライト用）
    float depthBias = 0.005f;       //!< 深度バイアス
    float normalBias = 0.01f;       //!< 法線バイアス
};

//============================================================================
//! @brief シャドウマップクラス
//!
//! @details ディレクショナルライト用のカスケードシャドウマップを管理
//!          シングルシャドウマップ（1ライト分）を提供
//============================================================================
class ShadowMap final
{
public:
    //! @brief シャドウマップ生成
    //! @param settings シャドウマップ設定
    //! @return 生成されたシャドウマップ（失敗時nullptr）
    [[nodiscard]] static std::unique_ptr<ShadowMap> Create(const ShadowMapSettings& settings = {});

    ~ShadowMap() = default;

    //----------------------------------------------------------
    //! @name ライト設定
    //----------------------------------------------------------
    //!@{

    //! @brief ディレクショナルライトのシャドウを設定
    //! @param lightDir ライト方向（正規化済み）
    //! @param sceneCenter シーン中心座標
    void SetDirectionalLight(const Vector3& lightDir, const Vector3& sceneCenter);

    //! @brief ビュー行列を取得
    [[nodiscard]] const Matrix& GetViewMatrix() const noexcept { return viewMatrix_; }

    //! @brief 投影行列を取得
    [[nodiscard]] const Matrix& GetProjectionMatrix() const noexcept { return projectionMatrix_; }

    //! @brief ビュー×投影行列を取得
    [[nodiscard]] Matrix GetViewProjectionMatrix() const noexcept {
        return viewMatrix_ * projectionMatrix_;
    }

    //!@}
    //----------------------------------------------------------
    //! @name レンダリング
    //----------------------------------------------------------
    //!@{

    //! @brief シャドウパス開始（デプスバッファをクリアしてバインド）
    void BeginShadowPass();

    //! @brief シャドウパス終了（バインド解除）
    void EndShadowPass();

    //!@}
    //----------------------------------------------------------
    //! @name アクセサ
    //----------------------------------------------------------
    //!@{

    //! @brief デプステクスチャ取得（SRV用）
    [[nodiscard]] Texture* GetDepthTexture() const noexcept { return depthTexture_.get(); }

    //! @brief シャドウマップ解像度取得
    [[nodiscard]] uint32_t GetResolution() const noexcept { return settings_.resolution; }

    //! @brief 深度バイアス取得
    [[nodiscard]] float GetDepthBias() const noexcept { return settings_.depthBias; }

    //! @brief 法線バイアス取得
    [[nodiscard]] float GetNormalBias() const noexcept { return settings_.normalBias; }

    //! @brief 設定取得
    [[nodiscard]] const ShadowMapSettings& GetSettings() const noexcept { return settings_; }

    //!@}

private:
    ShadowMap() = default;

    ShadowMapSettings settings_;
    TexturePtr depthTexture_;           //!< デプステクスチャ

    Matrix viewMatrix_;                  //!< ライトビュー行列
    Matrix projectionMatrix_;            //!< ライト投影行列

    // 前のレンダーターゲット保存用
    ID3D11RenderTargetView* prevRtv_ = nullptr;
    ID3D11DepthStencilView* prevDsv_ = nullptr;
};
