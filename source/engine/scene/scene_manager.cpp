//----------------------------------------------------------------------------
//! @file   scene_manager.cpp
//! @brief  シーンマネージャー実装
//----------------------------------------------------------------------------
#include "scene_manager.h"
#include "engine/texture/texture_manager.h"

//----------------------------------------------------------------------------
// ※ Create()/Destroy()はヘッダーでインライン実装

//----------------------------------------------------------------------------
void SceneManager::ApplyPendingChange(std::unique_ptr<Scene>& current)
{
    // 非同期ロード完了チェック
    if (asyncPending_ && loadingScene_) {
        // ロード完了を確認（ノンブロッキング）
        if (loadHandle_.IsValid() && loadHandle_.IsComplete()) {
            // エラーチェック
            if (loadHandle_.HasError()) {
                // ロード失敗 - クリーンアップ
                loadingScene_.reset();
                loadHandle_ = JobHandle{};
                asyncPending_ = false;
                loadProgress_.store(0.0f);
                return;
            }

            // ロード完了 - シーン切り替え実行
            // 現在のシーンを終了
            if (current) {
                current->OnExit();
                // テクスチャスコープ終了 → 自動GC
                TextureManager::ScopeId scopeId = current->GetTextureScopeId();
                if (scopeId != TextureManager::kGlobalScope) {
                    TextureManager::Get().EndScope(scopeId);
                }
            }

            // ロード完了コールバック（メインスレッド）
            loadingScene_->OnLoadComplete();

            // 新しいシーンに切り替え
            current = std::move(loadingScene_);
            loadHandle_ = JobHandle{};  // ハンドルをリセット
            asyncPending_ = false;
            loadProgress_.store(0.0f);

            if (current) {
                // テクスチャスコープ開始
                TextureManager::ScopeId newScopeId = TextureManager::Get().BeginScope();
                current->SetTextureScopeId(newScopeId);
                current->OnEnter();
            }
        }
        return;
    }

    // 同期ロード
    if (!pendingFactory_) {
        return;
    }

    // 現在のシーンを終了
    if (current) {
        current->OnExit();
        // テクスチャスコープ終了 → 自動GC
        TextureManager::ScopeId scopeId = current->GetTextureScopeId();
        if (scopeId != TextureManager::kGlobalScope) {
            TextureManager::Get().EndScope(scopeId);
        }
    }

    // 新しいシーンに切り替え
    current = pendingFactory_();
    pendingFactory_ = nullptr;

    if (current) {
        // テクスチャスコープ開始
        TextureManager::ScopeId newScopeId = TextureManager::Get().BeginScope();
        current->SetTextureScopeId(newScopeId);
        current->OnEnter();
    }
}

//----------------------------------------------------------------------------
bool SceneManager::IsLoading() const
{
    return loadHandle_.IsValid() && !loadHandle_.IsComplete();
}

//----------------------------------------------------------------------------
float SceneManager::GetLoadProgress() const
{
    if (loadingScene_) {
        return loadingScene_->GetLoadProgress();
    }
    return loadProgress_.load();
}

//----------------------------------------------------------------------------
void SceneManager::CancelAsyncLoad()
{
    // 注: 実行中のOnLoadAsync()は中断できない
    // ジョブの完了を待ってからクリーンアップ
    if (loadHandle_.IsValid()) {
        loadHandle_.Wait();
    }
    loadHandle_ = JobHandle{};
    loadingScene_.reset();
    asyncPending_ = false;
    loadProgress_.store(0.0f);
}
