//----------------------------------------------------------------------------
//! @file   game.h
//! @brief  ゲームクラス
//----------------------------------------------------------------------------
#pragma once

#include "engine/scene/scene.h"
#include <memory>

// 前方宣言
class SceneManager;

//----------------------------------------------------------------------------
//! @brief ゲームクラス
//!
//! シーンを所有し、ゲームループを実行する。
//! シーン切り替えはフレーム末に実行される。
//----------------------------------------------------------------------------
class Game final
{
public:
    Game();
    ~Game() noexcept = default;

    // コピー・ムーブ禁止
    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;
    Game(Game&&) = delete;
    Game& operator=(Game&&) = delete;

    //----------------------------------------------------------
    //! @name ライフサイクル
    //----------------------------------------------------------
    //!@{

    //! @brief 初期化
    [[nodiscard]] bool Initialize();

    //! @brief 終了処理
    void Shutdown() noexcept;

    //! @brief ログファイルを閉じる（Application::Shutdown後に呼ぶ）
    static void CloseLog() noexcept;

    //!@}

    //----------------------------------------------------------
    //! @name フレームコールバック
    //----------------------------------------------------------
    //!@{

    //! @brief 更新
    void Update();

    //! @brief 描画
    void Render();

    //! @brief フレーム末処理
    void EndFrame();

    //!@}

private:
    SceneManager& sceneManager_;              //!< シーンマネージャー参照
    std::unique_ptr<Scene> currentScene_;     //!< 現在のシーン（所有）
};
