//----------------------------------------------------------------------------
//! @file   player.cpp
//! @brief  Playerクラス実装
//----------------------------------------------------------------------------
#include "player.h"
#include "group.h"
#include "individual.h"
#include "game/bond/bond_manager.h"
#include "game/bond/bond.h"
#include "engine/input/input_manager.h"
#include "engine/texture/texture_manager.h"
#include "engine/c_systems/sprite_batch.h"
#include "engine/c_systems/collision_layers.h"
#include "game/systems/game_constants.h"
#include "common/logging/logging.h"

//----------------------------------------------------------------------------
Player::Player()
{
}

//----------------------------------------------------------------------------
Player::~Player()
{
    Shutdown();
}

//----------------------------------------------------------------------------
void Player::Initialize(const Vector2& position)
{
    // テクスチャロード
    texture_ = TextureManager::Get().LoadTexture2D("player.png");

    // GameObject作成
    gameObject_ = std::make_unique<GameObject>("Player");

    // Transform
    transform_ = gameObject_->AddComponent<Transform2D>();
    transform_->SetPosition(position);
    transform_->SetScale(0.3f);

    // SpriteRenderer
    sprite_ = gameObject_->AddComponent<SpriteRenderer>();
    sprite_->SetTexture(texture_.get());
    sprite_->SetSortingLayer(20);  // 他より手前

    // Pivot設定（テクスチャ全体を使用、中心基点）
    if (texture_) {
        float texWidth = static_cast<float>(texture_->Width());
        float texHeight = static_cast<float>(texture_->Height());
        sprite_->SetPivotFromCenter(texWidth, texHeight, 0.0f, 0.0f);
    }

    // Animatorなし（静止画1枚のみ）
    animator_ = nullptr;

    // Collider
    collider_ = gameObject_->AddComponent<Collider2D>();
    collider_->SetBounds(Vector2(-20, -30), Vector2(20, 30));
    collider_->SetLayer(CollisionLayer::Player);
    collider_->SetMask(CollisionLayer::PlayerMask);

    LOG_INFO("[Player] Initialized");
}

//----------------------------------------------------------------------------
void Player::Shutdown()
{
    gameObject_.reset();
    transform_ = nullptr;
    sprite_ = nullptr;
    animator_ = nullptr;
    collider_ = nullptr;
    texture_.reset();
}

//----------------------------------------------------------------------------
void Player::Update(float dt, Camera2D& camera)
{
    HandleInput(dt, camera);

    if (gameObject_) {
        gameObject_->Update(dt);
    }
}

//----------------------------------------------------------------------------
void Player::HandleInput(float dt, Camera2D& /*camera*/)
{
    InputManager* inputMgr = InputManager::GetInstance();
    if (!inputMgr || !transform_) return;

    Keyboard& keyboard = inputMgr->GetKeyboard();

    // 移動入力
    Vector2 move = Vector2::Zero;
    if (keyboard.IsKeyPressed(Key::W)) move.y -= moveSpeed_ * dt;
    if (keyboard.IsKeyPressed(Key::S)) move.y += moveSpeed_ * dt;
    if (keyboard.IsKeyPressed(Key::A)) move.x -= moveSpeed_ * dt;
    if (keyboard.IsKeyPressed(Key::D)) move.x += moveSpeed_ * dt;

    // 移動処理
    isMoving_ = (move.x != 0.0f || move.y != 0.0f);

    if (isMoving_) {
        transform_->Translate(move);

        // 移動方向に応じてスプライト反転（テクスチャは左向き）
        if (sprite_ && move.x != 0.0f) {
            sprite_->SetFlipX(move.x > 0.0f);
        }
    }

    // Love縁グループとの距離を常に制限（攻撃中でなくても）
    std::vector<Bond*> bonds = BondManager::Get().GetBondsFor(this);
    for (Bond* bond : bonds) {
        if (bond->GetType() != BondType::Love) continue;

        // 相手がGroupか確認
        BondableEntity other = (bond->GetEntityA() == BondableEntity(this))
                                   ? bond->GetEntityB()
                                   : bond->GetEntityA();
        Group** groupPtr = std::get_if<Group*>(&other);
        if (!groupPtr || !*groupPtr) continue;
        Group* group = *groupPtr;

        // グループとの距離を制限
        Vector2 playerPos = transform_->GetPosition();
        Vector2 groupPos = group->GetPosition();
        Vector2 diff = playerPos - groupPos;
        float distance = diff.Length();

        // ゼロ距離エッジケースを考慮
        constexpr float kMinDistance = 0.0001f;
        if (distance > GameConstants::kLoveInterruptDistance && distance > kMinDistance) {
            // 制限距離に向かって徐々に戻る（急なワープを防ぐ）
            diff.Normalize();
            Vector2 constrainedPos = groupPos + diff * GameConstants::kLoveInterruptDistance;

            // 現在位置から制限位置への移動を滑らかに
            Vector2 toConstrained = constrainedPos - playerPos;
            float distToConstrained = toConstrained.Length();

            // 最大移動速度（プレイヤーの移動速度より速め）
            constexpr float kMaxPullSpeed = 400.0f;
            float maxMove = kMaxPullSpeed * dt;

            if (distToConstrained > maxMove) {
                // 徐々に制限位置へ移動
                toConstrained.Normalize();
                transform_->SetPosition(playerPos + toConstrained * maxMove);
            } else {
                // 十分近いのでそのまま設定
                transform_->SetPosition(constrainedPos);
            }
        }
    }
}

//----------------------------------------------------------------------------
void Player::Render(SpriteBatch& spriteBatch)
{
    if (!transform_ || !sprite_) return;

    if (animator_) {
        spriteBatch.Draw(*sprite_, *transform_, *animator_);
    } else {
        spriteBatch.Draw(*sprite_, *transform_);
    }
}

//----------------------------------------------------------------------------
void Player::TakeDamage(float damage)
{
    if (!IsAlive()) return;

    hp_ -= damage;
    if (hp_ < 0.0f) {
        hp_ = 0.0f;
    }

    if (hp_ <= 0.0f) {
        LOG_INFO("[Player] Died!");
        // TODO: OnPlayerDiedイベント発行
    }
}

//----------------------------------------------------------------------------
Vector2 Player::GetPosition() const
{
    if (transform_) {
        return transform_->GetPosition();
    }
    return Vector2::Zero;
}

//----------------------------------------------------------------------------
bool Player::ConsumeFe(float amount)
{
    if (fe_ < amount) return false;

    fe_ -= amount;
    LOG_INFO("[Player] FE consumed: " + std::to_string(amount) + ", remaining: " + std::to_string(fe_));
    return true;
}

//----------------------------------------------------------------------------
void Player::RecoverFe(float amount)
{
    fe_ += amount;
    if (fe_ > maxFe_) {
        fe_ = maxFe_;
    }
}
