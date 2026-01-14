//----------------------------------------------------------------------------
//! @file   render_state_manager.h
//! @brief  レンダーステートマネージャー
//----------------------------------------------------------------------------
#pragma once

#include "dx11/gpu_common.h"
#include "dx11/state/blend_state.h"
#include "dx11/state/sampler_state.h"
#include "dx11/state/rasterizer_state.h"
#include "dx11/state/depth_stencil_state.h"
#include <memory>
#include <cassert>

//===========================================================================
//! レンダーステートマネージャー（シングルトン）
//!
//! 共通パイプラインステートを一元管理する。
//!
//! @note 使用例:
//! @code
//!   // 初期化
//!   RenderStateManager::Get().Initialize();
//!
//!   // ステート取得
//!   auto* blend = RenderStateManager::Get().GetAlphaBlend();
//!   auto* sampler = RenderStateManager::Get().GetLinearWrap();
//!
//!   // 終了
//!   RenderStateManager::Get().Shutdown();
//! @endcode
//===========================================================================
class RenderStateManager final : private NonCopyableNonMovable
{
public:
    //! シングルトンインスタンス取得
    static RenderStateManager& Get()
    {
        assert(instance_ && "RenderStateManager::Create() must be called first");
        return *instance_;
    }

    //! インスタンス生成
    static void Create();

    //! インスタンス破棄
    static void Destroy();

    //! デストラクタ
    ~RenderStateManager() = default;

    //----------------------------------------------------------
    //! @name   初期化・終了
    //----------------------------------------------------------
    //!@{

    //! 初期化
    //! @return 成功時true
    bool Initialize();

    //! 終了
    void Shutdown();

    //! 初期化済みかどうか
    [[nodiscard]] bool IsInitialized() const noexcept { return initialized_; }

    //!@}
    //----------------------------------------------------------
    //! @name   ブレンドステート
    //----------------------------------------------------------
    //!@{

    //! 不透明（ブレンド無効）
    [[nodiscard]] BlendState* GetOpaque() const noexcept { return opaque_.get(); }

    //! アルファブレンド（半透明）
    [[nodiscard]] BlendState* GetAlphaBlend() const noexcept { return alphaBlend_.get(); }

    //! 加算ブレンド（SrcAlpha + One）
    [[nodiscard]] BlendState* GetAdditive() const noexcept { return additive_.get(); }

    //! 純粋加算ブレンド（One + One）
    [[nodiscard]] BlendState* GetPureAdditive() const noexcept { return pureAdditive_.get(); }

    //! プリマルチプライドアルファ
    [[nodiscard]] BlendState* GetPremultipliedAlpha() const noexcept { return premultipliedAlpha_.get(); }

    //!@}
    //----------------------------------------------------------
    //! @name   サンプラーステート
    //----------------------------------------------------------
    //!@{

    //! リニアフィルタ + ラップ
    [[nodiscard]] SamplerState* GetLinearWrap() const noexcept { return linearWrap_.get(); }

    //! リニアフィルタ + クランプ
    [[nodiscard]] SamplerState* GetLinearClamp() const noexcept { return linearClamp_.get(); }

    //! ポイントフィルタ + ラップ
    [[nodiscard]] SamplerState* GetPointWrap() const noexcept { return pointWrap_.get(); }

    //! ポイントフィルタ + クランプ
    [[nodiscard]] SamplerState* GetPointClamp() const noexcept { return pointClamp_.get(); }

    //!@}
    //----------------------------------------------------------
    //! @name   ラスタライザーステート
    //----------------------------------------------------------
    //!@{

    //! デフォルト（ソリッド、バックフェイスカリング）
    [[nodiscard]] RasterizerState* GetDefault() const noexcept { return rasterizerDefault_.get(); }

    //! カリング無し（両面描画）
    [[nodiscard]] RasterizerState* GetNoCull() const noexcept { return noCull_.get(); }

    //! ワイヤーフレーム
    [[nodiscard]] RasterizerState* GetWireframe() const noexcept { return wireframe_.get(); }

    //!@}
    //----------------------------------------------------------
    //! @name   深度ステンシルステート
    //----------------------------------------------------------
    //!@{

    //! 深度テスト有効・書き込み有効
    [[nodiscard]] DepthStencilState* GetDepthDefault() const noexcept { return depthDefault_.get(); }

    //! 深度テスト有効・書き込み無効（読み取りのみ）
    [[nodiscard]] DepthStencilState* GetDepthReadOnly() const noexcept { return depthReadOnly_.get(); }

    //! 深度テスト無効
    [[nodiscard]] DepthStencilState* GetDepthDisabled() const noexcept { return depthDisabled_.get(); }

    //! 深度テスト有効（LessEqual）・書き込み有効
    [[nodiscard]] DepthStencilState* GetDepthLessEqual() const noexcept { return depthLessEqual_.get(); }

    //!@}

private:
    RenderStateManager() = default;
    RenderStateManager(const RenderStateManager&) = delete;
    RenderStateManager& operator=(const RenderStateManager&) = delete;

    static inline std::unique_ptr<RenderStateManager> instance_ = nullptr;

    bool initialized_ = false;

    // ブレンドステート
    std::unique_ptr<BlendState> opaque_;
    std::unique_ptr<BlendState> alphaBlend_;
    std::unique_ptr<BlendState> additive_;
    std::unique_ptr<BlendState> pureAdditive_;
    std::unique_ptr<BlendState> premultipliedAlpha_;

    // サンプラーステート
    std::unique_ptr<SamplerState> linearWrap_;
    std::unique_ptr<SamplerState> linearClamp_;
    std::unique_ptr<SamplerState> pointWrap_;
    std::unique_ptr<SamplerState> pointClamp_;

    // ラスタライザーステート
    std::unique_ptr<RasterizerState> rasterizerDefault_;
    std::unique_ptr<RasterizerState> noCull_;
    std::unique_ptr<RasterizerState> wireframe_;

    // 深度ステンシルステート
    std::unique_ptr<DepthStencilState> depthDefault_;
    std::unique_ptr<DepthStencilState> depthReadOnly_;
    std::unique_ptr<DepthStencilState> depthDisabled_;
    std::unique_ptr<DepthStencilState> depthLessEqual_;
};
