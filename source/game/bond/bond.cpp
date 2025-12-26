//----------------------------------------------------------------------------
//! @file   bond.cpp
//! @brief  縁クラス実装
//----------------------------------------------------------------------------
#include "bond.h"

//----------------------------------------------------------------------------
Bond::Bond(BondableEntity a, BondableEntity b, BondType type)
    : entityA_(a)
    , entityB_(b)
    , type_(type)
{
}

//----------------------------------------------------------------------------
bool Bond::Involves(const BondableEntity& entity) const
{
    return BondableHelper::IsSame(entityA_, entity) ||
           BondableHelper::IsSame(entityB_, entity);
}

//----------------------------------------------------------------------------
BondableEntity Bond::GetOther(const BondableEntity& entity) const
{
    if (BondableHelper::IsSame(entityA_, entity)) {
        return entityB_;
    }
    if (BondableHelper::IsSame(entityB_, entity)) {
        return entityA_;
    }
    // 関与していない場合はnullptrを返す
    return BondableEntity{static_cast<Player*>(nullptr)};
}

//----------------------------------------------------------------------------
bool Bond::Connects(const BondableEntity& a, const BondableEntity& b) const
{
    return (BondableHelper::IsSame(entityA_, a) && BondableHelper::IsSame(entityB_, b)) ||
           (BondableHelper::IsSame(entityA_, b) && BondableHelper::IsSame(entityB_, a));
}

//----------------------------------------------------------------------------
float Bond::GetMaxDistance(BondType type)
{
    switch (type) {
        case BondType::Love:
            return 300.0f;  // ラブ縁は300まで
        case BondType::Basic:
        case BondType::Friends:
        default:
            return 0.0f;    // 無制限
    }
}
