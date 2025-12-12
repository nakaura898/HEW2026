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
//!
//! @note メモリレイアウト: 32バイト（16バイトアライン x 2）
//============================================================================
class Animator : public Component {
public:
    //! @brief 行ごとの最大列数を格納できる最大行数
    static constexpr uint8_t kMaxRows = 16;

    //! @brief 想定フレームレート（SetFrameDuration用）
    static constexpr float kAssumedFrameRate = 60.0f;

    //------------------------------------------------------------------------
    //! @brief コンストラクタ
    //! @param rows シート縦方向の分割数（アニメーションの種類数、max 255）
    //! @param cols シート横方向の分割数（最大フレーム数、max 255）
    //! @param frameInterval フレーム間隔（ゲームフレーム数、max 255）
    //------------------------------------------------------------------------
    Animator(uint8_t rows = 1, uint8_t cols = 1, uint8_t frameInterval = 1);

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
    void SetPlaying(bool playing) noexcept { SetFlag(kFlagPlaying, playing); }
    [[nodiscard]] bool IsPlaying() const noexcept { return GetFlag(kFlagPlaying); }

    //! @brief ループ再生の設定
    void SetLooping(bool loop) noexcept { SetFlag(kFlagLooping, loop); }
    [[nodiscard]] bool IsLooping() const noexcept { return GetFlag(kFlagLooping); }

    //------------------------------------------------------------------------
    // フレーム間隔
    //------------------------------------------------------------------------

    //! @brief フレーム間隔を設定（ゲームフレーム数、max 255）
    void SetFrameInterval(uint8_t frames) noexcept { frameInterval_ = frames > 0 ? frames : 1; }
    [[nodiscard]] uint8_t GetFrameInterval() const noexcept { return frameInterval_; }

    //! @brief フレーム間隔を秒で設定（kAssumedFrameRate前提、max ~4.25秒）
    void SetFrameDuration(float seconds) noexcept {
        float frames = seconds * kAssumedFrameRate;
        frameInterval_ = frames >= 255.0f ? 255 : (frames < 1.0f ? 1 : static_cast<uint8_t>(frames));
    }

    //------------------------------------------------------------------------
    // 行（アニメーションの種類）
    //------------------------------------------------------------------------

    //! @brief 現在の行を取得
    [[nodiscard]] uint8_t GetRow() const noexcept { return currentRow_; }

    //! @brief 行を設定（先頭フレームにリセット）
    void SetRow(uint8_t row);

    //! @brief 総行数を取得
    [[nodiscard]] uint8_t GetRowCount() const noexcept { return rowCount_; }

    //! @brief 特定の行で使用するフレーム数を設定（0で全列使用）
    void SetRowFrameCount(uint8_t row, uint8_t frameCount);

    //! @brief 特定の行で使用するフレーム数を取得
    [[nodiscard]] uint8_t GetRowFrameCount(uint8_t row) const;

    //! @brief 特定の行のフレーム数とフレーム間隔を同時に設定
    //! @param row 行番号
    //! @param frameCount フレーム数（0で全列使用）
    //! @param frameInterval フレーム間隔（0でデフォルト値使用）
    void SetRowFrameCount(uint8_t row, uint8_t frameCount, uint8_t frameInterval);

    //! @brief 特定の行のフレーム間隔を設定
    //! @param row 行番号
    //! @param frameInterval フレーム間隔（0でデフォルト値使用）
    void SetRowFrameInterval(uint8_t row, uint8_t frameInterval);

    //! @brief 特定の行のフレーム間隔を取得
    //! @param row 行番号
    //! @return フレーム間隔（0の場合はデフォルト値を使用）
    [[nodiscard]] uint8_t GetRowFrameInterval(uint8_t row) const;

    //------------------------------------------------------------------------
    // 列（フレーム）
    //------------------------------------------------------------------------

    //! @brief 現在の列（フレーム）を取得
    [[nodiscard]] uint8_t GetColumn() const noexcept { return currentCol_; }

    //! @brief 列を直接設定
    void SetColumn(uint8_t col);

    //! @brief 総列数を取得
    [[nodiscard]] uint8_t GetColumnCount() const noexcept { return colCount_; }

    //------------------------------------------------------------------------
    // 反転（ミラーリング）
    //------------------------------------------------------------------------

    //! @brief 左右反転を設定
    void SetMirror(bool mirror) noexcept { SetFlag(kFlagMirror, mirror); }
    [[nodiscard]] bool GetMirror() const noexcept { return GetFlag(kFlagMirror); }

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
    [[nodiscard]] uint8_t GetCurrentRowFrameLimit() const;

    //------------------------------------------------------------------------
    // フラグビット定義
    //------------------------------------------------------------------------
    static constexpr uint8_t kFlagMirror  = 0x01;  //!< bit0: 左右反転
    static constexpr uint8_t kFlagPlaying = 0x02;  //!< bit1: 再生中
    static constexpr uint8_t kFlagLooping = 0x04;  //!< bit2: ループ再生

    //! @brief フラグ設定ヘルパー
    void SetFlag(uint8_t flag, bool value) noexcept {
        if (value) flags_ |= flag; else flags_ &= ~flag;
    }
    //! @brief フラグ取得ヘルパー
    [[nodiscard]] bool GetFlag(uint8_t flag) const noexcept {
        return (flags_ & flag) != 0;
    }

    //------------------------------------------------------------------------
    // メンバ変数
    //------------------------------------------------------------------------

    // 16 bytes: 行ごとの有効フレーム数（0で全列使用）
    std::array<uint8_t, kMaxRows> rowFrameCounts_{};

    // 16 bytes: 行ごとのフレーム間隔（0でデフォルト値使用）
    std::array<uint8_t, kMaxRows> rowFrameIntervals_{};

    // 8 bytes: UVキャッシュ
    Vector2 uvSize_;

    // 8 bytes: 状態（パック）
    uint8_t rowCount_;            //!< シート縦分割数（行数）
    uint8_t colCount_;            //!< シート横分割数（列数）
    uint8_t currentRow_ = 0;      //!< 現在の行
    uint8_t currentCol_ = 0;      //!< 現在の列（フレーム）
    uint8_t frameInterval_ = 1;   //!< フレーム間隔（ゲームフレーム数）
    uint8_t counter_ = 0;         //!< 経過フレームカウンタ
    uint8_t flags_ = kFlagPlaying | kFlagLooping;  //!< フラグ（mirror/playing/looping）
    uint8_t reserved_ = 0;        //!< 予約（パディング）
};
