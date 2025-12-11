//----------------------------------------------------------------------------
//! @file   animator.h
//! @brief  スプライトシートアニメーションコンポーネント
//----------------------------------------------------------------------------
#pragma once

#include "component.h"
#include "engine/scene/math_types.h"
#include <cstdint>
#include <array>

//============================================================================
//! @brief スプライトシートアニメーションコンポーネント
//!
//! スプライトシートを行（アニメーションの種類）と列（フレーム）で管理し、
//! 時間経過で自動的にフレームを進める。
//! SpriteRendererと組み合わせて使用する。
//============================================================================
class Animator : public Component {
public:
    //! @brief 行ごとの最大列数を格納できる最大行数
    static constexpr uint32_t kMaxRows = 16;

    //! @brief 想定フレームレート（SetFrameDuration用）
    static constexpr float kAssumedFrameRate = 60.0f;

    //------------------------------------------------------------------------
    //! @brief コンストラクタ
    //! @param rows シート縦方向の分割数（アニメーションの種類数）
    //! @param cols シート横方向の分割数（最大フレーム数）
    //! @param frameInterval フレーム間隔（ゲームフレーム数）
    //------------------------------------------------------------------------
    Animator(uint32_t rows = 1, uint32_t cols = 1, uint32_t frameInterval = 1);

    //------------------------------------------------------------------------
    // Component オーバーライド
    //------------------------------------------------------------------------

    void Update(float deltaTime) override;

    //------------------------------------------------------------------------
    // 再生制御
    //------------------------------------------------------------------------

    //! @brief アニメーションを先頭からリセット
    void Reset();

    //! @brief 再生/一時停止
    void SetPlaying(bool playing) noexcept { playing_ = playing; }
    [[nodiscard]] bool IsPlaying() const noexcept { return playing_; }

    //! @brief ループ再生の設定
    void SetLooping(bool loop) noexcept { looping_ = loop; }
    [[nodiscard]] bool IsLooping() const noexcept { return looping_; }

    //------------------------------------------------------------------------
    // フレーム間隔
    //------------------------------------------------------------------------

    //! @brief フレーム間隔を設定（ゲームフレーム数）
    void SetFrameInterval(uint32_t frames) noexcept { frameInterval_ = frames > 0 ? frames : 1; }
    [[nodiscard]] uint32_t GetFrameInterval() const noexcept { return frameInterval_; }

    //! @brief フレーム間隔を秒で設定（kAssumedFrameRate前提）
    void SetFrameDuration(float seconds) noexcept {
        frameInterval_ = static_cast<uint32_t>(seconds * kAssumedFrameRate);
        if (frameInterval_ < 1) frameInterval_ = 1;
    }

    //------------------------------------------------------------------------
    // 行（アニメーションの種類）
    //------------------------------------------------------------------------

    //! @brief 現在の行を取得
    [[nodiscard]] uint32_t GetRow() const noexcept { return currentRow_; }

    //! @brief 行を設定（先頭フレームにリセット）
    void SetRow(uint32_t row);

    //! @brief 総行数を取得
    [[nodiscard]] uint32_t GetRowCount() const noexcept { return rowCount_; }

    //! @brief 特定の行で使用するフレーム数を設定（0で全列使用）
    void SetRowFrameCount(uint32_t row, uint32_t frameCount);

    //! @brief 特定の行で使用するフレーム数を取得
    [[nodiscard]] uint32_t GetRowFrameCount(uint32_t row) const;

    //------------------------------------------------------------------------
    // 列（フレーム）
    //------------------------------------------------------------------------

    //! @brief 現在の列（フレーム）を取得
    [[nodiscard]] uint32_t GetColumn() const noexcept { return currentCol_; }

    //! @brief 列を直接設定
    void SetColumn(uint32_t col);

    //! @brief 総列数を取得
    [[nodiscard]] uint32_t GetColumnCount() const noexcept { return colCount_; }

    //------------------------------------------------------------------------
    // 反転（ミラーリング）
    //------------------------------------------------------------------------

    //! @brief 左右反転を設定
    void SetMirror(bool mirror) noexcept { mirror_ = mirror; }
    [[nodiscard]] bool GetMirror() const noexcept { return mirror_; }

    //------------------------------------------------------------------------
    // UV座標取得（SpriteRendererで使用）
    //------------------------------------------------------------------------

    //! @brief 現在のフレームのUV座標を取得（左上）
    //! @return UV座標（ミラー時は右上）
    [[nodiscard]] Vector2 GetUVCoord() const;

    //! @brief 1フレームのUVサイズを取得
    //! @return UVサイズ（ミラー時はXが負）
    [[nodiscard]] Vector2 GetUVSize() const;

    //! @brief 現在フレームのソース矩形を取得（ピクセル単位）
    //! @param textureWidth テクスチャの幅
    //! @param textureHeight テクスチャの高さ
    //! @return (x, y, width, height)
    [[nodiscard]] Vector4 GetSourceRect(float textureWidth, float textureHeight) const;

private:
    //! @brief 現在行の有効フレーム数を取得
    [[nodiscard]] uint32_t GetCurrentRowFrameLimit() const;

    // シート設定
    uint32_t rowCount_;     //!< シート縦分割数（行数）
    uint32_t colCount_;     //!< シート横分割数（列数）

    // 行ごとの有効フレーム数（0で全列使用）
    std::array<uint32_t, kMaxRows> rowFrameCounts_{};

    // 再生状態
    uint32_t currentRow_ = 0;       //!< 現在の行
    uint32_t currentCol_ = 0;       //!< 現在の列（フレーム）
    uint32_t frameInterval_ = 1;    //!< フレーム間隔（ゲームフレーム数）
    uint32_t counter_ = 0;          //!< 経過フレームカウンタ

    // フラグ
    bool mirror_ = false;   //!< 左右反転
    bool playing_ = true;   //!< 再生中
    bool looping_ = true;   //!< ループ再生

    // UV計算用キャッシュ
    Vector2 uvSize_;        //!< 1フレームのUVサイズ
};
