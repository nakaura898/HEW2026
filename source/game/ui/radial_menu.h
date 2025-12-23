//----------------------------------------------------------------------------
//! @file   radial_menu.h
//! @brief  ラジアルメニュー - 円形選択UI
//----------------------------------------------------------------------------
#pragma once

#include "engine/math/math_types.h"
#include "engine/math/color.h"
#include "game/bond/bond.h"
#include <vector>
#include <string>
#include <functional>

// 前方宣言
class SpriteBatch;

//----------------------------------------------------------------------------
//! @brief ラジアルメニューの選択肢
//----------------------------------------------------------------------------
struct RadialMenuItem
{
    std::string label;      //!< 表示ラベル
    BondType bondType;      //!< 対応する縁タイプ
    Color color;            //!< 表示色
};

//----------------------------------------------------------------------------
//! @brief ラジアルメニュー（縁タイプ選択用）
//----------------------------------------------------------------------------
class RadialMenu
{
public:
    //! @brief シングルトンインスタンス取得
    static RadialMenu& Get();

    //! @brief 初期化
    void Initialize();

    //! @brief メニューを開く
    //! @param centerPos メニュー中心位置（スクリーン座標）
    void Open(const Vector2& centerPos);

    //! @brief メニューを閉じる
    void Close();

    //! @brief 更新
    //! @param cursorPos カーソル位置（スクリーン座標）
    void Update(const Vector2& cursorPos);

    //! @brief 描画
    //! @param spriteBatch SpriteBatch参照
    void Render(SpriteBatch& spriteBatch);

    //! @brief メニューが開いているか
    [[nodiscard]] bool IsOpen() const { return isOpen_; }

    //! @brief 現在ホバー中のアイテムインデックス取得
    //! @return ホバー中のインデックス（-1 = なし）
    [[nodiscard]] int GetHoveredIndex() const { return hoveredIndex_; }

    //! @brief 現在ホバー中の縁タイプ取得
    //! @return ホバー中の縁タイプ（ホバーなしの場合はBasic）
    [[nodiscard]] BondType GetHoveredBondType() const;

    //! @brief 選択確定
    //! @return 選択された縁タイプ
    [[nodiscard]] BondType Confirm();

    //! @brief 選択確定時コールバック設定
    void SetOnSelected(std::function<void(BondType)> callback) { onSelected_ = callback; }

    //! @brief メニュー中心位置を設定（カメラ追従用）
    void SetCenter(const Vector2& center) { centerPos_ = center; }

    //! @brief メニュー半径設定
    void SetRadius(float radius) { radius_ = radius; }

    //! @brief デッドゾーン半径設定（中心からこの距離以内は選択なし）
    void SetDeadZone(float deadZone) { deadZone_ = deadZone; }

private:
    RadialMenu() = default;
    ~RadialMenu() = default;
    RadialMenu(const RadialMenu&) = delete;
    RadialMenu& operator=(const RadialMenu&) = delete;

    //! @brief カーソル位置からホバー中のアイテムを計算
    void UpdateHoveredItem(const Vector2& cursorPos);

    // 状態
    bool isOpen_ = false;
    Vector2 centerPos_ = Vector2::Zero;
    int hoveredIndex_ = -1;

    // メニュー項目
    std::vector<RadialMenuItem> items_;

    // パラメータ
    float radius_ = 250.0f;     //!< メニュー半径（大きめ）
    float deadZone_ = 60.0f;    //!< 中心のデッドゾーン

    // コールバック
    std::function<void(BondType)> onSelected_;
};
