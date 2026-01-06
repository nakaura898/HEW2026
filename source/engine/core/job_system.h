//----------------------------------------------------------------------------
//! @file   job_system.h
//! @brief  マルチスレッドジョブシステム
//----------------------------------------------------------------------------
#pragma once

#include "common/utility/non_copyable.h"
#include <functional>
#include <atomic>
#include <memory>
#include <cstdint>
#include <cassert>
#include <vector>
#include <string_view>

//! @brief ジョブ優先度
enum class JobPriority : uint8_t {
    High = 0,    //!< 高優先度（フレーム内で必ず完了）
    Normal = 1,  //!< 通常
    Low = 2,     //!< 低優先度（バックグラウンド処理）
    Count = 3
};

//============================================================================
//! @brief キャンセルトークン
//!
//! ジョブのキャンセル要求を伝達する。
//! シーン遷移やロード中断時に使用。
//============================================================================
class CancelToken
{
public:
    CancelToken() = default;

    //! @brief キャンセル要求
    void Cancel() noexcept { cancelled_.store(true, std::memory_order_release); }

    //! @brief キャンセルされたか
    [[nodiscard]] bool IsCancelled() const noexcept {
        return cancelled_.load(std::memory_order_acquire);
    }

    //! @brief リセット（再利用時）
    void Reset() noexcept { cancelled_.store(false, std::memory_order_release); }

private:
    std::atomic<bool> cancelled_{false};
};

using CancelTokenPtr = std::shared_ptr<CancelToken>;

//============================================================================
//! @brief ジョブカウンター（依存関係管理用）
//============================================================================
class JobCounter
{
public:
    JobCounter();
    explicit JobCounter(uint32_t initialCount);
    ~JobCounter();

    JobCounter(const JobCounter&) = delete;
    JobCounter& operator=(const JobCounter&) = delete;

    void Decrement() noexcept;
    void Wait() const noexcept;
    [[nodiscard]] bool IsComplete() const noexcept;
    [[nodiscard]] uint32_t GetCount() const noexcept;
    void Reset(uint32_t count) noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

using JobCounterPtr = std::shared_ptr<JobCounter>;

//! @brief ジョブ関数型
using JobFunction = std::function<void()>;

//! @brief キャンセル対応ジョブ関数型
using CancellableJobFunction = std::function<void(const CancelToken&)>;

//============================================================================
//! @brief ジョブハンドル
//!
//! 投入したジョブを追跡し、依存関係の設定やキャンセルに使用。
//============================================================================
class JobHandle
{
public:
    JobHandle() = default;

    //! @brief 有効なハンドルか
    [[nodiscard]] bool IsValid() const noexcept { return counter_ != nullptr; }

    //! @brief ジョブが完了したか
    [[nodiscard]] bool IsComplete() const noexcept {
        return counter_ && counter_->IsComplete();
    }

    //! @brief ジョブの完了を待機
    void Wait() const noexcept {
        if (counter_) counter_->Wait();
    }

    //! @brief 内部カウンター取得（依存関係設定用）
    [[nodiscard]] JobCounterPtr GetCounter() const noexcept { return counter_; }

private:
    friend class JobSystem;
    explicit JobHandle(JobCounterPtr counter) : counter_(std::move(counter)) {}
    JobCounterPtr counter_;
};

//============================================================================
//! @brief ジョブ記述子
//!
//! ジョブの詳細設定を行うビルダーパターン。
//============================================================================
class JobDesc
{
public:
    JobDesc() = default;
    explicit JobDesc(JobFunction func) : function_(std::move(func)) {}

    //! @brief ジョブ関数を設定
    JobDesc& SetFunction(JobFunction func) {
        function_ = std::move(func);
        return *this;
    }

    //! @brief キャンセル対応ジョブ関数を設定
    JobDesc& SetCancellableFunction(CancellableJobFunction func) {
        cancellableFunction_ = std::move(func);
        return *this;
    }

    //! @brief 優先度を設定
    JobDesc& SetPriority(JobPriority priority) {
        priority_ = priority;
        return *this;
    }

    //! @brief 依存ジョブを追加（このジョブより先に完了する必要がある）
    JobDesc& AddDependency(const JobHandle& dependency) {
        if (dependency.IsValid()) {
            dependencies_.push_back(dependency.GetCounter());
        }
        return *this;
    }

    //! @brief 複数の依存ジョブを追加
    JobDesc& AddDependencies(const std::vector<JobHandle>& deps) {
        for (const auto& dep : deps) {
            AddDependency(dep);
        }
        return *this;
    }

    //! @brief メインスレッドで実行（レンダリング関連）
    JobDesc& SetMainThreadOnly(bool mainThread = true) {
        mainThreadOnly_ = mainThread;
        return *this;
    }

    //! @brief キャンセルトークンを設定
    JobDesc& SetCancelToken(CancelTokenPtr token) {
        cancelToken_ = std::move(token);
        return *this;
    }

    //! @brief デバッグ名を設定（プロファイリング用）
    JobDesc& SetName([[maybe_unused]] std::string_view name) {
#ifdef _DEBUG
        name_ = name;
#endif
        return *this;
    }

private:
    friend class JobSystem;

