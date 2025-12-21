//----------------------------------------------------------------------------
//! @file   group.cpp
//! @brief  Groupクラス実装
//----------------------------------------------------------------------------
#include "group.h"
#include "game/systems/stagger_system.h"
#include "engine/c_systems/sprite_batch.h"
#include "common/logging/logging.h"
#include <random>
#include <algorithm>

//----------------------------------------------------------------------------
Group::Group(const std::string& id)
    : id_(id)
{
}

//----------------------------------------------------------------------------
Group::~Group()
{
    Shutdown();
}

//----------------------------------------------------------------------------
void Group::Initialize(const Vector2& centerPosition)
{
    // Formationを初期化
    std::vector<Individual*> individuals = GetAliveIndividuals();
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
    if (!individual) return;

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
    Vector2 currentCenter = GetPosition();
    Vector2 delta = Vector2(position.x - currentCenter.x, position.y - currentCenter.y);

    // 全個体を相対移動
    for (std::unique_ptr<Individual>& individual : individuals_) {
        if (individual && individual->IsAlive()) {
            Vector2 pos = individual->GetPosition();
            individual->SetPosition(Vector2(pos.x + delta.x, pos.y + delta.y));
        }
    }
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
void Group::CheckDefeated()
{
    if (isDefeated_) return;

    if (GetAliveCount() == 0) {
        isDefeated_ = true;
        LOG_INFO("[Group] " + id_ + " defeated!");

        // コールバック呼び出し
        if (onDefeated_) {
            onDefeated_(this);
        }
    }
}
