//----------------------------------------------------------------------------
//! @file   scene.h
//! @brief  シーン基底クラス
//----------------------------------------------------------------------------
#pragma once

#include <string>
#include <cstdint>
#include <atomic>
#include <algorithm>

//----------------------------------------------------------------------------
//! @brief シーン基底クラス
//!
//! ゲームの各画面（タイトル、ゲーム、リザルト等）の基底クラス。
//! 派生クラスで各仮想関数をオーバーライドして実装する。
//----------------------------------------------------------------------------
class Scene
{
public:
    Scene() = default;
    virtual ~Scene() noexcept = default;

    // コピー禁止、ムーブ許可
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;
    Scene(Scene&&) noexcept = default;
    Scene& operator=(Scene&&) noexcept = default;

    //----------------------------------------------------------
    //! @name ライフサイクル
    //----------------------------------------------------------
    //!@{

    //! @brief シーン開始時に呼ばれる
    virtual void OnEnter() {}

    //! @brief シーン終了時に呼ばれる
    virtual void OnExit() {}

    //!@}
    //----------------------------------------------------------
    //! @name 非同期ロード
    //----------------------------------------------------------
    //!@{

    //! @brief 非同期ロード処理（バックグラウンドスレッドで実行）
    //! @note テクスチャ、シェーダー等の重いリソースをここでロード
    //! @note D3D11はスレッドセーフなのでGPUリソース作成可能
    virtual void OnLoadAsync() {}

    //! @brief 非同期ロード完了後、メインスレッドで呼ばれる
    //! @note OnEnter()の前に呼ばれる
    virtual void OnLoadComplete() {}

    //! @brief ロード進捗を設定（0.0〜1.0）
    void SetLoadProgress(float progress) { loadProgress_.store(std::clamp(progress, 0.0f, 1.0f)); }

    //! @brief ロード進捗を取得
    [[nodiscard]] float GetLoadProgress() const { return loadProgress_.load(); }

    //!@}

    //----------------------------------------------------------
    //! @name フレームコールバック
    //----------------------------------------------------------
    //!@{

    //! @brief 更新処理
    virtual void Update() {}

    //! @brief 描画処理
    virtual void Render() {}

    //!@}

    //----------------------------------------------------------
    //! @name プロパティ
    //----------------------------------------------------------
    //!@{

    //! @brief シーン名を取得
    [[nodiscard]] virtual const char* GetName() const { return "Scene"; }

    //!@}

private:
    std::atomic<float> loadProgress_{ 0.0f };  //!< ロード進捗（0.0〜1.0）
};
