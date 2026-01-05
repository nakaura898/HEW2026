//----------------------------------------------------------------------------
//! @file   group_manager.h
//! @brief  グループ一元管理システム
//----------------------------------------------------------------------------
#pragma once

#include "game/entities/group.h"
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <cassert>

//----------------------------------------------------------------------------
//! @brief グループ一元管理（シングルトン）
//! @details グループの所有権を一元管理し、各システムへの参照を提供
//----------------------------------------------------------------------------
class GroupManager
{
public:
    //! @brief シングルトン取得
    static GroupManager& Get()
    {
        assert(instance_ && "GroupManager::Create() not called");
        return *instance_;
    }

    //! @brief インスタンス生成
    static void Create()
    {
        if (!instance_) {
            instance_.reset(new GroupManager());
        }
    }

    //! @brief インスタンス破棄
    static void Destroy()
    {
        instance_.reset();
    }

    //! @brief デストラクタ
    ~GroupManager() = default;

    //------------------------------------------------------------------------
    // グループ管理
    //------------------------------------------------------------------------

    //! @brief グループを登録（所有権を移譲）
    //! @param group 追加するグループ
    //! @return 登録したグループへのポインタ
    Group* AddGroup(std::unique_ptr<Group> group);

    //! @brief グループを削除
    //! @param group 削除するグループ
    void RemoveGroup(Group* group);

    //! @brief 全グループをクリア
    void Clear();

    //------------------------------------------------------------------------
    // グループ取得
    //------------------------------------------------------------------------

    //! @brief 全グループを取得（読み取り専用）
    [[nodiscard]] const std::vector<std::unique_ptr<Group>>& GetAllGroups() const
    {
        return groups_;
    }

    //! @brief 敵グループのみ取得（IsEnemy() == true）
    [[nodiscard]] std::vector<Group*> GetEnemyGroups() const;

    //! @brief 味方グループのみ取得（IsAlly() == true）
    [[nodiscard]] std::vector<Group*> GetAllyGroups() const;

    //! @brief 生存中のグループのみ取得（!IsDefeated()）
    [[nodiscard]] std::vector<Group*> GetAliveGroups() const;

    //! @brief IDでグループを検索
    //! @param id 検索するグループID
    //! @return 見つかったグループ、なければnullptr
    [[nodiscard]] Group* FindById(const std::string& id) const;

    //------------------------------------------------------------------------
    // ウェーブ管理
    //------------------------------------------------------------------------

    //! @brief ウェーブにグループを紐付け
    //! @param group 紐付けるグループ
    //! @param waveNumber ウェーブ番号（1始まり）
    void AssignToWave(Group* group, int waveNumber);

    //! @brief 特定ウェーブのグループを取得
    //! @param waveNumber ウェーブ番号（1始まり）
    //! @return 該当ウェーブのグループリスト
    [[nodiscard]] std::vector<Group*> GetGroupsForWave(int waveNumber) const;

    //! @brief 未割り当てを示すウェーブ番号定数
    static constexpr int kWaveUnassigned = 0;

    //! @brief グループのウェーブ番号を取得
    //! @param group 対象グループ
    //! @return ウェーブ番号（1始まり）。未割り当ての場合はkWaveUnassigned（0）
    //! @note ウェーブ1は最初のウェーブを意味する。0は「ウェーブに属さない」ことを示す
    [[nodiscard]] int GetWaveNumber(Group* group) const;

    //! @brief ウェーブ割り当てをクリア
    void ClearWaveAssignments();

private:
    GroupManager() = default;
    GroupManager(const GroupManager&) = delete;
    GroupManager& operator=(const GroupManager&) = delete;

    static inline std::unique_ptr<GroupManager> instance_ = nullptr;

    std::vector<std::unique_ptr<Group>> groups_;          //!< 全グループ（所有権保持）
    std::unordered_map<Group*, int> waveAssignments_;     //!< グループ→ウェーブ番号
};
