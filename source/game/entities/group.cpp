//----------------------------------------------------------------------------
//! @file   group.cpp
//! @brief  Groupクラス実装
//----------------------------------------------------------------------------
#include "group.h"
#include "game/ai/group_ai.h"
#include "game/systems/stagger_system.h"
#include "engine/event/event_bus.h"
#include "game/systems/event/game_events.h"
#include "game/bond/bond_manager.h"
#include "game/bond/bond.h"
#include "engine/c_systems/sprite_batch.h"
#include "common/logging/logging.h"
#include <random>
#include <algorithm>
#include <cmath>

//----------------------------------------------------------------------------
Group::Group(const std::string& id)
    : id_(id)
{
    // IndividualDiedEventを購読（所属個体死亡時にFormation再構築）
    individualDiedSubscriptionId_ = EventBus::Get().Subscribe<IndividualDiedEvent>(
        [this](const IndividualDiedEvent& e) {
            OnIndividualDied(e.individual, e.ownerGroup);
        });
}

//----------------------------------------------------------------------------
Group::~Group()
{
    Shutdown();
}

//----------------------------------------------------------------------------
void Group::Initialize(const Vector2& centerPosition)
{
    if (individuals_.empty()) {
        LOG_WARN("[Group] BUG: Initialize called with no individuals: " + id_);
        return;
    }

    // Formationを初期化
    std::vector<Individual*> individuals = GetAliveIndividuals();

    if (individuals.empty()) {
        LOG_WARN("[Group] BUG: Initialize called but all individuals are dead: " + id_);
        return;
    }

    formation_.Initialize(individuals, centerPosition);

    // 個体を初期位置に配置
    for (Individual* individual : individuals) {
        Vector2 slotPos = formation_.GetSlotPosition(individual);
        individual->SetPosition(slotPos);
    }

    LOG_INFO("[Group] " + id_ + " initialized with " + std::to_string(individuals.size()) + " individuals");
}

//----------------------------------------------------------------------------
void Group::Shutdown()
{
    // イベント購読を解除
    if (individualDiedSubscriptionId_ != 0) {
        EventBus::Get().Unsubscribe<IndividualDiedEvent>(individualDiedSubscriptionId_);
        individualDiedSubscriptionId_ = 0;
    }

    individuals_.clear();
    isDefeated_ = false;
}

//----------------------------------------------------------------------------
void Group::Update(float dt)
{
    // 硬直中は移動しない（速度をリセット）
    bool isStaggered = StaggerSystem::Get().IsStaggered(this);
    if (isStaggered) {
        for (std::unique_ptr<Individual>& individual : individuals_) {
            if (individual && individual->IsAlive()) {
                individual->SetDesiredVelocity(Vector2(0.0f, 0.0f));
            }
        }
    }

    // 生存個体リストを取得（分離計算用）
    std::vector<Individual*> aliveIndividuals = GetAliveIndividuals();

    // 分離オフセット計算（位置更新前に全個体分を計算）
    for (Individual* individual : aliveIndividuals) {
        individual->CalculateSeparation(aliveIndividuals);
    }

    // 全個体を更新
    for (std::unique_ptr<Individual>& individual : individuals_) {
        if (individual && individual->IsAlive()) {
            individual->Update(dt);
        }
    }

    // 全滅チェック
    CheckDefeated();
}

//----------------------------------------------------------------------------
void Group::Render(SpriteBatch& spriteBatch)
{
    // 全個体を描画
    for (std::unique_ptr<Individual>& individual : individuals_) {
        if (individual && individual->IsAlive()) {
            individual->Render(spriteBatch);
        }
    }
}

//----------------------------------------------------------------------------
void Group::AddIndividual(std::unique_ptr<Individual> individual)
{
    if (!individual) {
        LOG_WARN("[Group] BUG: AddIndividual called with null individual");
        return;
    }

    if (!individual->IsAlive()) {
        LOG_WARN("[Group] BUG: AddIndividual called with dead individual: " + individual->GetId());
        return;
    }

    individual->SetOwnerGroup(this);
    individuals_.push_back(std::move(individual));

    LOG_INFO("[Group] " + id_ + " added individual, count: " + std::to_string(individuals_.size()));
}

//----------------------------------------------------------------------------
std::vector<Individual*> Group::GetAliveIndividuals() const
{
    std::vector<Individual*> alive;
    for (const std::unique_ptr<Individual>& individual : individuals_) {
        if (individual && individual->IsAlive()) {
            alive.push_back(individual.get());
        }
    }
    return alive;
}

//----------------------------------------------------------------------------
Individual* Group::GetRandomAliveIndividual() const
{
    std::vector<Individual*> alive = GetAliveIndividuals();
    if (alive.empty()) return nullptr;

    // ランダムに選択
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<size_t> dist(0, alive.size() - 1);

    return alive[dist(gen)];
}

//----------------------------------------------------------------------------
size_t Group::GetAliveCount() const
{
    size_t count = 0;
    for (const std::unique_ptr<Individual>& individual : individuals_) {
        if (individual && individual->IsAlive()) {
            ++count;
        }
    }
    return count;
}

