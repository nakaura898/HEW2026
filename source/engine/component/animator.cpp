//----------------------------------------------------------------------------
//! @file   animator.cpp
//! @brief  スプライトシートアニメーションコンポーネント実装
//----------------------------------------------------------------------------

#include "animator.h"
#include <cassert>

Animator::Animator(uint8_t rows, uint8_t cols, uint8_t frameInterval)
    : rowCount_(rows > 0 ? rows : 1)
    , colCount_(cols > 0 ? cols : 1)
    , frameInterval_(frameInterval > 0 ? frameInterval : 1)
{
    // UVサイズを計算
    uvSize_ = Vector2(1.0f / static_cast<float>(colCount_),
                      1.0f / static_cast<float>(rowCount_));
}

void Animator::Update([[maybe_unused]] float deltaTime)
{
    if (!IsPlaying()) return;

    // 現在行のフレーム間隔を取得（0ならデフォルト値を使用）
    uint8_t interval = GetRowFrameInterval(currentRow_);
    if (interval == 0) interval = frameInterval_;

    ++counter_;
    if (counter_ >= interval) {
        counter_ = 0;

        uint8_t limit = GetCurrentRowFrameLimit();
        uint8_t nextCol = currentCol_ + 1;

        if (nextCol >= limit) {
            if (IsLooping()) {
                currentCol_ = 0;
            } else {
                // ループしない場合は最終フレームで停止
                currentCol_ = static_cast<uint8_t>(limit - 1);
                SetPlaying(false);
            }
        } else {
            currentCol_ = nextCol;
        }
    }
}

void Animator::Reset()
{
    currentCol_ = 0;
    counter_ = 0;
    SetPlaying(true);
}

void Animator::SetRow(uint8_t row)
{
    currentRow_ = row % rowCount_;
    // フレーム位置が現在の行の制限を超える場合のみ調整
    uint8_t limit = GetCurrentRowFrameLimit();
    if (currentCol_ >= limit) {
        currentCol_ = 0;
    }
    counter_ = 0;
}

void Animator::SetRowFrameCount(uint8_t row, uint8_t frameCount)
{
    assert(row < rowCount_ && "row（行）の番号/インデックスに、存在しない値（大きすぎる or 負の値）を渡している");
    assert(row < kMaxRows && "SetRowFrameCount: 行が最大行数（kMaxRows）を超えています");
    if (row >= rowCount_ || row >= kMaxRows) return;

    // 0または列数を超える場合は「全列使用」として0を格納
    if (frameCount == 0 || frameCount > colCount_) {
        rowFrameCounts_[row] = 0;
    } else {
        rowFrameCounts_[row] = frameCount;
    }
}

uint8_t Animator::GetRowFrameCount(uint8_t row) const
{
    assert(row < rowCount_ && "GetRowFrameCount: 行番号が有効範囲外です");
    assert(row < kMaxRows   && "GetRowFrameCount: 行番号が最大行数(kMaxRows)を超えています");

    if (row >= rowCount_ || row >= kMaxRows) return colCount_;

    uint8_t limit = rowFrameCounts_[row];
    return limit > 0 ? limit : colCount_;
}

void Animator::SetRowFrameCount(uint8_t row, uint8_t frameCount, uint8_t frameInterval)
{
    SetRowFrameCount(row, frameCount);
    SetRowFrameInterval(row, frameInterval);
}

void Animator::SetRowFrameInterval(uint8_t row, uint8_t frameInterval)
{
    assert(row < rowCount_ && "SetRowFrameInterval: 行番号が有効範囲外です");
    assert(row < kMaxRows   && "SetRowFrameInterval: 行番号が最大行数(kMaxRows)を超えています");

    if (row >= rowCount_ || row >= kMaxRows) return;

    rowFrameIntervals_[row] = frameInterval;
}

uint8_t Animator::GetRowFrameInterval(uint8_t row) const
{
    assert(row < kMaxRows && "GetRowFrameInterval: 行番号が最大行数(kMaxRows)を超えています");
    if (row >= kMaxRows) return frameInterval_;

    uint8_t interval = rowFrameIntervals_[row];
    return interval > 0 ? interval : frameInterval_;
}

void Animator::SetColumn(uint8_t col)
{
    uint8_t limit = GetCurrentRowFrameLimit();
    currentCol_ = col < limit ? col : static_cast<uint8_t>(limit - 1);
    counter_ = 0;
}

Vector2 Animator::GetUVCoord() const
{
    float u = uvSize_.x * static_cast<float>(currentCol_);
    float v = uvSize_.y * static_cast<float>(currentRow_);

    // ミラー時は右端から描画
    if (GetMirror()) {
        u += uvSize_.x;
    }

    return Vector2(u, v);
}

Vector2 Animator::GetUVSize() const
{
    // ミラー時は幅を負にする
    return GetMirror()
        ? Vector2(-uvSize_.x, uvSize_.y)
        : uvSize_;
}

Vector4 Animator::GetSourceRect(float textureWidth, float textureHeight) const
{
    float frameWidth = textureWidth / static_cast<float>(colCount_);
    float frameHeight = textureHeight / static_cast<float>(rowCount_);

    float x = frameWidth * static_cast<float>(currentCol_);
    float y = frameHeight * static_cast<float>(currentRow_);

    return Vector4(x, y, frameWidth, frameHeight);
}

uint8_t Animator::GetCurrentRowFrameLimit() const
{
    if (currentRow_ >= kMaxRows) return colCount_;

    uint8_t limit = rowFrameCounts_[currentRow_];
    return limit > 0 ? limit : colCount_;
}
