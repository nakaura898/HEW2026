//----------------------------------------------------------------------------
//! @file   animator.cpp
//! @brief  スプライトシートアニメーションコンポーネント実装
//----------------------------------------------------------------------------

#include "animator.h"
#include <cassert>

Animator::Animator(uint32_t rows, uint32_t cols, uint32_t frameInterval)
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
    if (!playing_) return;

    ++counter_;
    if (counter_ >= frameInterval_) {
        counter_ = 0;

        uint32_t limit = GetCurrentRowFrameLimit();
        uint32_t nextCol = currentCol_ + 1;

        if (nextCol >= limit) {
            if (looping_) {
                currentCol_ = 0;
            } else {
                // ループしない場合は最終フレームで停止
                currentCol_ = limit - 1;
                playing_ = false;
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
    playing_ = true;
}

void Animator::SetRow(uint32_t row)
{
    currentRow_ = row % rowCount_;
    // フレーム位置が現在の行の制限を超える場合のみ調整
    uint32_t limit = GetCurrentRowFrameLimit();
    if (currentCol_ >= limit) {
        currentCol_ = 0;
    }
    counter_ = 0;
}

void Animator::SetRowFrameCount(uint32_t row, uint32_t frameCount)
{
    assert(row < rowCount_ && "SetRowFrameCount: row out of range");
    assert(row < kMaxRows && "SetRowFrameCount: row exceeds kMaxRows");
    if (row >= rowCount_ || row >= kMaxRows) return;

    // 0または列数を超える場合は「全列使用」として0を格納
    if (frameCount == 0 || frameCount > colCount_) {
        rowFrameCounts_[row] = 0;
    } else {
        rowFrameCounts_[row] = frameCount;
    }
}

uint32_t Animator::GetRowFrameCount(uint32_t row) const
{
    assert(row < rowCount_ && "GetRowFrameCount: row out of range");
    assert(row < kMaxRows && "GetRowFrameCount: row exceeds kMaxRows");
    if (row >= rowCount_ || row >= kMaxRows) return colCount_;

    uint32_t limit = rowFrameCounts_[row];
    return limit > 0 ? limit : colCount_;
}

void Animator::SetColumn(uint32_t col)
{
    uint32_t limit = GetCurrentRowFrameLimit();
    currentCol_ = col < limit ? col : limit - 1;
    counter_ = 0;
}

Vector2 Animator::GetUVCoord() const
{
    float u = uvSize_.x * static_cast<float>(currentCol_);
    float v = uvSize_.y * static_cast<float>(currentRow_);

    // ミラー時は右端から描画
    if (mirror_) {
        u += uvSize_.x;
    }

    return Vector2(u, v);
}

Vector2 Animator::GetUVSize() const
{
    // ミラー時は幅を負にする
    return mirror_
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

uint32_t Animator::GetCurrentRowFrameLimit() const
{
    if (currentRow_ >= kMaxRows) return colCount_;

    uint32_t limit = rowFrameCounts_[currentRow_];
    return limit > 0 ? limit : colCount_;
}
