//----------------------------------------------------------------------------
//! @file   stage_background.h
//! @brief  ステージ背景 - レイヤー分けされた背景描画
//----------------------------------------------------------------------------
#pragma once

#include "dx11/gpu/texture.h"
#include "dx11/gpu/shader.h"
#include "engine/math/math_types.h"
#include "engine/math/color.h"
#include <vector>
#include <string>
#include <random>

// 前方宣言
class SpriteBatch;
class Camera2D;

//----------------------------------------------------------------------------
// チャンク設定
//----------------------------------------------------------------------------
constexpr float kChunkSize = 1024.0f;   //!< チャンクサイズ（ピクセル）
constexpr int kChunksX = 5;             //!< X方向チャンク数 (5120/1024)
constexpr int kChunksY = 3;             //!< Y方向チャンク数 (2880/1024、切り上げ)

//----------------------------------------------------------------------------
//! @brief ステージ背景クラス
//! @details レイヤー分けされた背景を描画する
//!          - 背景(haikei): sortingLayer -200
//!          - 地面(ground): sortingLayer -150
//!          - 装飾(奥): sortingLayer -120
//!          - 装飾(中): sortingLayer -100
//!          - 装飾(手前): sortingLayer -80
//----------------------------------------------------------------------------
class StageBackground
{
public:
    //! @brief ステージ背景を初期化
    //! @param stageId ステージID (例: "stage1")
    //! @param screenWidth 画面幅
    //! @param screenHeight 画面高さ
    void Initialize(const std::string& stageId, float screenWidth, float screenHeight);

    //! @brief 背景を描画
    //! @param spriteBatch スプライトバッチ
    //! @param camera カメラ（可視チャンク判定用）
    void Render(SpriteBatch& spriteBatch, const Camera2D& camera);

    //! @brief リソース解放
    void Shutdown();

private:
    //! @brief 地面タイル（回転/反転付き）
    struct GroundTile
    {
        Vector2 position;           //!< 位置
        float rotation;             //!< 回転（0, 90, 180, 270度）
        bool flipX;                 //!< X反転
        bool flipY;                 //!< Y反転
        float alpha;                //!< アルファ値（2層目用）
    };

    //! @brief 地面チャンク（分割描画用）
    struct GroundChunk
    {
        TexturePtr texture;         //!< チャンクテクスチャ (1024x1024)
        Vector2 position;           //!< チャンク左上のワールド座標
    };

    //! @brief 装飾オブジェクト
    struct DecorationObject
    {
        TexturePtr texture;         //!< テクスチャ
        Vector2 position;           //!< 位置
        Vector2 scale;              //!< スケール
        float rotation;             //!< 回転（ラジアン）
        int sortingLayer;           //!< ソートレイヤー
    };

    //! @brief 装飾をランダム配置
    //! @param stageId ステージID
    //! @param screenWidth 画面幅
    //! @param screenHeight 画面高さ
    void PlaceDecorations(const std::string& stageId, float screenWidth, float screenHeight);

    //! @brief 装飾を追加
    //! @param texture テクスチャ
    //! @param position 位置
    //! @param sortingLayer ソートレイヤー
    //! @param scale スケール（デフォルト1.0）
    //! @param rotation 回転（デフォルト0.0）
    void AddDecoration(TexturePtr texture, const Vector2& position, int sortingLayer,
                       const Vector2& scale = Vector2::One, float rotation = 0.0f);

    //! @brief 地面テクスチャをプリベイク
    void BakeGroundTexture();

    //! @brief ベイク済みテクスチャをチャンクに分割
    void SplitIntoChunks();

    // 地面テクスチャ（タイル用、ベイク時のみ使用）
    TexturePtr groundTexture_;

    // ベース地面テクスチャ（敷き詰め用）
    TexturePtr baseGroundTexture_;
    float baseGroundWidth_ = 0.0f;
    float baseGroundHeight_ = 0.0f;

    // 地面チャンク配列（分割描画用）
    std::vector<GroundChunk> chunks_;

    // ベイク済み地面テクスチャ（一時使用、分割後に解放）
    TexturePtr bakedGroundTexture_;

    // 地面タイル用シェーダー（端フェード付き）
    ShaderPtr groundVertexShader_;
    ShaderPtr groundPixelShader_;

    // 正規化シェーダー（2パス目）
    ShaderPtr normalizePixelShader_;

    // 蓄積用レンダーターゲット（RGBA16F、ベイク時のみ使用）
    TexturePtr accumulationRT_;

    // 地面タイル（回転/反転付き）
    std::vector<GroundTile> groundTiles_;

    // タイル描画サイズ
    float tileWidth_ = 0.0f;
    float tileHeight_ = 0.0f;

    // ステージサイズ
    float stageWidth_ = 0.0f;
    float stageHeight_ = 0.0f;

    // 装飾オブジェクト
    std::vector<DecorationObject> decorations_;

    // 乱数生成器
    std::mt19937 rng_;

    // 画面サイズ
    float screenWidth_ = 0.0f;
    float screenHeight_ = 0.0f;
};