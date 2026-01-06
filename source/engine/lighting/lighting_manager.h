//----------------------------------------------------------------------------
//! @file   lighting_manager.h
//! @brief  ライティングマネージャー
//----------------------------------------------------------------------------
#pragma once

#include "light.h"
#include "dx11/gpu/buffer.h"
#include "common/utility/non_copyable.h"
#include <memory>
#include <vector>
#include <array>

//============================================================================
//! @brief ライトID（スロットインデックス）
//============================================================================
using LightId = uint32_t;
constexpr LightId kInvalidLightId = UINT32_MAX;

//============================================================================
//! @brief ライティングマネージャー（シングルトン）
//!
//! @details シーン内のライトを一元管理し、シェーダー用の定数バッファを提供する。
//!          最大8ライトまで同時にアクティブにできる。
//!
//! @note 使用例:
//! @code
//!   // 初期化
//!   LightingManager::Get().Initialize();
//!
//!   // ライト追加
//!   auto sunLight = LightingManager::Get().AddDirectionalLight(
//!       Vector3(0.5f, -1.0f, 0.5f),
//!       Colors::White,
//!       1.0f
//!   );
//!
//!   auto pointLight = LightingManager::Get().AddPointLight(
//!       Vector3(0, 5, 0),
//!       Colors::Red,
//!       2.0f,
//!       10.0f
//!   );
//!
//!   // フレーム更新
//!   LightingManager::Get().SetCameraPosition(camera.GetPosition());
//!   LightingManager::Get().Update();
//!
//!   // シェーダーにバインド
//!   LightingManager::Get().Bind(3);  // slot b3
//!
//!   // ライト削除
//!   LightingManager::Get().RemoveLight(pointLight);
//! @endcode
//============================================================================
class LightingManager final : private NonCopyableNonMovable
{
public:
    //! シングルトンインスタンス取得
    [[nodiscard]] static LightingManager& Get();

    //! インスタンス生成
    static void Create();

    //! インスタンス破棄
    static void Destroy();

    //! デストラクタ
    ~LightingManager();

    //----------------------------------------------------------
    //! @name 初期化・終了
    //----------------------------------------------------------
    //!@{

    //! @brief 初期化
    void Initialize();

    //! 終了
    void Shutdown();

    //! 初期化済みかどうか
    [[nodiscard]] bool IsInitialized() const noexcept { return initialized_; }

    //!@}
    //----------------------------------------------------------
    //! @name ライト管理
    //----------------------------------------------------------
    //!@{

    //! @brief 平行光源を追加
    //! @param direction 光の方向（正規化される）
    //! @param color 光の色
    //! @param intensity 強度
    //! @return ライトID（失敗時kInvalidLightId）
    [[nodiscard]] LightId AddDirectionalLight(
        const Vector3& direction,
        const Color& color,
        float intensity);

    //! @brief 点光源を追加
    //! @param position 光源位置
    //! @param color 光の色
    //! @param intensity 強度
    //! @param range 有効範囲
    //! @return ライトID（失敗時kInvalidLightId）
    [[nodiscard]] LightId AddPointLight(
        const Vector3& position,
        const Color& color,
        float intensity,
        float range);

    //! @brief スポットライトを追加
    //! @param position 光源位置
    //! @param direction 光の方向（正規化される）
    //! @param color 光の色
    //! @param intensity 強度
    //! @param range 有効範囲
    //! @param innerAngleDegrees 内角（度）
    //! @param outerAngleDegrees 外角（度）
    //! @return ライトID（失敗時kInvalidLightId）
    [[nodiscard]] LightId AddSpotLight(
        const Vector3& position,
        const Vector3& direction,
        const Color& color,
        float intensity,
        float range,
        float innerAngleDegrees,
        float outerAngleDegrees);

    //! @brief ライトを削除
    //! @param lightId ライトID
    void RemoveLight(LightId lightId);

    //! @brief 全ライトを削除
    void ClearAllLights();

