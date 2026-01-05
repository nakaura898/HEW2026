//----------------------------------------------------------------------------
//! @file   wave_manager.h
//! @brief  ウェーブ管理システム
//----------------------------------------------------------------------------
#pragma once

#include "game/stage/stage_data.h"
#include "game/systems/group_manager.h"
#include <memory>
#include <vector>
#include <functional>
#include <cassert>

// 前方宣言
class Group;

//----------------------------------------------------------------------------
//! @brief ウェーブ管理（シングルトン）
//! @details 複数ウェーブの敵配置と進行を管理
//----------------------------------------------------------------------------
class WaveManager
{
public:
    //! @brief シングルトン取得
    static WaveManager& Get()
    {
        assert(instance_ && "WaveManager::Create() not called");
        return *instance_;
    }

    //! @brief インスタンス生成
    static void Create()
    {
        if (!instance_) {
            instance_.reset(new WaveManager());
        }
    }

    //! @brief インスタンス破棄
    static void Destroy()
    {
        instance_.reset();
    }

    //! @brief デストラクタ
    ~WaveManager() = default;

    //------------------------------------------------------------------------
    // ウェーブ管理
    //------------------------------------------------------------------------

    //! @brief ウェーブデータで初期化
    //! @param waves ウェーブデータ配列
    void Initialize(const std::vector<WaveData>& waves);

    //! @brief 更新（ウェーブクリア判定）
    void Update();

    //! @brief リセット
    void Reset();

    //! @brief 現在ウェーブをスポーン
    void SpawnCurrentWave();

    //! @brief 次のウェーブへ進む
    void AdvanceToNextWave();

    //------------------------------------------------------------------------
    // 状態取得
    //------------------------------------------------------------------------

    //! @brief 現在のウェーブ番号を取得（1始まり）
    [[nodiscard]] int GetCurrentWave() const { return currentWave_; }

    //! @brief 総ウェーブ数を取得
    [[nodiscard]] int GetTotalWaves() const { return static_cast<int>(waves_.size()); }

    //! @brief 現在ウェーブがクリアされたか判定
    [[nodiscard]] bool IsCurrentWaveCleared() const;

    //! @brief 全ウェーブがクリアされたか判定
    [[nodiscard]] bool IsAllWavesCleared() const;

    //! @brief 現在ウェーブのグループ一覧を取得
    [[nodiscard]] std::vector<Group*> GetCurrentWaveGroups() const
    {
        return GroupManager::Get().GetGroupsForWave(currentWave_);
    }

    //------------------------------------------------------------------------
    // トランジション（ウェーブ間スクロール）
    //------------------------------------------------------------------------

    //! @brief トランジション中か判定
    [[nodiscard]] bool IsTransitioning() const { return isTransitioning_; }

    //! @brief トランジション進捗を取得（0.0〜1.0）
    [[nodiscard]] float GetTransitionProgress() const { return transitionProgress_; }

    //! @brief 現在ウェーブのカメラY座標を取得
    [[nodiscard]] float GetCurrentWaveCameraY() const;

    //! @brief 開始カメラY座標を取得（トランジション元）
    [[nodiscard]] float GetStartCameraY() const { return startCameraY_; }

    //! @brief 目標カメラY座標を取得（トランジション先）
    [[nodiscard]] float GetTargetCameraY() const { return targetCameraY_; }

    //! @brief エリアの高さを設定
    void SetAreaHeight(float height) { areaHeight_ = height; }

    //! @brief エリアの高さを取得
    [[nodiscard]] float GetAreaHeight() const { return areaHeight_; }

    //! @brief トランジション開始
    void StartTransition();

    //! @brief トランジション更新
    //! @param dt デルタタイム
    void UpdateTransition(float dt);

    //------------------------------------------------------------------------
    // コールバック
    //------------------------------------------------------------------------

    //! @brief ウェーブクリア時コールバック（ウェーブ番号を渡す）
    void SetOnWaveCleared(std::function<void(int)> callback) { onWaveCleared_ = callback; }

    //! @brief 全ウェーブクリア時コールバック
    void SetOnAllWavesCleared(std::function<void()> callback) { onAllWavesCleared_ = callback; }

    //! @brief グループスポーン時コールバック（TestSceneが設定）
    //! @param spawner GroupDataを受け取り、生成したGroupポインタを返す関数
    void SetGroupSpawner(std::function<Group*(const GroupData&)> spawner) { groupSpawner_ = spawner; }

    //! @brief トランジション完了時コールバック
    void SetOnTransitionComplete(std::function<void()> callback) { onTransitionComplete_ = callback; }

private:
    WaveManager() = default;
    WaveManager(const WaveManager&) = delete;
    WaveManager& operator=(const WaveManager&) = delete;

    static inline std::unique_ptr<WaveManager> instance_ = nullptr;

    std::vector<WaveData> waves_;                   //!< ウェーブデータ
    int currentWave_ = 1;                           //!< 現在のウェーブ番号（1始まり）
    bool waveCleared_ = false;                      //!< 現在ウェーブクリア済みフラグ

    // トランジション関連
    bool isTransitioning_ = false;                  //!< トランジション中フラグ
    float transitionProgress_ = 0.0f;               //!< トランジション進捗（0.0〜1.0）
    float transitionDuration_ = 1.5f;               //!< トランジション時間（秒）
    float areaHeight_ = 1080.0f;                    //!< 1エリアの高さ
    float startCameraY_ = 0.0f;                     //!< トランジション開始時のカメラY
    float targetCameraY_ = 0.0f;                    //!< トランジション目標のカメラY

    // コールバック
    std::function<void(int)> onWaveCleared_;
    std::function<void()> onAllWavesCleared_;
    std::function<Group*(const GroupData&)> groupSpawner_;
    std::function<void()> onTransitionComplete_;    //!< トランジション完了コールバック
};
