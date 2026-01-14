//----------------------------------------------------------------------------
//! @file   circle_renderer.h
//! @brief  円描画クラス（シェーダーベース）
//----------------------------------------------------------------------------
#pragma once

#ifdef _DEBUG

#include "engine/math/math_types.h"
#include "engine/math/color.h"
#include "dx11/gpu/buffer.h"
#include "dx11/gpu/shader.h"
#include "dx11/state/blend_state.h"
#include "dx11/state/sampler_state.h"
#include "dx11/state/rasterizer_state.h"
#include "dx11/state/depth_stencil_state.h"
#include <vector>
#include <memory>
#include <cassert>
#include <wrl/client.h>

struct ID3D11InputLayout;

//----------------------------------------------------------------------------
//! @brief 円描画クラス（デバッグ用）
//----------------------------------------------------------------------------
class CircleRenderer
{
public:
    //! @brief シングルトン取得
    static CircleRenderer& Get()
    {
        assert(instance_ && "CircleRenderer::Create() must be called first");
        return *instance_;
    }

    //! @brief インスタンス生成
    static void Create()
    {
        if (!instance_) {
            instance_ = std::unique_ptr<CircleRenderer>(new CircleRenderer());
        }
    }

    //! @brief インスタンス破棄
    static void Destroy()
    {
        instance_.reset();
    }

    //! @brief デストラクタ
    ~CircleRenderer() = default;

    bool Initialize();
    void Shutdown();

    //! @brief 塗りつぶし円を描画
    void DrawFilled(
        const Vector2& center,
        float radius,
        const Color& color
    );

    //! @brief バッチ描画開始
    void Begin(const class Camera2D& camera);

    //! @brief バッチ描画終了（実際に描画）
    void End();

private:
    CircleRenderer() = default;
    CircleRenderer(const CircleRenderer&) = delete;
    CircleRenderer& operator=(const CircleRenderer&) = delete;

    static inline std::unique_ptr<CircleRenderer> instance_ = nullptr;

    struct CircleVertex {
        Vector3 position;
        Vector2 texCoord;
        Color color;
    };

    struct CircleInstance {
        Vector2 center;
        float radius;
        Color color;
    };

    // GPUリソース
    ShaderPtr vertexShader_;
    ShaderPtr pixelShader_;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout_;
    BufferPtr vertexBuffer_;
    BufferPtr indexBuffer_;
    BufferPtr constantBuffer_;

    // パイプラインステート
    std::unique_ptr<BlendState> blendState_;
    std::unique_ptr<SamplerState> samplerState_;
    std::unique_ptr<RasterizerState> rasterizerState_;
    std::unique_ptr<DepthStencilState> depthStencilState_;

    std::vector<CircleInstance> instances_;
    bool isBegun_ = false;
    bool initialized_ = false;

    Matrix constantData_{};
};

#endif // _DEBUG
