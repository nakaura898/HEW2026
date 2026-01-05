//----------------------------------------------------------------------------
//! @file   game_state_manager.h
//! @brief  ゲーム状態管理 - 勝敗判定とゲーム進行を管理
//----------------------------------------------------------------------------
#pragma once

#include <functional>
#include <vector>
#include <memory>
#include <cassert>

// 前方宣言
class Group;
class Player;

//----------------------------------------------------------------------------
//! @brief ゲーム状態
//----------------------------------------------------------------------------
enum class GameState
{
    Playing,    //!< プレイ中
    Victory,    //!< 勝利
    Defeat      //!< 敗北
};

//----------------------------------------------------------------------------
//! @brief ゲーム状態管理（シングルトン）
//! @details 勝敗条件の判定とゲーム進行を管理
//----------------------------------------------------------------------------
class GameStateManager
{
public:
    //! @brief シングルトンインスタンス取得
    static GameStateManager& Get();

    //! @brief インスタンス生成
    static void Create();

    //! @brief インスタンス破棄
    static void Destroy();

    //! @brief デストラクタ
    ~GameStateManager() = default;

    //------------------------------------------------------------------------
    // 初期化・更新
    //------------------------------------------------------------------------

    //! @brief ゲームを初期化
    void Initialize();

    //! @brief システム更新
    void Update();

    //! @brief ゲームをリセット
    void Reset();

    //------------------------------------------------------------------------
    // 登録
    //------------------------------------------------------------------------

    //! @brief プレイヤーを設定
    void SetPlayer(Player* player) { player_ = player; }

    //------------------------------------------------------------------------
    // 状態取得
    //------------------------------------------------------------------------

    //! @brief 現在のゲーム状態を取得
    [[nodiscard]] GameState GetState() const { return state_; }

    //! @brief プレイ中か判定
    [[nodiscard]] bool IsPlaying() const { return state_ == GameState::Playing; }

    //! @brief 勝利したか判定
    [[nodiscard]] bool IsVictory() const { return state_ == GameState::Victory; }

    //! @brief 敗北したか判定
    [[nodiscard]] bool IsDefeat() const { return state_ == GameState::Defeat; }

    //! @brief 最後の結果を取得（Result_Scene用）
    [[nodiscard]] static GameState GetLastResult() { return lastResult_; }

    //------------------------------------------------------------------------
    // 勝敗条件
    //------------------------------------------------------------------------

    //! @brief 勝利条件をチェック
    //! @return 勝利ならtrue
    [[nodiscard]] bool CheckVictoryCondition() const;

    //! @brief 敗北条件をチェック
    //! @return 敗北ならtrue
    [[nodiscard]] bool CheckDefeatCondition() const;

    //------------------------------------------------------------------------
    // コールバック
    //------------------------------------------------------------------------

    //! @brief 勝利時コールバック
    void SetOnVictory(std::function<void()> callback) { onVictory_ = callback; }

    //! @brief 敗北時コールバック
    void SetOnDefeat(std::function<void()> callback) { onDefeat_ = callback; }

    //! @brief 状態変更時コールバック
    void SetOnStateChanged(std::function<void(GameState)> callback) { onStateChanged_ = callback; }

private:
    GameStateManager() = default;
    GameStateManager(const GameStateManager&) = delete;
    GameStateManager& operator=(const GameStateManager&) = delete;

    //! @brief 状態を設定
    void SetState(GameState state);

    //! @brief 全敵が全滅したか判定
    [[nodiscard]] bool AreAllEnemiesDefeated() const;

    //! @brief 全敵がプレイヤーネットワーク内か判定
    [[nodiscard]] bool AreAllEnemiesInPlayerNetwork() const;

    static inline std::unique_ptr<GameStateManager> instance_ = nullptr;

    GameState state_ = GameState::Playing;  //!< 現在の状態
    Player* player_ = nullptr;              //!< プレイヤー参照

    static inline GameState lastResult_ = GameState::Playing;  //!< 最後の結果（シーン間で共有）

    // コールバック
    std::function<void()> onVictory_;
    std::function<void()> onDefeat_;
    std::function<void(GameState)> onStateChanged_;
};
