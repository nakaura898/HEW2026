//----------------------------------------------------------------------------
//! @file   mesh_batch.h
//! @brief  メッシュバッチレンダラー
//----------------------------------------------------------------------------
#pragma once

#include "engine/mesh/mesh_handle.h"
#include "engine/material/material_handle.h"
#include "engine/lighting/light.h"
#include "engine/math/math_types.h"
#include "dx11/gpu/buffer.h"
#include "dx11/gpu/shader.h"
#include <wrl/client.h>
#include <d3d11.h>
#include <memory>
#include <vector>

// 前方宣言
class Camera3D;
class MeshRenderer;
class Transform;
class ShadowMap;

//============================================================================
//! @brief メッシュバッチレンダラー（シングルトン）
//!
//! 3Dメッシュのバッチ描画を担当。
//! SpriteBatchと同様のBegin/Draw/Endパターンを使用。
//!
//! @code
//!   MeshBatch::Get().SetCamera(*camera3d);
//!   MeshBatch::Get().Begin();
//!   MeshBatch::Get().Draw(meshHandle, materialHandle, worldMatrix);
//!   MeshBatch::Get().End();
//! @endcode
//============================================================================
class MeshBatch final
{
public:
    //! シングルトンインスタンス取得
    [[nodiscard]] static MeshBatch& Get();

    //! インスタンス生成
    static void Create();

    //! インスタンス破棄
    static void Destroy();

    //! デストラクタ
    ~MeshBatch();

    //----------------------------------------------------------
    //! @name 初期化・終了
    //----------------------------------------------------------
    //!@{

    //! @brief 初期化
    //! @return 成功時true
    [[nodiscard]] bool Initialize();

    //! @brief 終了処理
    void Shutdown();

    //!@}
    //----------------------------------------------------------
    //! @name カメラ設定
    //----------------------------------------------------------
    //!@{

    //! @brief カメラを設定（Camera3Dから）
    void SetCamera(const Camera3D& camera);

    //! @brief ビュー・投影行列を直接設定
    void SetViewProjection(const Matrix& view, const Matrix& projection);

    //!@}
    //----------------------------------------------------------
    //! @name ライティング設定
    //----------------------------------------------------------
    //!@{

    //! @brief 環境光を設定
    void SetAmbientLight(const Color& color);

    //! @brief ライトを追加
    //! @param light ライトデータ
    //! @return 追加に成功した場合true（最大8個）
    bool AddLight(const LightData& light);

    //! @brief ライトをクリア
    void ClearLights();

    //!@}
    //----------------------------------------------------------
    //! @name シャドウ設定
    //----------------------------------------------------------
    //!@{

    //! @brief シャドウマップを設定
    //! @param shadowMap シャドウマップ（nullptrでシャドウ無効）
    void SetShadowMap(ShadowMap* shadowMap);

    //! @brief シャドウ有効/無効を設定
    void SetShadowEnabled(bool enabled) { shadowEnabled_ = enabled; }

    //! @brief シャドウ強度を設定
    //! @param strength 0.0〜1.0（0=影なし、1=完全な影）
    void SetShadowStrength(float strength) { shadowStrength_ = strength; }

    //!@}
    //----------------------------------------------------------
    //! @name 描画
    //----------------------------------------------------------
    //!@{

    //! @brief バッチ開始
    void Begin();

    //! @brief メッシュを描画キューに追加
    //! @param mesh メッシュハンドル
    //! @param material マテリアルハンドル
    //! @param world ワールド変換行列
    void Draw(MeshHandle mesh, MaterialHandle material, const Matrix& world);

    //! @brief MeshRendererから描画（Transform使用）
    //! @param renderer MeshRendererコンポーネント
    //! @param transform Transformコンポーネント
    void Draw(const MeshRenderer& renderer, Transform& transform);

    //! @brief バッチ終了・フラッシュ
    void End();

    //! @brief シャドウパスのみ描画
    //! @note Begin/Draw後、End前に呼び出す
    void RenderShadowPass();

    //!@}
    //----------------------------------------------------------
    //! @name 統計
    //----------------------------------------------------------
    //!@{

    //! @brief ドローコール数を取得
    [[nodiscard]] uint32_t GetDrawCallCount() const noexcept { return drawCallCount_; }

    //! @brief 描画メッシュ数を取得
    [[nodiscard]] uint32_t GetMeshCount() const noexcept { return meshCount_; }

    //!@}

private:
    MeshBatch() = default;
    MeshBatch(const MeshBatch&) = delete;
    MeshBatch& operator=(const MeshBatch&) = delete;

    static inline std::unique_ptr<MeshBatch> instance_ = nullptr;

    //------------------------------------------------------------------------
    // 内部構造体
    //------------------------------------------------------------------------

    //! @brief 描画コマンド
    struct DrawCommand
    {
        MeshHandle mesh;
        MaterialHandle material;
        uint32_t subMeshIndex;
        Matrix worldMatrix;
        float distanceToCamera;  // ソート用
    };

    //! @brief フレーム定数バッファ
    struct alignas(16) PerFrameConstants
    {
        Matrix viewProjection;     // 64 bytes
        Vector4 cameraPosition;    // 16 bytes
    };  // 80 bytes

    //! @brief オブジェクト定数バッファ
    struct alignas(16) PerObjectConstants
    {
        Matrix world;              // 64 bytes
        Matrix worldInvTranspose;  // 64 bytes
    };  // 128 bytes

    //! @brief シャドウパス定数バッファ
    struct alignas(16) ShadowPassConstants
    {
        Matrix lightViewProjection;  // 64 bytes
    };  // 64 bytes

    //! @brief シャドウ定数バッファ（ピクセルシェーダー用）
    struct alignas(16) ShadowConstants
    {
        Matrix lightViewProjection;  // 64 bytes
        Vector4 shadowParams;        // 16 bytes (depthBias, normalBias, strength, enabled)
    };  // 80 bytes

    //------------------------------------------------------------------------
    // 内部メソッド
    //------------------------------------------------------------------------

    bool CreateShaders();
    bool CreateConstantBuffers();
    void SortDrawCommands();
    void FlushBatch();
    void BindMaterialTextures(class Material* mat);
    void RenderMesh(const DrawCommand& cmd);
    void RenderMeshShadow(const DrawCommand& cmd);

    //------------------------------------------------------------------------
    // メンバ変数
    //------------------------------------------------------------------------

    bool initialized_ = false;
    bool isBegun_ = false;

    // シェーダー
    ShaderPtr vertexShader_;
    ShaderPtr pixelShader_;
    ShaderPtr shadowVertexShader_;
    ShaderPtr shadowPixelShader_;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout_;

    // 定数バッファ
    BufferPtr perFrameBuffer_;      // b0
    BufferPtr perObjectBuffer_;     // b1
    BufferPtr lightingBuffer_;      // b3
    BufferPtr shadowBuffer_;        // b4
    BufferPtr shadowPassBuffer_;    // シャドウパス用b0

    // カメラ情報
    Matrix viewMatrix_;
    Matrix projectionMatrix_;
    Vector3 cameraPosition_;

    // ライティング
    LightingConstants lightingConstants_;

    // シャドウ
    ShadowMap* shadowMap_ = nullptr;
    bool shadowEnabled_ = true;
    float shadowStrength_ = 1.0f;

    // 描画キュー
    std::vector<DrawCommand> drawQueue_;

    // 統計
    uint32_t drawCallCount_ = 0;
    uint32_t meshCount_ = 0;
};
