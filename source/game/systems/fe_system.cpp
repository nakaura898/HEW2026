//----------------------------------------------------------------------------
//! @file   fe_system.cpp
//! @brief  FEシステム実装
//----------------------------------------------------------------------------
#include "fe_system.h"
#include "game/entities/player.h"
#include "common/logging/logging.h"

//----------------------------------------------------------------------------
FESystem& FESystem::Get()
{
    static FESystem instance;
    return instance;
}

//----------------------------------------------------------------------------
bool FESystem::CanConsume(float amount) const
{
    if (!player_) return false;
    return player_->HasEnoughFe(amount);
}

//----------------------------------------------------------------------------
bool FESystem::Consume(float amount)
{
    if (!player_) return false;

    if (!player_->HasEnoughFe(amount)) {
        LOG_WARN("[FESystem] Not enough FE: need " + std::to_string(amount) +
                 ", have " + std::to_string(player_->GetFe()));
        return false;
    }

    float before = player_->GetFe();
    player_->ConsumeFe(amount);
    float after = player_->GetFe();

    LOG_INFO("[FESystem] Consumed " + std::to_string(amount) +
             " FE (" + std::to_string(before) + " -> " + std::to_string(after) + ")");

    if (onFEChanged_) {
        onFEChanged_(after, player_->GetMaxFe(), -amount);
    }

    return true;
}

//----------------------------------------------------------------------------
void FESystem::Recover(float amount)
{
    if (!player_) return;

    float before = player_->GetFe();
    player_->RecoverFe(amount);
    float after = player_->GetFe();
    float actualRecovery = after - before;

    if (actualRecovery > 0.0f) {
        LOG_INFO("[FESystem] Recovered " + std::to_string(actualRecovery) +
                 " FE (" + std::to_string(before) + " -> " + std::to_string(after) + ")");

        if (onFEChanged_) {
            onFEChanged_(after, player_->GetMaxFe(), actualRecovery);
        }
    }
}

//----------------------------------------------------------------------------
float FESystem::GetCurrentFE() const
{
    return player_ ? player_->GetFe() : 0.0f;
}

//----------------------------------------------------------------------------
float FESystem::GetMaxFE() const
{
    return player_ ? player_->GetMaxFe() : 0.0f;
}

//----------------------------------------------------------------------------
float FESystem::GetFERatio() const
{
    return player_ ? player_->GetFeRatio() : 0.0f;
}
