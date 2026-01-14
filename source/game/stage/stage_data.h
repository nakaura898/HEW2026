//----------------------------------------------------------------------------
//! @file   stage_data.h
//! @brief  ステージデータ構造体
//----------------------------------------------------------------------------
#pragma once

#include <string>
#include <vector>

//----------------------------------------------------------------------------
//! @brief グループ1つ分のデータ
//----------------------------------------------------------------------------
struct GroupData
{
    std::string id;             //!< グループID（例: "group1"）
    std::string species;        //!< 種族名（"Elf", "Knight"）
    int count = 1;              //!< 個体数
    float x = 0.0f;             //!< X座標
    float y = 0.0f;             //!< Y座標
    float threat = 100.0f;      //!< 脅威度
    float detectionRange = 300.0f;  //!< 索敵範囲
    float hp = 100.0f;          //!< 個体HP
    float attack = 10.0f;       //!< 攻撃力
    float speed = 100.0f;       //!< 移動速度
    int wave = 1;               //!< 所属ウェーブ番号（1始まり）
};

//----------------------------------------------------------------------------
//! @brief ウェーブ1つ分のデータ
//----------------------------------------------------------------------------
struct WaveData
{
    int waveNumber = 0;                 //!< ウェーブ番号
    std::vector<GroupData> groups;      //!< このウェーブのグループ一覧
};

//----------------------------------------------------------------------------
//! @brief 縁1つ分のデータ
//----------------------------------------------------------------------------
struct BondData
{
    std::string fromId;         //!< 接続元グループID
    std::string toId;           //!< 接続先グループID
    std::string type = "Basic"; //!< 縁タイプ（"Basic", "Friends", "Love"）
};

//----------------------------------------------------------------------------
//! @brief ステージ全体のデータ
//----------------------------------------------------------------------------
struct StageData
{
    std::string name;                       //!< ステージ名
    float playerX = 640.0f;                 //!< プレイヤー初期X
    float playerY = 360.0f;                 //!< プレイヤー初期Y
    float playerHp = 100.0f;                //!< プレイヤーHP
    float playerFe = 100.0f;                //!< プレイヤーFE
    float playerSpeed = 200.0f;             //!< プレイヤー移動速度
    int maxBindCount = -1;                  //!< 結ぶ回数上限 (-1=無制限)
    int maxCutCount = -1;                   //!< 切る回数上限 (-1=無制限)
    std::vector<GroupData> groups;          //!< グループ一覧（全ウェーブ分）
    std::vector<BondData> bonds;            //!< 縁一覧
    std::vector<WaveData> waves;            //!< ウェーブ別グループ（groupsから自動生成）

    //! @brief 有効なデータか判定
    [[nodiscard]] bool IsValid() const { return !groups.empty() || !waves.empty(); }

    //! @brief グループをウェーブ別に振り分け
    void BuildWaves()
    {
        if (groups.empty()) return;

        // 最大ウェーブ番号を取得
        int maxWave = 1;
        for (const GroupData& g : groups) {
            if (g.wave > maxWave) maxWave = g.wave;
        }

        // ウェーブ配列を初期化
        waves.resize(maxWave);
        for (int i = 0; i < maxWave; ++i) {
            waves[i].waveNumber = i + 1;
        }

        // グループを振り分け
        for (const GroupData& g : groups) {
            int idx = g.wave - 1;
            if (idx >= 0 && idx < static_cast<int>(waves.size())) {
                waves[idx].groups.push_back(g);
            }
        }
    }
};
