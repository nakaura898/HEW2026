//----------------------------------------------------------------------------
//! @file   system_manager.h
//! @brief  ゲームシステムの一括管理
//! @details 依存関係順でシステムを初期化・破棄する
//----------------------------------------------------------------------------
#pragma once

//----------------------------------------------------------------------------
//! @brief システムマネージャー
//! @details 全シングルトンシステムのライフサイクルを管理
//!          依存関係順で Create → Initialize → Shutdown → Destroy
//----------------------------------------------------------------------------
class SystemManager
{
public:
    //! @brief 全システムを生成（依存関係順）
    //! @note ゲーム開始時に1回呼び出す
    static void CreateAll();

    //! @brief 全システムを破棄（逆順）
    //! @note ゲーム終了時に1回呼び出す
    static void DestroyAll();

    //! @brief 生成済みか
    [[nodiscard]] static bool IsCreated() { return created_; }

private:
    static inline bool created_ = false;
};