    JobFunction function_;
    CancellableJobFunction cancellableFunction_;
    JobPriority priority_ = JobPriority::Normal;
    std::vector<JobCounterPtr> dependencies_;
    CancelTokenPtr cancelToken_;
    bool mainThreadOnly_ = false;
#ifdef _DEBUG
    std::string_view name_;
#endif
};

//============================================================================
//! @brief ジョブシステム（シングルトン）
//!
//! ワーカースレッドプールを管理し、ジョブを並列実行する。
//!
//! @note 使用例:
//! @code
//!   // 単純なジョブ
//!   JobSystem::Get().Submit([]{ DoWork(); });
//!
//!   // 依存関係付きジョブ
//!   auto job1 = JobSystem::Get().SubmitJob(JobDesc([]{ LoadMesh(); }));
//!   auto job2 = JobSystem::Get().SubmitJob(
//!       JobDesc([]{ ProcessMesh(); }).AddDependency(job1));
//!   job2.Wait();
//!
//!   // メインスレッドジョブ（レンダリング）
//!   JobSystem::Get().SubmitJob(
//!       JobDesc([]{ UploadToGPU(); }).SetMainThreadOnly());
//!
//!   // キャンセル可能ジョブ
//!   auto token = std::make_shared<CancelToken>();
//!   JobSystem::Get().SubmitJob(
//!       JobDesc().SetCancellableFunction([](const CancelToken& ct) {
//!           while (!ct.IsCancelled()) { DoWork(); }
//!       }).SetCancelToken(token));
//!   token->Cancel();  // キャンセル要求
//!
//!   // ゲームループ統合
//!   void GameLoop() {
//!       JobSystem::Get().BeginFrame();
//!       // ジョブ投入...
//!       JobSystem::Get().EndFrame();  // フレーム内ジョブ完了待機
//!   }
//! @endcode
//============================================================================
class JobSystem final : private NonCopyableNonMovable
{
public:
    static JobSystem& Get() noexcept {
        assert(instance_ && "JobSystem::Create() must be called first");
        return *instance_;
    }

    static void Create(uint32_t numWorkers = 0);
    static void Destroy();
    [[nodiscard]] static bool IsCreated() noexcept { return instance_ != nullptr; }

    //------------------------------------------------------------------------
    //! @name 基本ジョブ投入（後方互換）
    //------------------------------------------------------------------------
    //!@{

    void Submit(JobFunction job, JobPriority priority = JobPriority::Normal);
    void Submit(JobFunction job, JobCounterPtr counter, JobPriority priority = JobPriority::Normal);
    [[nodiscard]] JobCounterPtr SubmitAndGetCounter(JobFunction job, JobPriority priority = JobPriority::Normal);

    //!@}

    //------------------------------------------------------------------------
    //! @name 高度なジョブ投入
    //------------------------------------------------------------------------
    //!@{

    //! @brief ジョブ記述子でジョブを投入
    //! @param desc ジョブ記述子
    //! @return ジョブハンドル（依存関係設定や待機に使用）
    [[nodiscard]] JobHandle SubmitJob(JobDesc desc);

    //! @brief 複数ジョブを一括投入
    //! @param descs ジョブ記述子の配列
    //! @return ジョブハンドルの配列
    [[nodiscard]] std::vector<JobHandle> SubmitJobs(std::vector<JobDesc> descs);

    //!@}

    //------------------------------------------------------------------------
    //! @name メインスレッドジョブ
    //------------------------------------------------------------------------
    //!@{

    //! @brief メインスレッドジョブを処理（メインスレッドから呼び出し）
    //! @param maxJobs 処理する最大ジョブ数（0で全て）
    //! @return 処理したジョブ数
    uint32_t ProcessMainThreadJobs(uint32_t maxJobs = 0);

    //! @brief 現在のスレッドがメインスレッドか
    [[nodiscard]] bool IsMainThread() const noexcept;

    //!@}

    //------------------------------------------------------------------------
    //! @name フレーム同期
    //------------------------------------------------------------------------
    //!@{

    //! @brief フレーム開始（フレームカウンターをリセット）
    void BeginFrame();

    //! @brief フレーム終了（High優先度ジョブの完了を待機）
    void EndFrame();

    //! @brief 全ジョブの完了を待機（シーン遷移時など）
    void WaitAll();

    //!@}

    //------------------------------------------------------------------------
    //! @name 並列ループ
    //------------------------------------------------------------------------
    //!@{

    void ParallelFor(uint32_t begin, uint32_t end,
                     const std::function<void(uint32_t)>& func,
                     uint32_t granularity = 0);

    void ParallelForRange(uint32_t begin, uint32_t end,
                          const std::function<void(uint32_t, uint32_t)>& func,
                          uint32_t granularity = 0);

    //!@}

    //------------------------------------------------------------------------
    //! @name 状態取得
    //------------------------------------------------------------------------
    //!@{

    [[nodiscard]] uint32_t GetWorkerCount() const noexcept;
    [[nodiscard]] bool IsWorkerThread() const noexcept;
    [[nodiscard]] uint32_t GetPendingJobCount() const noexcept;
    [[nodiscard]] uint32_t GetMainThreadJobCount() const noexcept;

    //!@}

    //------------------------------------------------------------------------
    //! @name プロファイリング（デバッグビルドのみ）
    //------------------------------------------------------------------------
    //!@{

#ifdef _DEBUG
    //! @brief プロファイリングコールバック型
    using ProfileCallback = std::function<void(std::string_view jobName, float durationMs)>;

    //! @brief プロファイリングコールバックを設定
    void SetProfileCallback(ProfileCallback callback);

    //! @brief 統計情報取得
    struct Stats {
        uint64_t totalJobsExecuted = 0;
        uint64_t totalJobsStolen = 0;  // Work-Stealing統計
        float averageJobDurationMs = 0.0f;
    };
    [[nodiscard]] Stats GetStats() const noexcept;
#endif

    //!@}

    ~JobSystem();

private:
    JobSystem() = default;

    void Initialize(uint32_t numWorkers);
    void Shutdown();

    class Impl;
    std::unique_ptr<Impl> impl_;

    static inline std::unique_ptr<JobSystem> instance_ = nullptr;
};