//----------------------------------------------------------------------------
Vector2 Group::GetPosition() const
{
    std::vector<Individual*> alive = GetAliveIndividuals();
    if (alive.empty()) {
        return Vector2::Zero;
    }

    // 全生存個体の平均位置を計算
    Vector2 sum = Vector2::Zero;
    for (Individual* individual : alive) {
        Vector2 pos = individual->GetPosition();
        sum.x += pos.x;
        sum.y += pos.y;
    }

    float count = static_cast<float>(alive.size());
    return Vector2(sum.x / count, sum.y / count);
}

//----------------------------------------------------------------------------
void Group::SetPosition(const Vector2& position)
{
    Vector2 targetPos = position;

    // Love追従はGroupAI::UpdateWanderで処理するため、ここでは制約をかけない

    Vector2 currentCenter = GetPosition();
    Vector2 delta = Vector2(targetPos.x - currentCenter.x, targetPos.y - currentCenter.y);

    // 全個体を相対移動
    for (std::unique_ptr<Individual>& individual : individuals_) {
        if (individual && individual->IsAlive()) {
            Vector2 pos = individual->GetPosition();
            individual->SetPosition(Vector2(pos.x + delta.x, pos.y + delta.y));
        }
    }

    // Formationの中心位置も更新
    formation_.SetCenter(targetPos);
}

//----------------------------------------------------------------------------
float Group::GetHpRatio() const
{
    float totalHp = 0.0f;
    float totalMaxHp = 0.0f;

    for (const std::unique_ptr<Individual>& individual : individuals_) {
        if (individual) {
            totalHp += individual->GetHp();
            totalMaxHp += individual->GetMaxHp();
        }
    }

    if (totalMaxHp <= 0.0f) return 0.0f;
    return totalHp / totalMaxHp;
}

//----------------------------------------------------------------------------
bool Group::IsDefeated() const
{
    return isDefeated_ || GetAliveCount() == 0;
}

//----------------------------------------------------------------------------
void Group::SetThreatModifier(float modifier)
{
    if (threatModifier_ != modifier) {
        threatModifier_ = modifier;

        // コールバック呼び出し
        if (onThreatChanged_) {
            onThreatChanged_(this);
        }
    }
}

//----------------------------------------------------------------------------
float Group::GetMaxAttackRange() const
{
    float maxRange = 0.0f;
    for (const std::unique_ptr<Individual>& individual : individuals_) {
        if (individual && individual->IsAlive()) {
            float range = individual->GetAttackRange();
            if (range > maxRange) {
                maxRange = range;
            }
        }
    }
    return maxRange;
}

//----------------------------------------------------------------------------
void Group::RebuildFormation()
{
    std::vector<Individual*> alive = GetAliveIndividuals();
    formation_.Rebuild(alive);
}

//----------------------------------------------------------------------------
void Group::ResetOnBond()
{
    if (IsDefeated()) {
        LOG_WARN("[Group] BUG: ResetOnBond called on defeated group: " + id_);
        return;
    }

    // AIをWander状態に戻す + 戦闘状態をリセット
    if (ai_) {
        ai_->SetState(AIState::Wander);
        ai_->ClearTarget();
        ai_->ExitCombat();  // inCombat_をfalseに
    }

    // 個体の状態をリセット（攻撃中断とアニメーションロック解除含む）
    for (auto& individual : individuals_) {
        if (individual && individual->IsAlive()) {
            individual->InterruptAttack();
            individual->SetGroupMoving(false);
        }
    }

    LOG_INFO("[Group] " + id_ + " reset on bond");
}

//----------------------------------------------------------------------------
void Group::CheckDefeated()
{
    if (isDefeated_) return;

    if (GetAliveCount() == 0) {
        isDefeated_ = true;
        
        // このグループに関連する縁の状況をログ出力
        LOG_INFO("[Group] === " + id_ + " Defeated ===");
        
        BondableEntity thisEntity = this;
        std::vector<Bond*> bonds = BondManager::Get().GetBondsFor(thisEntity);
        
        if (bonds.empty()) {
            LOG_INFO("[Group]   No bonds");
        } else {
            LOG_INFO("[Group]   Related bonds (" + std::to_string(bonds.size()) + "):");
            for (Bond* bond : bonds) {
                std::string typeName;
                switch (bond->GetType()) {
                    case BondType::Friends: typeName = "Friends"; break;
                    case BondType::Love: typeName = "Love"; break;
                    default: typeName = "Unknown"; break;
                }
                BondableEntity other = bond->GetOther(thisEntity);
                LOG_INFO("[Group]     - " + id_ + " <-[" + typeName + "]-> " + BondableHelper::GetId(other));
            }
        }

        // コールバック呼び出し
        if (onDefeated_) {
            onDefeated_(this);
        }
    }
}

//----------------------------------------------------------------------------
void Group::OnIndividualDied([[maybe_unused]] Individual* individual, Group* ownerGroup)
{
    // nullチェック + 自分のグループの個体が死亡した場合のみ処理
    if (ownerGroup == nullptr || ownerGroup != this) return;

    LOG_INFO("[Group] " + id_ + " individual died, rebuilding formation");

    // Formationを再構築
    RebuildFormation();
}
