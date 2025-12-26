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
    std::vector<GroupData> groups;          //!< グループ一覧
    std::vector<BondData> bonds;            //!< 縁一覧

    //! @brief 有効なデータか判定
    [[nodiscard]] bool IsValid() const { return !groups.empty(); }
};
