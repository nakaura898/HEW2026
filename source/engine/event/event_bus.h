//----------------------------------------------------------------------------
//! @file   event_bus.h
//! @brief  EventBus - 型安全なイベント通信システム
//----------------------------------------------------------------------------
#pragma once

#include <functional>
#include <unordered_map>
#include <vector>
#include <typeindex>
#include <memory>
#include <cstdint>
#include <mutex>
#include <shared_mutex>
#include <cassert>
#include <algorithm>

//----------------------------------------------------------------------------
//! @brief イベント優先度
//----------------------------------------------------------------------------
enum class EventPriority : uint8_t {
    High = 0,    //!< 高優先度（システム処理など）
    Normal = 1,  //!< 通常
    Low = 2      //!< 低優先度（UI更新など）
};

//----------------------------------------------------------------------------
//! @brief イベントハンドラの基底クラス
//----------------------------------------------------------------------------
class IEventHandler
{
public:
    virtual ~IEventHandler() = default;
};

//----------------------------------------------------------------------------
//! @brief 型付きイベントハンドラ
//! @details shared_mutexを使用して読み取り並行性を向上
//!          優先度付きコールバックをサポート
//----------------------------------------------------------------------------
template<typename TEvent>
class EventHandler : public IEventHandler
{
public:
    using CallbackType = std::function<void(const TEvent&)>;

    //! @brief コールバックエントリ（優先度付き）
    struct CallbackEntry {
        uint32_t id;
        CallbackType callback;
        EventPriority priority;

        bool operator<(const CallbackEntry& other) const noexcept {
            return priority < other.priority;  // 優先度昇順（High=0が先）
        }
    };

    void Add(uint32_t id, CallbackType callback, EventPriority priority = EventPriority::Normal) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        callbacks_.push_back({id, std::move(callback), priority});
        sorted_ = false;
    }

    void Remove(uint32_t id) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        callbacks_.erase(
            std::remove_if(callbacks_.begin(), callbacks_.end(),
                [id](const CallbackEntry& e) { return e.id == id; }),
            callbacks_.end());
    }

    void Invoke(const TEvent& event) {
        std::vector<CallbackEntry> callbacksCopy;
        {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            // 必要に応じてソート
            if (!sorted_) {
                std::stable_sort(callbacks_.begin(), callbacks_.end());
                sorted_ = true;
            }
            callbacksCopy = callbacks_;
        }
        // ロック解放後にコールバック実行（再入可能）
        for (auto& entry : callbacksCopy) {
            entry.callback(event);
        }
    }

    [[nodiscard]] bool IsEmpty() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return callbacks_.empty();
    }

private:
    std::vector<CallbackEntry> callbacks_;
    mutable std::shared_mutex mutex_;
    mutable bool sorted_ = true;
};

//----------------------------------------------------------------------------
//! @brief EventBus - システム間イベント通信
//! @details 型安全なPublish/Subscribeパターンを提供
//----------------------------------------------------------------------------
class EventBus
{
public:
    //! @brief シングルトン取得
    static EventBus& Get() {
        assert(instance_ && "EventBus::Create() not called");
        return *instance_;
    }

    //! @brief インスタンス生成（スレッドセーフ）
    static void Create() {
        std::call_once(initFlag_, []() {
            instance_.reset(new EventBus());
        });
    }

    //! @brief インスタンス破棄
    static void Destroy() {
        instance_.reset();
    }

    //! @brief デストラクタ
    ~EventBus() = default;

    //------------------------------------------------------------------------
    // 購読
    //------------------------------------------------------------------------

    //! @brief イベントを購読
    //! @tparam TEvent イベント型
    //! @param callback コールバック関数
    //! @param priority 優先度（デフォルト: Normal）
    //! @return 購読ID（解除時に使用）
    template<typename TEvent>
    uint32_t Subscribe(std::function<void(const TEvent&)> callback,
                       EventPriority priority = EventPriority::Normal) {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        uint32_t id = nextSubscriptionId_++;
        GetOrCreateHandlerLocked<TEvent>()->Add(id, std::move(callback), priority);
        return id;
    }

    //! @brief イベント購読を解除
    //! @tparam TEvent イベント型
    //! @param subscriptionId 購読ID
    template<typename TEvent>
    void Unsubscribe(uint32_t subscriptionId) {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        auto* handler = GetHandlerLocked<TEvent>();
        if (handler) {
            handler->Remove(subscriptionId);
        }
    }

    //------------------------------------------------------------------------
    // 発行
    //------------------------------------------------------------------------

    //! @brief イベントを発行
    //! @tparam TEvent イベント型
    //! @param event イベントデータ
    template<typename TEvent>
    void Publish(const TEvent& event) {
        EventHandler<TEvent>* handler = nullptr;
        {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            handler = GetHandlerLocked<TEvent>();
        }
        if (handler) {
            handler->Invoke(event);
        }
    }

    //! @brief イベントを発行（引数から構築）
    //! @tparam TEvent イベント型
    //! @tparam Args コンストラクタ引数
    template<typename TEvent, typename... Args>
    void Publish(Args&&... args) {
        TEvent event(std::forward<Args>(args)...);
        Publish(event);
    }

    //------------------------------------------------------------------------
    // 管理
    //------------------------------------------------------------------------

    //! @brief 全購読をクリア
    void Clear() {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        handlers_.clear();
    }

private:
    EventBus() = default;
    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    //! @note mutex_を保持した状態で呼び出すこと
    template<typename TEvent>
    EventHandler<TEvent>* GetHandlerLocked() {
        std::type_index typeIndex(typeid(TEvent));
        auto it = handlers_.find(typeIndex);
        if (it == handlers_.end()) {
            return nullptr;
        }
        return static_cast<EventHandler<TEvent>*>(it->second.get());
    }

    //! @note mutex_を保持した状態で呼び出すこと
    template<typename TEvent>
    EventHandler<TEvent>* GetOrCreateHandlerLocked() {
        std::type_index typeIndex(typeid(TEvent));
        auto it = handlers_.find(typeIndex);
        if (it == handlers_.end()) {
            auto handler = std::make_unique<EventHandler<TEvent>>();
            auto* ptr = handler.get();
            handlers_[typeIndex] = std::move(handler);
            return ptr;
        }
        return static_cast<EventHandler<TEvent>*>(it->second.get());
    }

    static inline std::unique_ptr<EventBus> instance_ = nullptr;
    static inline std::once_flag initFlag_;

    std::unordered_map<std::type_index, std::unique_ptr<IEventHandler>> handlers_;
    mutable std::shared_mutex mutex_;
    uint32_t nextSubscriptionId_ = 1;
};
