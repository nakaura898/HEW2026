//----------------------------------------------------------------------------
//! @file   sprite_batch.h
//! @brief  スプライトバッチ描画システム
//----------------------------------------------------------------------------
#pragma once

#include "dx11/gpu_common.h"
#include "dx11/gpu/buffer.h"
#include "dx11/gpu/shader.h"
#include "dx11/gpu/texture.h"
#include "dx11/state/blend_state.h"
#include "dx11/state/sampler_state.h"
#include "dx11/state/rasterizer_state.h"
#include "dx11/state/depth_stencil_state.h"
#include "engine/scene/math_types.h"
#include "engine/color.h"
#include "engine/component/sprite_renderer.h"
#include "engine/component/animator.h"
#include <vector>

// 前方宣言
class Transform2D;
class Camera2D;

//============================================================================
//! @brief スプライトバッチ描画システム
//!
//! 2Dスプライトを効率的にバッチ描画するシステム。
//! 同じテクスチャのスプライトをまとめて描画することで描画コールを削減。
//============================================================================
#pragma warning(push)
#pragma warning(disable: 4324) // structure was padded due to alignment specifier
class SpriteBatch final : private NonCopyableNonMovable {
public:
    //! @brief 1回のバッチで描画できる最大スプライト数
    static constexpr uint32_t MaxSpritesPerBatch = 2048;

    //------------------------------------------------------------------------
    //! @brief シングルトンインスタンス取得
    //------------------------------------------------------------------------
    static SpriteBatch& Get() noexcept {
        static SpriteBatch instance;
        return instance;
    }

    //------------------------------------------------------------------------
    //! @brief 初期化
    //! @return 成功した場合はtrue
    //------------------------------------------------------------------------
    bool Initialize();

    //------------------------------------------------------------------------
    //! @brief 終了処理
    //------------------------------------------------------------------------
    void Shutdown();

    //------------------------------------------------------------------------
    //! @brief カメラを設定
    //! @param camera 2Dカメラ
    //------------------------------------------------------------------------
    void SetCamera(Camera2D& camera);

    //------------------------------------------------------------------------
    //! @brief ビュープロジェクション行列を直接設定
    //! @param viewProjection ビュープロジェクション行列（転置済み）
    //------------------------------------------------------------------------
    void SetViewProjection(const Matrix& viewProjection);

    //------------------------------------------------------------------------
    //! @brief バッチ描画開始
    //------------------------------------------------------------------------
    void Begin();

    //------------------------------------------------------------------------
    //! @brief スプライトを描画キューに追加
    //------------------------------------------------------------------------
    void Draw(
        Texture* texture,
        const Vector2& position,
        const Color& color = Colors::White,
        float rotation = 0.0f,
        const Vector2& origin = Vector2::Zero,
        const Vector2& scale = Vector2::One,
        bool flipX = false,
        bool flipY = false,
        int sortingLayer = 0,
        int orderInLayer = 0
    );

    //------------------------------------------------------------------------
    //! @brief SpriteRendererコンポーネントから描画
    //------------------------------------------------------------------------
    void Draw(const SpriteRenderer& renderer, const Transform2D& transform);

    //------------------------------------------------------------------------
    //! @brief SpriteRenderer + Animator から描画（スプライトシートアニメーション）
    //------------------------------------------------------------------------
    void Draw(const SpriteRenderer& renderer, const Transform2D& transform, const Animator& animator);

    //------------------------------------------------------------------------
    //! @brief バッチ描画終了（実際の描画を実行）
    //------------------------------------------------------------------------
    void End();

    //------------------------------------------------------------------------
    //! @brief 描画統計を取得
    //------------------------------------------------------------------------
    [[nodiscard]] uint32_t GetDrawCallCount() const noexcept { return drawCallCount_; }
    [[nodiscard]] uint32_t GetSpriteCount() const noexcept { return spriteCount_; }

private:
    SpriteBatch() = default;
    ~SpriteBatch() = default;

    //! @brief スプライト頂点データ
    struct SpriteVertex {
        Vector3 position;
        Vector2 texCoord;
        Color color;
    };

    //! @brief スプライト情報（ソート用）
    struct SpriteInfo {
        Texture* texture;
        SpriteVertex vertices[4];
        int sortingLayer;
        int orderInLayer;
    };

    bool CreateShaders();
    void FlushBatch();
    void SortSprites();

    // GPUリソース（dx11/gpu/）
    BufferPtr vertexBuffer_;
    BufferPtr indexBuffer_;
    BufferPtr constantBuffer_;

    // シェーダー（dx11/gpu/）
    ShaderPtr vertexShader_;
    ShaderPtr pixelShader_;
    ComPtr<ID3D11InputLayout> inputLayout_;

    // パイプラインステート（dx11/state/）
    std::unique_ptr<BlendState> blendState_;
    std::unique_ptr<SamplerState> samplerState_;
    std::unique_ptr<RasterizerState> rasterizerState_;
    std::unique_ptr<DepthStencilState> depthStencilState_;

    // スプライトキュー
    std::vector<SpriteInfo> spriteQueue_;
    std::vector<uint32_t> sortIndices_;  //!< ソート用インデックス配列

    // 定数バッファデータ
    struct alignas(16) CBufferData {
        Matrix viewProjection;
    };
    CBufferData cbufferData_;

    // 状態
    bool isBegun_ = false;
    bool initialized_ = false;

    // 統計
    uint32_t drawCallCount_ = 0;
    uint32_t spriteCount_ = 0;
};
#pragma warning(pop)
