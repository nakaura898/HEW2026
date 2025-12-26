//----------------------------------------------------------------------------
//! @file   scene_manager.h
//! @brief  シーンマネージャー
//----------------------------------------------------------------------------
#pragma once

#include "scene.h"
#include <memory>
#include <future>
#include <atomic>
#include <functional>

//----------------------------------------------------------------------------
//! @brief シーンマネージャー（シングルトン）
//!
//! シーン切り替えの予約を管理する。
//! シーンの所有はGame側で行う。
//----------------------------------------------------------------------------
class SceneManager final
{
public:
    //! @brief シングルトンインスタンス取得
    static SceneManager& Get() noexcept;

private:
    SceneManager() = default;
    ~SceneManager() = default;

    // コピー・ムーブ禁止
    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;
    SceneManager(SceneManager&&) = delete;
    SceneManager& operator=(SceneManager&&) = delete;

public:

    //----------------------------------------------------------
    //! @name シーン切り替え（同期）
    //----------------------------------------------------------
    //!@{

    //! @brief シーン読み込みを予約（同期）
    //! @tparam T シーンクラス（Sceneの派生クラス）
    template<typename T>
    void Load()
    {
        pendingFactory_ = &CreateScene<T>;
        asyncPending_ = false;
    }

    //! @brief 予約されたシーン切り替えを適用
    //! @param current 現在のシーン（参照で受け取り、切り替え後は新シーンになる）
    void ApplyPendingChange(std::unique_ptr<Scene>& current);

    //!@}
    //----------------------------------------------------------
    //! @name シーン切り替え（非同期）
    //----------------------------------------------------------
    //!@{

    //! @brief シーンを非同期で読み込み予約
    //! @tparam T シーンクラス（Sceneの派生クラス）
    //! @note バックグラウンドでOnLoadAsync()を実行
    template<typename T>
    void LoadAsync()
    {
        // 既にロード中なら無視
        if (IsLoading()) return;

        asyncPending_ = true;
        loadingScene_ = std::make_unique<T>();
        loadProgress_.store(0.0f);

        // バックグラウンドでロード開始
        loadFuture_ = std::async(std::launch::async, [this]() {
            if (loadingScene_) {
                loadingScene_->OnLoadAsync();
                loadingScene_->SetLoadProgress(1.0f);
            }
        });
    }

    //! @brief 非同期ロード中かどうか
    [[nodiscard]] bool IsLoading() const;

    //! @brief ロード進捗を取得（0.0〜1.0）
    [[nodiscard]] float GetLoadProgress() const;

    //! @brief 非同期ロードをキャンセル
    void CancelAsyncLoad();

    //!@}

private:
    //! シーン生成ヘルパー
    template<typename T>
    static std::unique_ptr<Scene> CreateScene()
    {
        return std::make_unique<T>();
    }

    //! シーン生成関数の型
    using SceneFactory = std::unique_ptr<Scene>(*)();

    //! 次のシーン生成関数（同期切り替え予約）
    SceneFactory pendingFactory_ = nullptr;

    //! 非同期ロード関連
    std::unique_ptr<Scene> loadingScene_;       //!< ロード中のシーン
    std::future<void> loadFuture_;              //!< ロードタスク
    std::atomic<float> loadProgress_{ 0.0f };   //!< ロード進捗
    bool asyncPending_ = false;                 //!< 非同期切り替え予約フラグ
};
