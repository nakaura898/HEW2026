//----------------------------------------------------------------------------
//! @file   arrow_manager.cpp
//! @brief  矢マネージャー実装
//----------------------------------------------------------------------------
#include "arrow_manager.h"
#include "individual.h"
#include "player.h"
#include "engine/c_systems/sprite_batch.h"
#include <algorithm>

//----------------------------------------------------------------------------
ArrowManager& ArrowManager::Get()
{
    static ArrowManager instance;
    return instance;
}

//----------------------------------------------------------------------------
void ArrowManager::Shoot(Individual* owner, Individual* target, const Vector2& startPos, float damage)
{
    if (!owner || !target) return;

    std::unique_ptr<Arrow> arrow = std::make_unique<Arrow>(owner, target, damage);
    arrow->Initialize(startPos, target->GetPosition());
    arrows_.push_back(std::move(arrow));
}

//----------------------------------------------------------------------------
void ArrowManager::ShootAtPlayer(Individual* owner, Player* targetPlayer, const Vector2& startPos, float damage)
{
    if (!owner || !targetPlayer) return;

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
