//----------------------------------------------------------------------------
//! @file   fe_system.h
//! @brief  FEシステム - プレイヤーのFE（縁エネルギー）を管理
//----------------------------------------------------------------------------
#pragma once

#include <functional>

// 前方宣言
class Player;

//----------------------------------------------------------------------------
//! @brief FEシステム（シングルトン）
//! @details プレイヤーのFE消費/回復を管理し、変更時にイベントを発行
//----------------------------------------------------------------------------
class FESystem
{
public:
    //! @brief シングルトンインスタンス取得
    static FESystem& Get();

    //------------------------------------------------------------------------
    // 初期化
    //------------------------------------------------------------------------

    //! @brief プレイヤーを設定
    void SetPlayer(Player* player) { player_ = player; }

    //! @brief プレイヤーを取得
    [[nodiscard]] Player* GetPlayer() const { return player_; }

    //------------------------------------------------------------------------
    // FE操作
    //------------------------------------------------------------------------

    //! @brief FEを消費できるか判定
    //! @param amount 消費量
    //! @return 消費可能ならtrue
    [[nodiscard]] bool CanConsume(float amount) const;

    //! @brief FEを消費
    //! @param amount 消費量
    //! @return 消費成功ならtrue
    bool Consume(float amount);

    //! @brief FEを回復
    //! @param amount 回復量
    void Recover(float amount);

    //------------------------------------------------------------------------
    // FE取得
    //------------------------------------------------------------------------

    //! @brief 現在FEを取得
    [[nodiscard]] float GetCurrentFE() const;

    //! @brief 最大FEを取得
    [[nodiscard]] float GetMaxFE() const;

    //! @brief FE割合を取得（0.0〜1.0）
    [[nodiscard]] float GetFERatio() const;

    //------------------------------------------------------------------------
    // コールバック
    //------------------------------------------------------------------------

    //! @brief FE変更時コールバック
    //! @param callback (現在FE, 最大FE, 変化量)
    void SetOnFEChanged(std::function<void(float, float, float)> callback)
    {
        onFEChanged_ = callback;
    }

private:
    FESystem() = default;
    ~FESystem() = default;
    FESystem(const FESystem&) = delete;
    FESystem& operator=(const FESystem&) = delete;

    Player* player_ = nullptr;

    // コールバック
    std::function<void(float, float, float)> onFEChanged_;
};
