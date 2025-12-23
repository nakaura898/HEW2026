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
//----------------------------------------------------------------------------
template<typename TEvent>
class EventHandler : public IEventHandler
{
public:
    using CallbackType = std::function<void(const TEvent&)>;

    void Add(uint32_t id, CallbackType callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        callbacks_[id] = std::move(callback);
    }

    void Remove(uint32_t id) {
        std::lock_guard<std::mutex> lock(mutex_);
        callbacks_.erase(id);
    }

    void Invoke(const TEvent& event) {
        // コールバック中の変更に備えてコピー（mutex保護下）
        std::unordered_map<uint32_t, CallbackType> callbacksCopy;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            callbacksCopy = callbacks_;
        }
        for (auto& [id, callback] : callbacksCopy) {
            callback(event);
        }
    }

    [[nodiscard]] bool IsEmpty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return callbacks_.empty();
    }

private:
    std::unordered_map<uint32_t, CallbackType> callbacks_;
    mutable std::mutex mutex_;
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
        static EventBus instance;
        return instance;
    }

    //------------------------------------------------------------------------
    // 購読
    //------------------------------------------------------------------------

    //! @brief イベントを購読
    //! @tparam TEvent イベント型
    //! @param callback コールバック関数
    //! @return 購読ID（解除時に使用）
    template<typename TEvent>
    uint32_t Subscribe(std::function<void(const TEvent&)> callback) {
        std::lock_guard<std::mutex> lock(mutex_);
        uint32_t id = nextSubscriptionId_++;
        GetOrCreateHandlerLocked<TEvent>()->Add(id, std::move(callback));
        return id;
    }

    //! @brief イベント購読を解除
    //! @tparam TEvent イベント型
    //! @param subscriptionId 購読ID
    template<typename TEvent>
    void Unsubscribe(uint32_t subscriptionId) {
        std::lock_guard<std::mutex> lock(mutex_);
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
            std::lock_guard<std::mutex> lock(mutex_);
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
        std::lock_guard<std::mutex> lock(mutex_);
        handlers_.clear();
    }

private:
    EventBus() = default;
    ~EventBus() = default;
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

    std::unordered_map<std::type_index, std::unique_ptr<IEventHandler>> handlers_;
    mutable std::mutex mutex_;
    uint32_t nextSubscriptionId_ = 1;
};
