//----------------------------------------------------------------------------
//! @file   game_object.h
//! @brief  ゲームオブジェクトクラス
//----------------------------------------------------------------------------
#pragma once

#include "component.h"
#include <vector>
#include <memory>
#include <string>
#include <algorithm>
#include <unordered_map>

//============================================================================
//! @brief ゲームオブジェクトクラス
//!
//! コンポーネントをアタッチして機能を構築するエンティティ。
//! Transform2D、SpriteRendererなどのコンポーネントを持つ。
//============================================================================
class GameObject {
public:
    //------------------------------------------------------------------------
    //! @brief コンストラクタ
    //! @param name オブジェクト名
    //------------------------------------------------------------------------
    explicit GameObject(const std::string& name = "GameObject")
        : name_(name) {}

    ~GameObject() {
        // 全コンポーネントをデタッチ
        for (auto& comp : components_) {
            comp->OnDetach();
            comp->owner_ = nullptr;
        }
    }

    // コピー禁止、ムーブ許可
    GameObject(const GameObject&) = delete;
    GameObject& operator=(const GameObject&) = delete;
    GameObject(GameObject&&) = default;
    GameObject& operator=(GameObject&&) = default;

    //------------------------------------------------------------------------
    //! @brief コンポーネントを追加
    //! @tparam T コンポーネント型
    //! @tparam Args コンストラクタ引数の型
    //! @param args コンストラクタ引数
    //! @return 追加されたコンポーネントへのポインタ
    //------------------------------------------------------------------------
    template<typename T, typename... Args>
    T* AddComponent(Args&&... args) {
        static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

        auto component = std::make_unique<T>(std::forward<Args>(args)...);
        T* ptr = component.get();
        ptr->owner_ = this;
        components_.push_back(std::move(component));

        // 型IDでマップに登録（O(1)ルックアップ用）
        size_t typeId = GetComponentTypeId<T>();
        componentMap_[typeId].push_back(ptr);

        ptr->OnAttach();
        return ptr;
    }

    //------------------------------------------------------------------------
    //! @brief コンポーネントを取得
    //! @tparam T コンポーネント型
    //! @return コンポーネントへのポインタ（見つからない場合はnullptr）
    //------------------------------------------------------------------------
    template<typename T>
    [[nodiscard]] T* GetComponent() const {
        static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

        size_t typeId = GetComponentTypeId<T>();
        auto it = componentMap_.find(typeId);
        if (it != componentMap_.end() && !it->second.empty()) {
            return static_cast<T*>(it->second.front());
        }
        return nullptr;
    }

    //------------------------------------------------------------------------
    //! @brief 指定した型のコンポーネントを全て取得
    //! @tparam T コンポーネント型
    //! @return コンポーネントのベクター
    //------------------------------------------------------------------------
    template<typename T>
    [[nodiscard]] std::vector<T*> GetComponents() const {
        static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

        size_t typeId = GetComponentTypeId<T>();
        auto it = componentMap_.find(typeId);
        if (it != componentMap_.end()) {
            std::vector<T*> result;
            result.reserve(it->second.size());
            for (Component* comp : it->second) {
                result.push_back(static_cast<T*>(comp));
            }
            return result;
        }
        
        return {};
    }

    //------------------------------------------------------------------------
    //! @brief コンポーネントを削除
    //! @tparam T コンポーネント型
    //! @return 削除に成功した場合はtrue
    //------------------------------------------------------------------------
    template<typename T>
    bool RemoveComponent() {
        static_assert(std::is_base_of_v<Component, T>, "T must derive from Component");

        size_t typeId = GetComponentTypeId<T>();
        auto mapIt = componentMap_.find(typeId);
        if (mapIt == componentMap_.end() || mapIt->second.empty()) {
            return false;
        }

        // マップから最初のコンポーネントを取得
        Component* toRemove = mapIt->second.front();
        mapIt->second.erase(mapIt->second.begin());

        // 所有権リストから削除
        auto it = std::find_if(components_.begin(), components_.end(),
            [toRemove](const auto& comp) {
                return comp.get() == toRemove;
            });

        if (it != components_.end()) {
            (*it)->OnDetach();
            (*it)->owner_ = nullptr;
            components_.erase(it);
            return true;
        }
        return false;
    }

    //------------------------------------------------------------------------
    //! @brief 全コンポーネントを更新
    //! @param deltaTime 前フレームからの経過時間（秒）
    //------------------------------------------------------------------------
    void Update(float deltaTime) {
        if (!active_) return;

        for (auto& comp : components_) {
            if (comp->IsEnabled()) {
                comp->Update(deltaTime);
            }
        }
    }

    //------------------------------------------------------------------------
    // アクセサ
    //------------------------------------------------------------------------

    [[nodiscard]] const std::string& GetName() const noexcept { return name_; }
    void SetName(const std::string& name) { name_ = name; }

    [[nodiscard]] bool IsActive() const noexcept { return active_; }
    void SetActive(bool active) noexcept { active_ = active; }

    [[nodiscard]] int GetLayer() const noexcept { return layer_; }
    void SetLayer(int layer) noexcept { layer_ = layer; }

private:
    std::string name_;
    std::vector<std::unique_ptr<Component>> components_;          //!< コンポーネント所有権
    std::unordered_map<size_t, std::vector<Component*>> componentMap_;  //!< 型ID→コンポーネント（O(1)ルックアップ用）
    bool active_ = true;
    int layer_ = 0;  //!< 描画/更新の優先度
};
