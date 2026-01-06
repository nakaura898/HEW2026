//----------------------------------------------------------------------------
//! @file   mesh_handle.h
//! @brief  メッシュハンドル（所有権なし参照）
//----------------------------------------------------------------------------
#pragma once

#include <cstdint>

//----------------------------------------------------------------------------
//! @brief メッシュハンドル
//! @details Generation-based handle: 古いハンドルの誤使用を検出
//!
//! MeshPtrと異なり、所有権を持たない。
//! MeshManagerが全メッシュを所有し、Handleは参照のみ。
//! Shutdown時に自動解放されるため、手動reset()不要。
//!
//! @note 構造: upper 16 bits = generation, lower 16 bits = index
//! @note 最大65535メッシュまで対応
//----------------------------------------------------------------------------
struct MeshHandle
{
    uint32_t id = 0;  //!< 0 = invalid

    //! @brief 有効なハンドルか
    [[nodiscard]] bool IsValid() const noexcept { return id != 0; }

    //! @brief bool変換
    explicit operator bool() const noexcept { return IsValid(); }

    //! @brief スロットインデックス取得
    [[nodiscard]] uint16_t GetIndex() const noexcept {
        return static_cast<uint16_t>(id & 0xFFFF);
    }

    //! @brief 世代番号取得
    //! @note Create()で+1されているため、-1して元の値を返す
    [[nodiscard]] uint16_t GetGeneration() const noexcept {
        return static_cast<uint16_t>((id >> 16) - 1);
    }

    //! @brief ハンドル生成
    [[nodiscard]] static MeshHandle Create(uint16_t index, uint16_t generation) noexcept {
        // generation=0でindex=0の場合でもid=0にならないよう、generationを+1する
        uint16_t gen = static_cast<uint16_t>(generation + 1);
        return MeshHandle{ (static_cast<uint32_t>(gen) << 16) | index };
    }

    //! @brief 無効なハンドル
    [[nodiscard]] static constexpr MeshHandle Invalid() noexcept {
        return MeshHandle{ 0 };
    }

    //! @brief 比較演算子
    bool operator==(const MeshHandle& other) const noexcept { return id == other.id; }
    bool operator!=(const MeshHandle& other) const noexcept { return id != other.id; }
};
