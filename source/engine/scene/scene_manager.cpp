//----------------------------------------------------------------------------
//! @file   scene_manager.cpp
//! @brief  シーンマネージャー実装
//----------------------------------------------------------------------------
#include "scene_manager.h"
#include "engine/texture/texture_manager.h"
#include <chrono>

//----------------------------------------------------------------------------
// ※ Create()/Destroy()はヘッダーでインライン実装

//----------------------------------------------------------------------------
void SceneManager::ApplyPendingChange(std::unique_ptr<Scene>& current)
{
    // 非同期ロード完了チェック
    if (asyncPending_ && loadingScene_) {
        // ロード完了を確認（ノンブロッキング）
        if (loadFuture_.valid()) {
            auto status = loadFuture_.wait_for(std::chrono::milliseconds(0));
            if (status == std::future_status::ready) {
                // ロード完了 - シーン切り替え実行
                loadFuture_.get();  // 例外があればここで再throw

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
                asyncPending_ = false;
                loadProgress_.store(0.0f);

                if (current) {
                    // テクスチャスコープ開始
                    TextureManager::ScopeId newScopeId = TextureManager::Get().BeginScope();
                    current->SetTextureScopeId(newScopeId);
                    current->OnEnter();
                }
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
    if (!loadFuture_.valid()) return false;

    auto status = loadFuture_.wait_for(std::chrono::milliseconds(0));
    return status != std::future_status::ready;
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
    // futureの完了を待ってからクリーンアップ
    if (loadFuture_.valid()) {
        loadFuture_.wait();
    }
    loadingScene_.reset();
    asyncPending_ = false;
    loadProgress_.store(0.0f);
}
