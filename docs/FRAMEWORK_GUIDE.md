# フレームワーク使用ガイド

チームメンバー向けのゲームフレームワーク使用ガイドです。

## 目次

1. [基本構造](#基本構造)
2. [シーンの作成](#シーンの作成)
3. [GameObjectとコンポーネント](#gameobjectとコンポーネント)
4. [カスタムComponentの作成](#カスタムcomponentの作成)
5. [Transform2D - 位置・回転・スケール](#transform2d---位置回転スケール)
6. [SpriteRenderer - スプライト描画](#spriterenderer---スプライト描画)
7. [入力処理](#入力処理)
8. [Camera2D - カメラ](#camera2d---カメラ)
9. [Collider2D - 当たり判定](#collider2d---当たり判定)
10. [Animator - アニメーション](#animator---アニメーション)

---

## 基本構造

### ゲームのライフサイクル

```
起動 → Initialize() → メインループ(Update → Render) → Shutdown()
```

### レイヤー構成

```
game/          ← ゲーム固有のコード（シーン、ゲームロジック）
engine/        ← 汎用エンジン（入力、描画、衝突判定など）
dx11/          ← DirectX11ラッパー（基本的に触らない）
```

---

## シーンの作成

シーンは「タイトル画面」「ゲーム画面」「リザルト画面」などの単位です。

### 新しいシーンを作る

**1. ヘッダーファイル（例: `source/game/scenes/title_scene.h`）**

```cpp
#pragma once

#include "engine/scene/scene.h"
#include "engine/component/game_object.h"
#include "engine/component/transform2d.h"
#include "engine/component/sprite_renderer.h"
#include "engine/component/camera2d.h"
#include <memory>

class TitleScene : public Scene {
public:
    void OnEnter() override;   // シーン開始時
    void OnExit() override;    // シーン終了時
    void Update() override;    // 毎フレーム更新
    void Render() override;    // 毎フレーム描画

    const char* GetName() const override { return "TitleScene"; }

private:
    std::unique_ptr<GameObject> cameraObj_;
    std::unique_ptr<GameObject> logo_;
    Camera2D* camera_ = nullptr;
};
```

**2. 実装ファイル（例: `source/game/scenes/title_scene.cpp`）**

```cpp
#include "title_scene.h"
#include "engine/platform/application.h"
#include "engine/texture/texture_manager.h"
#include "engine/graphics2d/sprite_batch.h"
#include "dx11/graphics_context.h"
#include "engine/platform/renderer.h"

void TitleScene::OnEnter()
{
    Application& app = Application::Get();
    Window* window = app.GetWindow();
    float width = static_cast<float>(window->GetWidth());
    float height = static_cast<float>(window->GetHeight());

    // カメラ作成
    cameraObj_ = std::make_unique<GameObject>("Camera");
    cameraObj_->AddComponent<Transform2D>(Vector2(width * 0.5f, height * 0.5f));
    camera_ = cameraObj_->AddComponent<Camera2D>(width, height);

    // ロゴ作成
    logo_ = std::make_unique<GameObject>("Logo");
    Transform2D* logoTransform = logo_->AddComponent<Transform2D>();
    logoTransform->SetPosition(Vector2(width * 0.5f, height * 0.5f));

    SpriteRenderer* logoSprite = logo_->AddComponent<SpriteRenderer>();
    logoSprite->SetTexture(TextureManager::Get().LoadTexture2D("textures:/logo.png"));

    SpriteBatch::Get().Initialize();
}

void TitleScene::OnExit()
{
    logo_.reset();
    cameraObj_.reset();
    SpriteBatch::Get().Shutdown();
}

void TitleScene::Update()
{
    float dt = Application::Get().GetDeltaTime();
    logo_->Update(dt);
}

void TitleScene::Render()
{
    GraphicsContext& ctx = GraphicsContext::Get();
    Renderer& renderer = Renderer::Get();
    Texture* backBuffer = renderer.GetBackBuffer();
    if (!backBuffer) return;

    ctx.SetRenderTarget(backBuffer, nullptr);
    ctx.SetViewport(0, 0,
        static_cast<float>(backBuffer->Width()),
        static_cast<float>(backBuffer->Height()));

    float clearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
    ctx.ClearRenderTarget(backBuffer, clearColor);

    SpriteBatch& spriteBatch = SpriteBatch::Get();
    spriteBatch.SetCamera(*camera_);
    spriteBatch.Begin();

    Transform2D* transform = logo_->GetComponent<Transform2D>();
    SpriteRenderer* sprite = logo_->GetComponent<SpriteRenderer>();
    if (transform && sprite) {
        spriteBatch.Draw(*sprite, *transform);
    }

    spriteBatch.End();
}
```

**3. シーンを登録（`game.cpp`）**

```cpp
#include "scenes/title_scene.h"

bool Game::Initialize()
{
    // ... 初期化コード ...

    // 初期シーンを設定
    sceneManager_.Load<TitleScene>();
    sceneManager_.ApplyPendingChange(currentScene_);

    return true;
}
```

### シーン遷移

```cpp
#include "engine/scene/scene_manager.h"

// 別のシーンに切り替え
SceneManager::Get().Load<GameScene>();
```

---

## GameObjectとコンポーネント

### 基本的な使い方

```cpp
#include "engine/component/game_object.h"
#include "engine/component/transform2d.h"
#include "engine/component/sprite_renderer.h"

// GameObjectを作成
std::unique_ptr<GameObject> player = std::make_unique<GameObject>("Player");

// コンポーネントを追加
Transform2D* transform = player->AddComponent<Transform2D>();
SpriteRenderer* sprite = player->AddComponent<SpriteRenderer>();

// コンポーネントを取得
Transform2D* t = player->GetComponent<Transform2D>();

// 更新（全コンポーネントのUpdate()が呼ばれる）
player->Update(deltaTime);
```

### コンポーネントの初期化

コンストラクタ引数で初期値を渡せます：

```cpp
// 初期位置を指定してTransform2D作成
player->AddComponent<Transform2D>(Vector2(100.0f, 200.0f));

// 初期位置、回転、スケールを指定
player->AddComponent<Transform2D>(
    Vector2(100.0f, 200.0f),  // 位置
    0.0f,                      // 回転（ラジアン）
    Vector2(1.0f, 1.0f)        // スケール
);

// コライダーのサイズを指定
player->AddComponent<Collider2D>(Vector2(32.0f, 32.0f));
```

---

## カスタムComponentの作成

ゲームロジック（操作、AI、HP管理など）はComponentとして作成し、GameObjectに追加します。

### 構造

```
GameObject
  ├── Transform2D      (位置・回転・スケール)
  ├── SpriteRenderer   (見た目)
  ├── Collider2D       (当たり判定)
  ├── PlayerController (操作ロジック)    ← 自作
  ├── EnemyAI          (敵の動き)        ← 自作
  └── Health           (HP管理)          ← 自作
```

### PlayerController - プレイヤー操作

**source/game/components/player_controller.h**

```cpp
#pragma once

#include "engine/component/component.h"
#include "engine/component/transform2d.h"
#include "engine/component/sprite_renderer.h"
#include "engine/input/input_manager.h"

class PlayerController : public Component {
public:
    void OnAttach() override {
        transform_ = GetOwner()->GetComponent<Transform2D>();
        sprite_ = GetOwner()->GetComponent<SpriteRenderer>();
    }

    void Update(float dt) override {
        Keyboard& kb = InputManager::GetInstance()->GetKeyboard();

        Vector2 move = Vector2::Zero;
        if (kb.IsKeyPressed(Key::W)) move.y -= speed_ * dt;
        if (kb.IsKeyPressed(Key::S)) move.y += speed_ * dt;
        if (kb.IsKeyPressed(Key::A)) move.x -= speed_ * dt;
        if (kb.IsKeyPressed(Key::D)) move.x += speed_ * dt;

        if (move.x != 0.0f || move.y != 0.0f) {
            transform_->Translate(move);

            // 向きで反転
            if (move.x < 0) sprite_->SetFlipX(true);
            if (move.x > 0) sprite_->SetFlipX(false);
        }
    }

    void SetSpeed(float speed) { speed_ = speed; }

private:
    Transform2D* transform_ = nullptr;
    SpriteRenderer* sprite_ = nullptr;
    float speed_ = 200.0f;
};
```

### Health - HP管理

**source/game/components/health.h**

```cpp
#pragma once

#include "engine/component/component.h"
#include <functional>

class Health : public Component {
public:
    explicit Health(int maxHp = 100) : hp_(maxHp), maxHp_(maxHp) {}

    void TakeDamage(int amount) {
        hp_ -= amount;
        if (hp_ <= 0) {
            hp_ = 0;
            if (onDeath_) onDeath_();
        }
    }

    void Heal(int amount) {
        hp_ += amount;
        if (hp_ > maxHp_) hp_ = maxHp_;
    }

    bool IsAlive() const { return hp_ > 0; }
    int GetHP() const { return hp_; }
    int GetMaxHP() const { return maxHp_; }

    // 死亡時コールバック
    void SetOnDeath(std::function<void()> callback) { onDeath_ = callback; }

private:
    int hp_;
    int maxHp_;
    std::function<void()> onDeath_;
};
```

### EnemyAI - 敵の動き

**source/game/components/enemy_ai.h**

```cpp
#pragma once

#include "engine/component/component.h"
#include "engine/component/transform2d.h"
#include <cmath>
#include <cstdlib>

class EnemyAI : public Component {
public:
    void OnAttach() override {
        transform_ = GetOwner()->GetComponent<Transform2D>();
        targetPos_ = transform_->GetPosition();
    }

    void Update(float dt) override {
        Vector2 pos = transform_->GetPosition();
        Vector2 diff = targetPos_ - pos;
        float dist = std::sqrt(diff.x * diff.x + diff.y * diff.y);

        if (dist < 5.0f) {
            // 新しい目標地点
            targetPos_.x = pos.x + (rand() % 200 - 100);
            targetPos_.y = pos.y + (rand() % 200 - 100);
        } else {
            // 目標に向かう
            Vector2 dir(diff.x / dist, diff.y / dist);
            transform_->Translate(dir.x * speed_ * dt, dir.y * speed_ * dt);
        }
    }

    void SetSpeed(float speed) { speed_ = speed; }

private:
    Transform2D* transform_ = nullptr;
    Vector2 targetPos_;
    float speed_ = 50.0f;
};
```

### シーンでの使用例

```cpp
#include "components/player_controller.h"
#include "components/enemy_ai.h"
#include "components/health.h"

class GameScene : public Scene {
private:
    std::vector<std::unique_ptr<GameObject>> objects_;

public:
    void OnEnter() override
    {
        // プレイヤー作成
        CreatePlayer(Vector2(400, 300));

        // 敵を5体作成
        for (int i = 0; i < 5; ++i) {
            CreateEnemy(Vector2(100 + i * 150, 200));
        }
    }

    void CreatePlayer(const Vector2& pos)
    {
        std::unique_ptr<GameObject> obj = std::make_unique<GameObject>("Player");

        obj->AddComponent<Transform2D>(pos)->SetScale(2.0f);

        SpriteRenderer* sprite = obj->AddComponent<SpriteRenderer>();
        sprite->SetTexture(playerTexture_);

        Collider2D* col = obj->AddComponent<Collider2D>(Vector2(64, 64));
        col->SetLayer(0x01);
        col->SetMask(0x02);

        obj->AddComponent<PlayerController>()->SetSpeed(250.0f);
        obj->AddComponent<Health>(100);

        // 衝突でダメージ
        col->SetOnCollisionEnter([obj = obj.get()](Collider2D*, Collider2D*) {
            Health* hp = obj->GetComponent<Health>();
            if (hp) {
                hp->TakeDamage(10);
                LOG_INFO("[Player] HP: %d", hp->GetHP());
            }
        });

        objects_.push_back(std::move(obj));
    }

    void CreateEnemy(const Vector2& pos)
    {
        std::unique_ptr<GameObject> obj = std::make_unique<GameObject>("Enemy");

        obj->AddComponent<Transform2D>(pos);

        SpriteRenderer* sprite = obj->AddComponent<SpriteRenderer>();
        sprite->SetTexture(enemyTexture_);
        sprite->SetColor(Color(1.0f, 0.3f, 0.3f, 1.0f));

        Collider2D* col = obj->AddComponent<Collider2D>(Vector2(32, 32));
        col->SetLayer(0x02);
        col->SetMask(0x01);

        obj->AddComponent<EnemyAI>()->SetSpeed(80.0f);
        obj->AddComponent<Health>(30);

        objects_.push_back(std::move(obj));
    }

    void Update() override
    {
        float dt = Application::Get().GetDeltaTime();

        // 全オブジェクト更新（これだけ！）
        for (std::unique_ptr<GameObject>& obj : objects_) {
            obj->Update(dt);
        }

        // 死んだオブジェクト削除
        objects_.erase(
            std::remove_if(objects_.begin(), objects_.end(),
                [](const std::unique_ptr<GameObject>& obj) {
                    Health* hp = obj->GetComponent<Health>();
                    return hp && !hp->IsAlive();
                }),
            objects_.end()
        );

        CollisionManager::Get().Update(dt);
    }

    void Render() override
    {
        SpriteBatch& sb = SpriteBatch::Get();
        sb.Begin();

        for (std::unique_ptr<GameObject>& obj : objects_) {
            Transform2D* t = obj->GetComponent<Transform2D>();
            SpriteRenderer* s = obj->GetComponent<SpriteRenderer>();
            if (t && s) sb.Draw(*s, *t);
        }

        sb.End();
    }
};
```

### メリット

- `obj->Update(dt)` だけで全Component更新
- ロジックの再利用が簡単（EnemyAIを別の敵にも使える）
- Player/Enemyクラス不要、GameObjectだけで完結

---

## Transform2D - 位置・回転・スケール

### 位置

```cpp
Transform2D* transform = obj->GetComponent<Transform2D>();

// 位置の取得
Vector2 pos = transform->GetPosition();

// 位置の設定
transform->SetPosition(Vector2(100.0f, 200.0f));
transform->SetPosition(100.0f, 200.0f);  // 同じ

// 相対移動
transform->Translate(Vector2(10.0f, 0.0f));  // 右に10
transform->Translate(0.0f, -5.0f);           // 上に5
```

### 回転

```cpp
// 度で設定
transform->SetRotationDegrees(45.0f);

// ラジアンで設定
transform->SetRotation(3.14159f);  // 180度

// 相対回転
transform->RotateDegrees(90.0f);   // 90度回転
```

### スケール

```cpp
// 均一スケール
transform->SetScale(2.0f);  // 2倍

// X, Y個別スケール
transform->SetScale(2.0f, 1.0f);  // 横2倍、縦そのまま
transform->SetScale(Vector2(0.5f, 0.5f));  // 半分
```

### 親子関係

```cpp
Transform2D* parentTransform = parent->GetComponent<Transform2D>();
Transform2D* childTransform = child->GetComponent<Transform2D>();

// 親を設定（子は親の変換に追従）
childTransform->SetParent(parentTransform);

// 親子関係を解除
childTransform->SetParent(nullptr);
// または
childTransform->DetachFromParent();

// ワールド座標を取得（親の変換を考慮）
Vector2 worldPos = childTransform->GetWorldPosition();
```

---

## SpriteRenderer - スプライト描画

### 基本設定

```cpp
SpriteRenderer* sprite = obj->AddComponent<SpriteRenderer>();

// テクスチャ設定
sprite->SetTexture(texture);

// 色（乗算）
sprite->SetColor(Color(1.0f, 0.0f, 0.0f, 1.0f));  // 赤
sprite->SetColor(1.0f, 1.0f, 1.0f, 0.5f);         // 半透明

// アルファのみ変更
sprite->SetAlpha(0.5f);

// 反転
sprite->SetFlipX(true);   // 左右反転
sprite->SetFlipY(true);   // 上下反転

// カスタムサイズ（指定しなければテクスチャサイズ）
sprite->SetSize(64.0f, 64.0f);
```

### 描画順

```cpp
// レイヤー（大きいほど手前）
sprite->SetSortingLayer(1);  // 背景=0, キャラ=1, UI=2 など

// 同レイヤー内の順序
sprite->SetOrderInLayer(10);
```

### SpriteBatchで描画

```cpp
void MyScene::Render()
{
    SpriteBatch& spriteBatch = SpriteBatch::Get();
    spriteBatch.SetCamera(*camera_);
    spriteBatch.Begin();

    // 描画
    Transform2D* transform = obj->GetComponent<Transform2D>();
    SpriteRenderer* sprite = obj->GetComponent<SpriteRenderer>();
    spriteBatch.Draw(*sprite, *transform);

    spriteBatch.End();
}
```

---

## 入力処理

### キーボード

```cpp
#include "engine/input/input_manager.h"

void MyScene::Update()
{
    InputManager* inputMgr = InputManager::GetInstance();
    Keyboard& keyboard = inputMgr->GetKeyboard();

    // 押している間ずっとtrue
    if (keyboard.IsKeyPressed(Key::W)) {
        // Wキーを押している間
    }

    // 押した瞬間だけtrue（1フレームのみ）
    if (keyboard.IsKeyDown(Key::Space)) {
        // スペースを押した瞬間（ジャンプ等）
    }

    // 離した瞬間だけtrue
    if (keyboard.IsKeyUp(Key::E)) {
        // Eを離した瞬間
    }

    // 長押し時間（秒）
    float holdTime = keyboard.GetKeyHoldTime(Key::LShift);

    // 修飾キー
    if (keyboard.IsShiftPressed()) { }
    if (keyboard.IsControlPressed()) { }
    if (keyboard.IsAltPressed()) { }
}
```

### よく使うキー

```cpp
// 矢印キー
Key::Up, Key::Down, Key::Left, Key::Right

// WASD
Key::W, Key::A, Key::S, Key::D

// アルファベット
Key::A ~ Key::Z

// 数字
Key::D0 ~ Key::D9  // メインキー
Key::Num0 ~ Key::Num9  // テンキー

// 特殊キー
Key::Space, Key::Enter, Key::Escape, Key::Tab
Key::LShift, Key::RShift, Key::LControl, Key::RControl
```

### マウス

```cpp
Mouse& mouse = inputMgr->GetMouse();

// 位置
Vector2 pos = mouse.GetPosition();

// ボタン
if (mouse.IsButtonPressed(MouseButton::Left)) { }
if (mouse.IsButtonDown(MouseButton::Right)) { }  // クリック瞬間

// スクロール
float scroll = mouse.GetScrollDelta();
```

---

## Camera2D - カメラ

### カメラの作成

```cpp
// カメラ用GameObjectを作成（Transform2D必須）
std::unique_ptr<GameObject> cameraObj = std::make_unique<GameObject>("Camera");
cameraObj->AddComponent<Transform2D>(Vector2(screenWidth * 0.5f, screenHeight * 0.5f));
Camera2D* camera = cameraObj->AddComponent<Camera2D>(screenWidth, screenHeight);
```

### カメラ操作

```cpp
// 位置設定
camera->SetPosition(Vector2(500.0f, 300.0f));

// 移動
camera->Translate(Vector2(10.0f, 0.0f));

// ズーム（1.0 = 等倍、2.0 = 2倍拡大）
camera->SetZoom(2.0f);

// 回転
camera->SetRotationDegrees(45.0f);
```

### プレイヤー追従

```cpp
void MyScene::Update()
{
    float dt = Application::Get().GetDeltaTime();

    // プレイヤーにスムーズ追従（0.1 = 追従速度、小さいほどゆっくり）
    camera_->Follow(playerTransform_->GetPosition(), 0.1f);
}
```

### 座標変換

```cpp
// スクリーン座標 → ワールド座標（マウスクリック位置など）
Vector2 mousePos = mouse.GetPosition();
Vector2 worldPos = camera->ScreenToWorld(mousePos);

// ワールド座標 → スクリーン座標
Vector2 screenPos = camera->WorldToScreen(enemyPos);
```

---

## Collider2D - 当たり判定

### 基本セットアップ

**`game.cpp` に初期化を追加：**

```cpp
#include "engine/collision/collision_manager.h"

bool Game::Initialize()
{
    // CollisionManager初期化（グリッドセルサイズ）
    CollisionManager::Get().Initialize(256);

    // ... 他の初期化 ...
}

void Game::Shutdown() noexcept
{
    // ... 他のシャットダウン ...
    CollisionManager::Get().Shutdown();
}
```

### コライダーの追加

```cpp
#include "engine/component/collider2d.h"

// プレイヤーにコライダーを追加
Collider2D* playerCollider = player->AddComponent<Collider2D>(Vector2(32, 32));  // サイズ

// レイヤー設定（自分が何者か）
playerCollider->SetLayer(0x01);  // プレイヤーレイヤー

// マスク設定（何と衝突するか）
playerCollider->SetMask(0x02);   // 敵レイヤーと衝突
```

### レイヤーとマスクの設計例

```cpp
// レイヤー定義（ビットフラグ）
constexpr uint8_t LAYER_PLAYER = 0x01;   // 0000 0001
constexpr uint8_t LAYER_ENEMY  = 0x02;   // 0000 0010
constexpr uint8_t LAYER_BULLET = 0x04;   // 0000 0100
constexpr uint8_t LAYER_WALL   = 0x08;   // 0000 1000

// プレイヤー：敵と弾と壁に当たる
playerCollider->SetLayer(LAYER_PLAYER);
playerCollider->SetMask(LAYER_ENEMY | LAYER_BULLET | LAYER_WALL);

// 敵：プレイヤーと壁に当たる
enemyCollider->SetLayer(LAYER_ENEMY);
enemyCollider->SetMask(LAYER_PLAYER | LAYER_WALL);

// 弾：プレイヤーに当たる（敵の弾の場合）
bulletCollider->SetLayer(LAYER_BULLET);
bulletCollider->SetMask(LAYER_PLAYER);
```

### コールバックとは？

**コールバック**とは「ある出来事が起きたときに自動で呼び出される関数」のことです。

当たり判定の場合、「衝突が発生したときに何をするか」をあらかじめ登録しておくと、
エンジンが衝突を検出したときに自動でその処理を実行してくれます。

```cpp
// 例: 衝突したときにログを出力する処理を登録
playerCollider->SetOnCollisionEnter([](Collider2D* self, Collider2D* other) {
    LOG_INFO("何かに当たった！");
});
```

上のコードでは `[](Collider2D* self, Collider2D* other) { ... }` の部分が**ラムダ式**（無名関数）です。
これを `SetOnCollisionEnter` に渡すことで、「衝突開始時にこの処理を実行して」と登録しています。

### 衝突コールバックの種類

```cpp
// 衝突した瞬間（1回だけ呼ばれる）
playerCollider->SetOnCollisionEnter([](Collider2D* self, Collider2D* other) {
    LOG_INFO("衝突開始！");
    // self  = 自分のコライダー
    // other = 相手のコライダー
    // other->GetOwner() で相手のGameObjectを取得できる
});

// 衝突している間（毎フレーム呼ばれる）
playerCollider->SetOnCollision([](Collider2D* self, Collider2D* other) {
    // 継続的なダメージ処理など
});

// 衝突が終わった瞬間（1回だけ呼ばれる）
playerCollider->SetOnCollisionExit([](Collider2D* self, Collider2D* other) {
    LOG_INFO("衝突終了！");
});
```

### クラス内でコールバックを使う

クラスのメンバ変数にアクセスしたい場合は `[this]` でキャプチャします：

```cpp
class Player {
private:
    int hp_ = 100;

public:
    void Initialize(...)
    {
        collider_->SetOnCollisionEnter([this](Collider2D* /*self*/, Collider2D* /*other*/) {
            // this をキャプチャしているので hp_ にアクセスできる
            hp_ -= 10;
            LOG_INFO("ダメージ！ HP: %d", hp_);
        });
    }
};
```

### 衝突判定の更新

```cpp
void MyScene::Update()
{
    float dt = Application::Get().GetDeltaTime();

    // GameObjectの更新（Collider2DがTransformと同期される）
    player_->Update(dt);
    for (auto& enemy : enemies_) {
        enemy->Update(dt);
    }

    // 衝突判定を実行（固定タイムステップ）
    CollisionManager::Get().Update(dt);
}
```

### トリガーモード

物理的に押し戻さず、イベントだけ発生させたい場合：

```cpp
itemCollider->SetTrigger(true);  // トリガーモード
```

---

## Animator - アニメーション

スプライトシートからアニメーションを再生します。

### セットアップ

```cpp
#include "engine/component/animator.h"

// スプライトシート: 4行 x 8列、6フレームごとに更新
Animator* animator = sprite->AddComponent<Animator>(4, 8, 6);

// 各行のフレーム数を設定
animator->SetRowFrameCount(0, 4);  // 行0: 4フレーム（待機）
animator->SetRowFrameCount(1, 6);  // 行1: 6フレーム（歩き）
animator->SetRowFrameCount(2, 3);  // 行2: 3フレーム（攻撃）
animator->SetRowFrameCount(3, 2);  // 行3: 2フレーム（ジャンプ）
```

### アニメーション切り替え

```cpp
// 行を切り替え（アニメーション変更）
animator->SetRow(1);  // 歩きアニメーション

// 左右反転
animator->SetMirror(true);   // 左向き
animator->SetMirror(false);  // 右向き
```

### 描画

```cpp
// SpriteBatchで描画する際、AnimatorのUV情報を使用
// （SpriteBatchが自動的にAnimatorを参照）
```

---

## ログ出力

```cpp
#include "common/logging/logging.h"

LOG_DEBUG("デバッグ情報: %d", value);  // 青（デバッグビルドのみ）
LOG_INFO("情報: %s", message);          // 緑
LOG_WARN("警告: 値が大きすぎます");     // 黄
LOG_ERROR("エラー: %s", error);         // 赤
```

デバッグビルドではコンソールウィンドウが開き、色付きでログが表示されます。

---

## トラブルシューティング

### よくあるエラー

| 症状 | 原因 | 解決策 |
|------|------|--------|
| コンポーネントがnullptr | AddComponent忘れ | `obj->AddComponent<T>()` を確認 |
| 描画されない | SpriteBatch未初期化 | `SpriteBatch::Get().Initialize()` を追加 |
| 衝突が検出されない | CollisionManager未初期化 | `CollisionManager::Get().Initialize(256)` を追加 |
| 衝突が検出されない | レイヤー/マスク設定ミス | SetLayer/SetMaskを確認 |
| カメラが動かない | Transform2D未追加 | Camera2Dの前にTransform2Dを追加 |

### デバッグのコツ

1. `LOG_INFO` でオブジェクトの位置を出力
2. 衝突コールバックで `LOG_INFO("[Collision] Enter!")` を出力
3. Update/Render の各処理にログを追加して呼ばれているか確認

---

## 参考リンク

- [README.md](../README.md) - ビルド方法、Git操作
