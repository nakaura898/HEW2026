//----------------------------------------------------------------------------
//! @file   stage_progress_manager.h
//! @brief  ステージ進行管理システム
//----------------------------------------------------------------------------
#pragma once

#include <memory>
#include <vector>
#include <string>
#include <cassert>

//----------------------------------------------------------------------------
//! @brief 持ち越しグループデータ
//----------------------------------------------------------------------------
struct CarryOverGroupData
{
    std::string id;             //!< グループID
    std::string species;        //!< 種族
    int aliveCount = 0;         //!< 生存個体数
    float totalHp = 0.0f;       //!< 合計HP
    float attackDamage = 0.0f;  //!< 攻撃力
    float moveSpeed = 0.0f;     //!< 移動速度
    float threat = 0.0f;        //!< 脅威度
    float detectionRange = 0.0f;//!< 索敵範囲
};

//----------------------------------------------------------------------------
//! @brief ステージ進行管理（シングルトン）
//----------------------------------------------------------------------------
class StageProgressManager
{
public:
    //! @brief シングルトン取得
    static StageProgressManager& Get()
    {
        assert(instance_ && "StageProgressManager::Create() not called");
        return *instance_;
    }

    //! @brief インスタンス生成
    static void Create()
    {
        if (!instance_) {
            instance_.reset(new StageProgressManager());
        }
    }

    //! @brief インスタンス破棄
    static void Destroy()
    {
        instance_.reset();
    }

    //! @brief デストラクタ
    ~StageProgressManager() = default;

    //------------------------------------------------------------------------
    // ステージ進行
    //------------------------------------------------------------------------

    //! @brief 現在のステージ番号を取得
    [[nodiscard]] int GetCurrentStage() const { return currentStage_; }

    //! @brief 次のステージへ進む
    void AdvanceToNextStage() { currentStage_++; }

    //! @brief ステージをリセット（最初から）
    void ResetProgress()
    {
        currentStage_ = 1;
        carryOverGroups_.clear();
        totalBindsUsed_ = 0;
        totalCutsUsed_ = 0;
    }

    //------------------------------------------------------------------------
    // 持ち越しグループ
    //------------------------------------------------------------------------

    //! @brief 持ち越しグループを追加
    void AddCarryOverGroup(const CarryOverGroupData& data)
    {
        carryOverGroups_.push_back(data);
    }

    //! @brief 持ち越しグループデータを取得
    [[nodiscard]] const std::vector<CarryOverGroupData>& GetCarryOverGroups() const
    {
        return carryOverGroups_;
    }

    //! @brief 持ち越しデータをクリア（ゲームオーバー時）
    void ClearCarryOver()
    {
        carryOverGroups_.clear();
    }

    //------------------------------------------------------------------------
    // 統計データ
    //------------------------------------------------------------------------

    //! @brief 使用した結ぶ/切る回数を保存
    void AddActionCounts(int bindCount, int cutCount)
    {
        assert(bindCount >= 0 && "bindCount must be non-negative");
        assert(cutCount >= 0 && "cutCount must be non-negative");
        totalBindsUsed_ += bindCount;
        totalCutsUsed_ += cutCount;
    }

    //! @brief 累計結ぶ回数を取得
    [[nodiscard]] int GetTotalBindsUsed() const { return totalBindsUsed_; }

    //! @brief 累計切る回数を取得
    [[nodiscard]] int GetTotalCutsUsed() const { return totalCutsUsed_; }

private:
    StageProgressManager() = default;
    StageProgressManager(const StageProgressManager&) = delete;
    StageProgressManager& operator=(const StageProgressManager&) = delete;

    static inline std::unique_ptr<StageProgressManager> instance_ = nullptr;

    int currentStage_ = 1;                              //!< 現在のステージ番号
    std::vector<CarryOverGroupData> carryOverGroups_;   //!< 持ち越しグループ
    int totalBindsUsed_ = 0;                            //!< 累計結ぶ回数
    int totalCutsUsed_ = 0;                             //!< 累計切る回数
};
