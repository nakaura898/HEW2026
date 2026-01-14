//----------------------------------------------------------------------------
//! @file   arrow_manager.cpp
//! @brief  矢マネージャー実装
//----------------------------------------------------------------------------
#include "arrow_manager.h"
#include "individual.h"
#include "player.h"
#include "engine/c_systems/sprite_batch.h"
#include "common/logging/logging.h"
#include <algorithm>

//----------------------------------------------------------------------------
ArrowManager& ArrowManager::Get()
{
    assert(instance_ && "ArrowManager::Create() not called");
    return *instance_;
}

//----------------------------------------------------------------------------
void ArrowManager::Create()
{
    if (!instance_) {
        instance_.reset(new ArrowManager());
    }
}

//----------------------------------------------------------------------------
void ArrowManager::Destroy()
{
    instance_.reset();
}

//----------------------------------------------------------------------------
void ArrowManager::Shoot(Individual* owner, Individual* target, const Vector2& startPos, float damage)
{
    if (!owner) {
        LOG_WARN("[ArrowManager] BUG: Shoot called with null owner");
        return;
    }
    if (!target) {
        LOG_WARN("[ArrowManager] BUG: Shoot called with null target");
        return;
    }
    if (!owner->IsAlive()) {
        LOG_WARN("[ArrowManager] BUG: Shoot called with dead owner: " + owner->GetId());
        return;
    }
    if (!target->IsAlive()) {
        LOG_WARN("[ArrowManager] BUG: Shoot called with dead target: " + target->GetId());
        return;
    }
    if (damage < 0.0f) {
        LOG_WARN("[ArrowManager] BUG: Negative damage: " + std::to_string(damage));
        return;
    }

    std::unique_ptr<Arrow> arrow = std::make_unique<Arrow>(owner, target, damage);
    arrow->Initialize(startPos, target->GetPosition());
    arrows_.push_back(std::move(arrow));
}

//----------------------------------------------------------------------------
void ArrowManager::ShootAtPlayer(Individual* owner, Player* targetPlayer, const Vector2& startPos, float damage)
{
    if (!owner) {
        LOG_WARN("[ArrowManager] BUG: ShootAtPlayer called with null owner");
        return;
    }
    if (!targetPlayer) {
        LOG_WARN("[ArrowManager] BUG: ShootAtPlayer called with null targetPlayer");
        return;
    }
    if (!owner->IsAlive()) {
        LOG_WARN("[ArrowManager] BUG: ShootAtPlayer called with dead owner: " + owner->GetId());
        return;
    }
    if (!targetPlayer->IsAlive()) {
        LOG_WARN("[ArrowManager] BUG: ShootAtPlayer called with dead player");
        return;
    }
    if (damage < 0.0f) {
        LOG_WARN("[ArrowManager] BUG: Negative damage: " + std::to_string(damage));
        return;
    }

    std::unique_ptr<Arrow> arrow = std::make_unique<Arrow>(owner, targetPlayer, damage);
    arrow->Initialize(startPos, targetPlayer->GetPosition());
    arrows_.push_back(std::move(arrow));
}

//----------------------------------------------------------------------------
void ArrowManager::Update(float dt)
{
    // 全矢を更新
    for (std::unique_ptr<Arrow>& arrow : arrows_) {
        arrow->Update(dt);
    }

    // 無効な矢を削除
    RemoveInactiveArrows();
}

//----------------------------------------------------------------------------
void ArrowManager::Render(SpriteBatch& spriteBatch)
{
    for (std::unique_ptr<Arrow>& arrow : arrows_) {
        arrow->Render(spriteBatch);
    }
}

//----------------------------------------------------------------------------
void ArrowManager::Clear()
{
    arrows_.clear();
}

//----------------------------------------------------------------------------
void ArrowManager::RemoveInactiveArrows()
{
    arrows_.erase(
        std::remove_if(arrows_.begin(), arrows_.end(),
            [](const std::unique_ptr<Arrow>& arrow) {
                return !arrow->IsActive();
            }),
        arrows_.end()
    );
}