    //! @brief ライトデータを取得
    //! @param lightId ライトID
    //! @return ライトデータ（無効なIDの場合nullptr）
    [[nodiscard]] LightData* GetLight(LightId lightId);
    [[nodiscard]] const LightData* GetLight(LightId lightId) const;

    //! @brief アクティブライト数を取得
    [[nodiscard]] uint32_t GetActiveLightCount() const noexcept { return activeLightCount_; }

    //!@}
    //----------------------------------------------------------
    //! @name ライトパラメータ変更
    //----------------------------------------------------------
    //!@{

    //! @brief ライト位置を設定（PointLight/SpotLight用）
    void SetLightPosition(LightId lightId, const Vector3& position);

    //! @brief ライト方向を設定（DirectionalLight/SpotLight用）
    void SetLightDirection(LightId lightId, const Vector3& direction);

    //! @brief ライト色を設定
    void SetLightColor(LightId lightId, const Color& color);

    //! @brief ライト強度を設定
    void SetLightIntensity(LightId lightId, float intensity);

    //! @brief ライト有効範囲を設定（PointLight/SpotLight用）
    void SetLightRange(LightId lightId, float range);

    //! @brief ライト有効/無効を設定
    void SetLightEnabled(LightId lightId, bool enabled);

    //! @brief ライトが有効かどうか
    [[nodiscard]] bool IsLightEnabled(LightId lightId) const;

    //!@}
    //----------------------------------------------------------
    //! @name 環境設定
    //----------------------------------------------------------
    //!@{

    //! @brief 環境光色を設定
    void SetAmbientColor(const Color& color) noexcept;

    //! @brief 環境光色を取得
    [[nodiscard]] const Color& GetAmbientColor() const noexcept { return ambientColor_; }

    //! @brief カメラ位置を設定（スペキュラ計算用）
    void SetCameraPosition(const Vector3& position) noexcept;

    //! @brief カメラ位置を取得
    [[nodiscard]] const Vector3& GetCameraPosition() const noexcept { return cameraPosition_; }

    //!@}
    //----------------------------------------------------------
    //! @name 更新・バインディング
    //----------------------------------------------------------
    //!@{

    //! @brief 定数バッファを更新（フレーム毎に呼ぶ）
    void Update();

    //! @brief 定数バッファをシェーダーにバインド
    //! @param slot 定数バッファスロット（通常b3）
    void Bind(uint32_t slot = 3);

    //! @brief 定数バッファを取得
    [[nodiscard]] Buffer* GetConstantBuffer() const noexcept { return constantBuffer_.get(); }

    //! @brief ライティング定数を取得
    [[nodiscard]] const LightingConstants& GetConstants() const noexcept { return constants_; }

    //!@}

private:
    LightingManager() = default;

    static inline std::unique_ptr<LightingManager> instance_ = nullptr;

    //------------------------------------------------------------------------
    //! @name 内部構造体
    //------------------------------------------------------------------------
    //!@{

    //! @brief ライトスロット
    struct LightSlot
    {
        LightData data;         //!< ライトデータ
        bool active = false;    //!< アクティブフラグ
        bool enabled = true;    //!< 有効フラグ
    };

    //!@}
    //------------------------------------------------------------------------
    //! @name 内部ヘルパー
    //------------------------------------------------------------------------
    //!@{

    //! @brief 空きスロットを探す
    [[nodiscard]] LightId FindFreeSlot() const;

    //! @brief アクティブライト数を再計算
    void RecalculateActiveLightCount();

    //!@}
    //------------------------------------------------------------------------
    //! @name メンバ変数
    //------------------------------------------------------------------------

    bool initialized_ = false;
    bool dirty_ = true;

    // ライトスロット配列
    std::array<LightSlot, kMaxLights> lightSlots_;
    uint32_t activeLightCount_ = 0;

    // 環境設定
    Color ambientColor_ = Color(0.1f, 0.1f, 0.15f, 1.0f);  //!< デフォルト環境光
    Vector3 cameraPosition_ = Vector3::Zero;

    // 定数バッファ
    LightingConstants constants_;
    BufferPtr constantBuffer_;
};

