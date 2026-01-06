//----------------------------------------------------------------------------
//! @file   job_system.cpp
//! @brief  マルチスレッドジョブシステム 実装
//----------------------------------------------------------------------------

// Windows.hのmin/maxマクロを無効化（最初に定義）
#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <Windows.h>
#endif

#include "job_system.h"
#include "common/logging/logging.h"

#include <thread>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <vector>
#include <deque>
#include <algorithm>
#include <chrono>

// マクロ干渉回避用
#undef max
#undef min

//----------------------------------------------------------------------------
// JobCounter::Impl 実装
//----------------------------------------------------------------------------

class JobCounter::Impl
{
public:
    Impl() = default;
    explicit Impl(uint32_t initialCount) : count_(initialCount) {}

    void Increment() noexcept
    {
        std::unique_lock<std::mutex> lock(mutex_);
        ++count_;
    }

    void Decrement() noexcept
    {
        std::unique_lock<std::mutex> lock(mutex_);
        if (count_ > 0) {
            --count_;
            if (count_ == 0) {
                cv_.notify_all();
            }
        }
    }

    void Wait() const noexcept
    {
        std::unique_lock<std::mutex> lock(mutex_);
        cv_.wait(lock, [this] { return count_ == 0; });
    }

    [[nodiscard]] bool IsComplete() const noexcept
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return count_ == 0;
    }

    [[nodiscard]] uint32_t GetCount() const noexcept
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return count_;
    }

    void Reset(uint32_t count) noexcept
    {
        std::unique_lock<std::mutex> lock(mutex_);
        count_ = count;
        result_ = JobResult::Pending;
    }

    void SetResult(JobResult result) noexcept
    {
        std::unique_lock<std::mutex> lock(mutex_);
        // エラー状態（Exception/Cancelled）は上書きしない
        // Pending → Success/Exception/Cancelled は許可
        // Success → Exception/Cancelled は許可（エラーへの遷移）
        // Exception/Cancelled → 他への遷移は不許可
        if (result_ == JobResult::Exception || result_ == JobResult::Cancelled) {
            return;  // エラー状態を保持
        }
        result_ = result;
    }

    [[nodiscard]] JobResult GetResult() const noexcept
    {
        std::unique_lock<std::mutex> lock(mutex_);
        return result_;
    }

private:
    mutable std::mutex mutex_;
    mutable std::condition_variable cv_;
    uint32_t count_ = 0;
    JobResult result_ = JobResult::Pending;
};

//----------------------------------------------------------------------------
// JobCounter 実装
//----------------------------------------------------------------------------

JobCounter::JobCounter() : impl_(std::make_unique<Impl>()) {}
JobCounter::JobCounter(uint32_t initialCount) : impl_(std::make_unique<Impl>(initialCount)) {}
JobCounter::~JobCounter() = default;

void JobCounter::Increment() noexcept { impl_->Increment(); }
void JobCounter::Decrement() noexcept { impl_->Decrement(); }
void JobCounter::Wait() const noexcept { impl_->Wait(); }
bool JobCounter::IsComplete() const noexcept { return impl_->IsComplete(); }
uint32_t JobCounter::GetCount() const noexcept { return impl_->GetCount(); }
void JobCounter::Reset(uint32_t count) noexcept { impl_->Reset(count); }
void JobCounter::SetResult(JobResult result) noexcept { impl_->SetResult(result); }
JobResult JobCounter::GetResult() const noexcept { return impl_->GetResult(); }

//----------------------------------------------------------------------------
// JobSystem::Impl
//----------------------------------------------------------------------------

class JobSystem::Impl
{
public:
    //! @brief 内部ジョブデータ
    struct InternalJob {
        JobFunction function;
        CancellableJobFunction cancellableFunction;
        JobCounterPtr counter;
        std::vector<JobCounterPtr> dependencies;
        CancelTokenPtr cancelToken;
        bool mainThreadOnly = false;
        bool countedInFrame = false;  // フレームカウンターにカウントされているか
#ifdef _DEBUG
        std::string name;
#endif
    };

    //! @brief 現在のスレッドのワーカーID（-1 = 非ワーカー）
    static inline thread_local int32_t currentWorkerId_ = -1;

    Impl() : mainThreadId_(std::this_thread::get_id()) {}
    ~Impl() { Shutdown(); }

    void Initialize(uint32_t numWorkers)
    {
        if (running_) return;

        // ワーカー数を決定（0なら論理コア数-1、最低1）
        if (numWorkers == 0) {
            numWorkers = std::max(1u, std::thread::hardware_concurrency() - 1);
        }

        running_ = true;

        // Work-Stealing用のローカルキューを各ワーカーに割り当て
        localQueues_.resize(numWorkers);
        localQueueMutexes_ = std::make_unique<std::mutex[]>(numWorkers);
        workers_.reserve(numWorkers);

        for (uint32_t i = 0; i < numWorkers; ++i) {
            workers_.emplace_back(&Impl::WorkerThread, this, i);
        }

        LOG_INFO("[JobSystem] 初期化完了: ワーカースレッド数=" + std::to_string(numWorkers));
    }

    void Shutdown()
    {
        if (!running_) return;

        {
            std::unique_lock<std::mutex> lock(globalMutex_);
            running_ = false;
        }
        globalCondition_.notify_all();

        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
        workers_.clear();
        localQueues_.clear();
        localQueueMutexes_.reset();

        // 残っているジョブをクリア
        for (int i = 0; i < static_cast<int>(JobPriority::Count); ++i) {
            globalQueues_[i].clear();
        }
        mainThreadQueue_.clear();

        LOG_INFO("[JobSystem] シャットダウン完了");
    }

    //------------------------------------------------------------------------
    // 基本ジョブ投入
    //------------------------------------------------------------------------

    void Submit(JobFunction job, JobPriority priority)
    {
        Submit(std::move(job), nullptr, priority);
    }

    void Submit(JobFunction job, JobCounterPtr counter, JobPriority priority)
    {
        InternalJob internalJob;
        internalJob.function = std::move(job);
        internalJob.counter = std::move(counter);
        EnqueueJob(std::move(internalJob), priority, false);
    }

    JobCounterPtr SubmitAndGetCounter(JobFunction job, JobPriority priority)
    {
        auto counter = std::make_shared<JobCounter>(1);
        Submit(std::move(job), counter, priority);
        return counter;
    }

    //------------------------------------------------------------------------
    // 高度なジョブ投入
    //------------------------------------------------------------------------

    JobHandle SubmitJob(JobDesc desc)
    {
        auto counter = std::make_shared<JobCounter>(1);

        InternalJob job;
        job.function = std::move(desc.function_);
        job.cancellableFunction = std::move(desc.cancellableFunction_);
        job.counter = counter;
        job.dependencies = std::move(desc.dependencies_);
        job.cancelToken = std::move(desc.cancelToken_);
        job.mainThreadOnly = desc.mainThreadOnly_;
#ifdef _DEBUG
        job.name = desc.name_;
#endif

        // フレームカウンターに追加（High優先度のみ）
        if (desc.priority_ == JobPriority::High) {
            std::unique_lock<std::mutex> lock(frameMutex_);
            if (frameCounter_) {
                frameCounter_->Increment();
                job.countedInFrame = true;
            }
        }

        EnqueueJob(std::move(job), desc.priority_, desc.mainThreadOnly_);
        return JobHandle(counter);
    }

    std::vector<JobHandle> SubmitJobs(std::vector<JobDesc> descs)
    {
        std::vector<JobHandle> handles;
        handles.reserve(descs.size());
        for (auto& desc : descs) {
            handles.push_back(SubmitJob(std::move(desc)));
        }
        return handles;
    }

    //------------------------------------------------------------------------
    // メインスレッドジョブ
    //------------------------------------------------------------------------

    uint32_t ProcessMainThreadJobs(uint32_t maxJobs)
    {
        if (std::this_thread::get_id() != mainThreadId_) {
            return 0;
        }

        uint32_t processed = 0;
        while (maxJobs == 0 || processed < maxJobs) {
            InternalJob job;
            {
                std::unique_lock<std::mutex> lock(mainThreadMutex_);
                if (mainThreadQueue_.empty()) break;
                job = std::move(mainThreadQueue_.front());
                mainThreadQueue_.pop_front();
            }

            ExecuteJob(job);
            ++processed;
        }
        return processed;
    }

    bool IsMainThread() const noexcept
    {
        return std::this_thread::get_id() == mainThreadId_;
    }

    uint32_t GetMainThreadJobCount() const noexcept
    {
        std::unique_lock<std::mutex> lock(mainThreadMutex_);
        return static_cast<uint32_t>(mainThreadQueue_.size());
    }

    //------------------------------------------------------------------------
    // フレーム同期
    //------------------------------------------------------------------------

    void BeginFrame()
    {
        std::unique_lock<std::mutex> lock(frameMutex_);
        frameCounter_ = std::make_shared<JobCounter>(0);
    }

    void EndFrame()
    {
        // メインスレッドジョブを処理
        ProcessMainThreadJobs(0);

        // フレームカウンターが完了するまで待機
        JobCounterPtr counter;
        {
            std::unique_lock<std::mutex> lock(frameMutex_);
            counter = frameCounter_;
        }
        if (counter) {
            counter->Wait();
        }
    }

    void WaitAll()
    {
        // 全キューが空になるまで待機
        while (true) {
            ProcessMainThreadJobs(0);

            std::unique_lock<std::mutex> lock(globalMutex_);
            // ロック保持中なのでHasPendingJobsLocked()を直接使用（デッドロック回避）
            if (!HasPendingJobsLocked() && !HasLocalJobs() && mainThreadQueue_.empty()) {
                break;
            }
            // 少し待ってから再チェック
            lock.unlock();
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }

    //------------------------------------------------------------------------
    // 並列ループ
    //------------------------------------------------------------------------

    [[nodiscard]] JobHandle ParallelFor(uint32_t begin, uint32_t end,
                                         const std::function<void(uint32_t)>& func,
                                         uint32_t granularity)
    {
        if (begin >= end) return JobHandle();

        uint32_t count = end - begin;

        if (granularity == 0) {
            uint32_t numJobs = std::max(1u, static_cast<uint32_t>(workers_.size()) * 2);
            granularity = std::max(1u, count / numJobs);
        }

        uint32_t numJobs = (count + granularity - 1) / granularity;
        auto counter = std::make_shared<JobCounter>(numJobs);

        for (uint32_t i = 0; i < numJobs; ++i) {
            uint32_t jobBegin = begin + i * granularity;
            uint32_t jobEnd = std::min(jobBegin + granularity, end);

            Submit([func, jobBegin, jobEnd] {
                for (uint32_t j = jobBegin; j < jobEnd; ++j) {
                    func(j);
                }
                // 結果はExecuteJobInternalで設定される
            }, counter, JobPriority::Normal);
        }

        return JobHandle(counter);
    }

    [[nodiscard]] JobHandle ParallelForRange(uint32_t begin, uint32_t end,
                                              const std::function<void(uint32_t, uint32_t)>& func,
                                              uint32_t granularity)
    {
        if (begin >= end) return JobHandle();

        uint32_t count = end - begin;

        if (granularity == 0) {
            uint32_t numJobs = std::max(1u, static_cast<uint32_t>(workers_.size()) * 2);
            granularity = std::max(1u, count / numJobs);
        }

        uint32_t numJobs = (count + granularity - 1) / granularity;
        auto counter = std::make_shared<JobCounter>(numJobs);

        for (uint32_t i = 0; i < numJobs; ++i) {
            uint32_t jobBegin = begin + i * granularity;
            uint32_t jobEnd = std::min(jobBegin + granularity, end);

            Submit([func, jobBegin, jobEnd] {
                func(jobBegin, jobEnd);
                // 結果はExecuteJobInternalで設定される
            }, counter, JobPriority::Normal);
        }

        return JobHandle(counter);
    }

    //------------------------------------------------------------------------
    // 状態取得
    //------------------------------------------------------------------------

    [[nodiscard]] uint32_t GetWorkerCount() const noexcept
    {
        return static_cast<uint32_t>(workers_.size());
    }

    [[nodiscard]] static bool IsWorkerThread() noexcept
    {
        return currentWorkerId_ >= 0;
    }

    [[nodiscard]] uint32_t GetPendingJobCount() const noexcept
    {
        return pendingJobs_.load(std::memory_order_acquire);
    }

    //------------------------------------------------------------------------
    // プロファイリング
    //------------------------------------------------------------------------

#ifdef _DEBUG
    void SetProfileCallback(JobSystem::ProfileCallback callback)
    {
        std::unique_lock<std::mutex> lock(profileMutex_);
        profileCallback_ = std::move(callback);
    }

    JobSystem::Stats GetStats() const noexcept
    {
        return stats_;
    }
#endif

private:
    void EnqueueJob(InternalJob job, JobPriority priority, bool mainThread)
    {
        if (mainThread) {
            std::unique_lock<std::mutex> lock(mainThreadMutex_);
            mainThreadQueue_.push_back(std::move(job));
        } else {
            // ワーカースレッドからの投入はローカルキューへ（Work-Stealing用）
            int32_t workerId = currentWorkerId_;
            if (workerId >= 0 && workerId < static_cast<int32_t>(localQueues_.size())) {
                std::unique_lock<std::mutex> lock(localQueueMutexes_[workerId]);
                localQueues_[workerId].push_back(std::move(job));
                ++pendingJobs_;
            } else {
                // 非ワーカースレッドからはグローバルキューへ
                std::unique_lock<std::mutex> lock(globalMutex_);
                globalQueues_[static_cast<int>(priority)].push_back(std::move(job));
                ++pendingJobs_;
            }
        }
        globalCondition_.notify_one();
    }

    //! @brief 1つのジョブを取得して実行（待機中のヘルプ用）
    bool TryExecuteOneJob()
    {
        InternalJob job;
        int32_t workerId = currentWorkerId_;

        // 1. ローカルキューから取得
        if (workerId >= 0 && workerId < static_cast<int32_t>(localQueues_.size())) {
            std::unique_lock<std::mutex> lock(localQueueMutexes_[workerId], std::try_to_lock);
            if (lock.owns_lock() && !localQueues_[workerId].empty()) {
                job = std::move(localQueues_[workerId].front());
                localQueues_[workerId].pop_front();
                --pendingJobs_;
                ExecuteJobInternal(job);
                return true;
            }
        }

        // 2. グローバルキューから取得
        {
            std::unique_lock<std::mutex> lock(globalMutex_, std::try_to_lock);
            if (lock.owns_lock()) {
                for (int i = 0; i < static_cast<int>(JobPriority::Count); ++i) {
                    if (!globalQueues_[i].empty()) {
                        job = std::move(globalQueues_[i].front());
                        globalQueues_[i].pop_front();
                        --pendingJobs_;
                        lock.unlock();
                        ExecuteJobInternal(job);
                        return true;
                    }
                }
            }
        }

        // 3. 他のワーカーから盗む
        if (workerId >= 0) {
            for (size_t i = 0; i < localQueues_.size(); ++i) {
                if (static_cast<int32_t>(i) == workerId) continue;

                std::unique_lock<std::mutex> lock(localQueueMutexes_[i], std::try_to_lock);
                if (!lock.owns_lock()) continue;

                if (!localQueues_[i].empty()) {
                    job = std::move(localQueues_[i].back());  // 後ろから盗む
                    localQueues_[i].pop_back();
                    --pendingJobs_;
#ifdef _DEBUG
                    ++stats_.totalJobsStolen;
#endif
                    lock.unlock();
                    ExecuteJobInternal(job);
                    return true;
                }
            }
        }

        return false;
    }

    //! @brief ジョブの実際の実行（依存関係チェック後）
    void ExecuteJobInternal(InternalJob& job)
    {
        // キャンセルチェック
        if (job.cancelToken && job.cancelToken->IsCancelled()) {
            if (job.counter) {
                job.counter->SetResult(JobResult::Cancelled);
                job.counter->Decrement();
            }
            return;
        }

#ifdef _DEBUG
        auto startTime = std::chrono::high_resolution_clock::now();
#endif

        JobResult result = JobResult::Success;

        // ジョブ実行（function と cancellableFunction は排他）
        try {
            if (job.cancellableFunction) {
                assert(job.cancelToken && "CancellableFunction requires CancelToken");
                job.cancellableFunction(*job.cancelToken);
            } else if (job.function) {
                job.function();
            }
        } catch (...) {
            result = JobResult::Exception;
            // 例外を飲み込む（ワーカースレッドのクラッシュを防ぐ）
            LOG_ERROR("[JobSystem] ジョブ実行中に例外が発生");
        }

#ifdef _DEBUG
        auto endTime = std::chrono::high_resolution_clock::now();
        float durationMs = std::chrono::duration<float, std::milli>(endTime - startTime).count();

        // 統計更新
        ++stats_.totalJobsExecuted;
        stats_.averageJobDurationMs =
            (stats_.averageJobDurationMs * (stats_.totalJobsExecuted - 1) + durationMs)
            / stats_.totalJobsExecuted;

        // プロファイルコールバック
        if (profileCallback_ && !job.name.empty()) {
            std::unique_lock<std::mutex> lock(profileMutex_);
            if (profileCallback_) {
                profileCallback_(job.name, durationMs);
            }
        }
#endif

        // 結果を設定してカウンターをデクリメント
        if (job.counter) {
            job.counter->SetResult(result);
            job.counter->Decrement();
        }

        // フレームカウンターをデクリメント（カウントされたジョブのみ）
        if (job.countedInFrame) {
            std::unique_lock<std::mutex> lock(frameMutex_);
            if (frameCounter_) {
                frameCounter_->Decrement();
            }
        }
    }

    //! @brief ジョブ実行（依存関係待機 + 実行）
    void ExecuteJob(InternalJob& job)
    {
        // 依存関係をチェック（待機中は他のジョブを実行してデッドロック回避）
        for (const auto& dep : job.dependencies) {
            if (dep) {
                while (!dep->IsComplete()) {
                    if (!TryExecuteOneJob()) {
                        // 実行できるジョブがなければ少し待つ
                        std::this_thread::yield();
                    }
                }
            }
        }

        ExecuteJobInternal(job);
    }

    void WorkerThread(uint32_t workerId)
    {
        // このスレッドのワーカーIDを設定
        currentWorkerId_ = static_cast<int32_t>(workerId);

#if defined(_WIN32) && defined(_DEBUG)
        std::wstring name = L"JobWorker_" + std::to_wstring(workerId);
        SetThreadDescription(GetCurrentThread(), name.c_str());
#endif

        while (true) {
            InternalJob job;
            bool gotJob = false;

            // 1. 自分のローカルキューをチェック
            {
                std::unique_lock<std::mutex> lock(localQueueMutexes_[workerId]);
                if (!localQueues_[workerId].empty()) {
                    job = std::move(localQueues_[workerId].front());
                    localQueues_[workerId].pop_front();
                    --pendingJobs_;
                    gotJob = true;
                }
            }

            // 2. グローバルキューをチェック
            if (!gotJob) {
                std::unique_lock<std::mutex> lock(globalMutex_);

                // ジョブがなければ待機
                if (!HasPendingJobsLocked() && !HasLocalJobs()) {
                    globalCondition_.wait(lock, [this] {
                        return !running_ || HasPendingJobsLocked() || HasLocalJobs();
                    });
                }

                if (!running_ && !HasPendingJobsLocked() && !HasLocalJobs()) {
                    return;
                }

                gotJob = TryPopJob(job);
                if (gotJob) {
                    --pendingJobs_;
                }
            }

            // 3. Work-Stealing: 他のワーカーから盗む
            if (!gotJob) {
                gotJob = TryStealJob(job, workerId);
                if (gotJob) {
                    --pendingJobs_;
                }
            }

            if (gotJob) {
                ExecuteJob(job);
            }
        }
    }

    //! @brief グローバルキューにジョブがあるか（ロック保持前提）
    [[nodiscard]] bool HasPendingJobsLocked() const noexcept
    {
        for (int i = 0; i < static_cast<int>(JobPriority::Count); ++i) {
            if (!globalQueues_[i].empty()) {
                return true;
            }
        }
        return false;
    }

    //! @brief いずれかのローカルキューにジョブがあるか
    [[nodiscard]] bool HasLocalJobs() const noexcept
    {
        for (size_t i = 0; i < localQueues_.size(); ++i) {
            // ロックなしでチェック（厳密ではないがウェイクアップ判定用）
            if (!localQueues_[i].empty()) {
                return true;
            }
        }
        return false;
    }

    //! @brief グローバルまたはローカルにジョブがあるか（スレッドセーフ）
    [[nodiscard]] bool HasPendingJobs() const noexcept
    {
        std::unique_lock<std::mutex> lock(globalMutex_);
        return HasPendingJobsLocked() || HasLocalJobs();
    }

    bool TryPopJob(InternalJob& outJob)
    {
        for (int i = 0; i < static_cast<int>(JobPriority::Count); ++i) {
            if (!globalQueues_[i].empty()) {
                outJob = std::move(globalQueues_[i].front());
                globalQueues_[i].pop_front();
                return true;
            }
        }
        return false;
    }

    bool TryStealJob(InternalJob& outJob, uint32_t thiefId)
    {
        // 他のワーカーのローカルキューから盗む
        for (size_t i = 0; i < localQueues_.size(); ++i) {
            if (i == thiefId) continue;

            std::unique_lock<std::mutex> lock(localQueueMutexes_[i], std::try_to_lock);
            if (!lock.owns_lock()) continue;

            if (!localQueues_[i].empty()) {
                outJob = std::move(localQueues_[i].back());
                localQueues_[i].pop_back();
#ifdef _DEBUG
                ++stats_.totalJobsStolen;
#endif
                return true;
            }
        }
        return false;
    }

    // スレッド管理
    std::vector<std::thread> workers_;
    std::thread::id mainThreadId_;

    // グローバルキュー（優先度別）
    std::deque<InternalJob> globalQueues_[static_cast<int>(JobPriority::Count)];
    mutable std::mutex globalMutex_;
    std::condition_variable globalCondition_;

    // ローカルキュー（Work-Stealing用）
    std::vector<std::deque<InternalJob>> localQueues_;
    std::unique_ptr<std::mutex[]> localQueueMutexes_;

    // メインスレッドキュー
    std::deque<InternalJob> mainThreadQueue_;
    mutable std::mutex mainThreadMutex_;

    // フレーム同期
    JobCounterPtr frameCounter_;
    std::mutex frameMutex_;

    // 状態
    std::atomic<uint32_t> pendingJobs_{0};
    bool running_ = false;

#ifdef _DEBUG
    // プロファイリング
    JobSystem::ProfileCallback profileCallback_;
    std::mutex profileMutex_;
    mutable JobSystem::Stats stats_;
#endif
};

//----------------------------------------------------------------------------
// JobSystem シングルトン
//----------------------------------------------------------------------------

void JobSystem::Create(uint32_t numWorkers)
{
    if (!instance_) {
        instance_ = std::unique_ptr<JobSystem>(new JobSystem());
        instance_->Initialize(numWorkers);
    }
}

void JobSystem::Destroy()
{
    if (instance_) {
        instance_->Shutdown();
        instance_.reset();
    }
}

JobSystem::~JobSystem() = default;

void JobSystem::Initialize(uint32_t numWorkers)
{
    impl_ = std::make_unique<Impl>();
    impl_->Initialize(numWorkers);
}

void JobSystem::Shutdown()
{
    if (impl_) {
        impl_->Shutdown();
        impl_.reset();
    }
}

//----------------------------------------------------------------------------
// ジョブ投入
//----------------------------------------------------------------------------

void JobSystem::Submit(JobFunction job, JobPriority priority)
{
    impl_->Submit(std::move(job), priority);
}

JobHandle JobSystem::SubmitJob(JobDesc desc)
{
    return impl_->SubmitJob(std::move(desc));
}

std::vector<JobHandle> JobSystem::SubmitJobs(std::vector<JobDesc> descs)
{
    return impl_->SubmitJobs(std::move(descs));
}

//----------------------------------------------------------------------------
// メインスレッドジョブ
//----------------------------------------------------------------------------

uint32_t JobSystem::ProcessMainThreadJobs(uint32_t maxJobs)
{
    return impl_->ProcessMainThreadJobs(maxJobs);
}

bool JobSystem::IsMainThread() const noexcept
{
    return impl_ ? impl_->IsMainThread() : false;
}

uint32_t JobSystem::GetMainThreadJobCount() const noexcept
{
    return impl_ ? impl_->GetMainThreadJobCount() : 0;
}

//----------------------------------------------------------------------------
// フレーム同期
//----------------------------------------------------------------------------

void JobSystem::BeginFrame()
{
    impl_->BeginFrame();
}

void JobSystem::EndFrame()
{
    impl_->EndFrame();
}

void JobSystem::WaitAll()
{
    impl_->WaitAll();
}

//----------------------------------------------------------------------------
// 並列ループ
//----------------------------------------------------------------------------

JobHandle JobSystem::ParallelFor(uint32_t begin, uint32_t end,
                                 const std::function<void(uint32_t)>& func,
                                 uint32_t granularity)
{
    return impl_->ParallelFor(begin, end, func, granularity);
}

JobHandle JobSystem::ParallelForRange(uint32_t begin, uint32_t end,
                                      const std::function<void(uint32_t, uint32_t)>& func,
                                      uint32_t granularity)
{
    return impl_->ParallelForRange(begin, end, func, granularity);
}

//----------------------------------------------------------------------------
// 状態取得
//----------------------------------------------------------------------------

uint32_t JobSystem::GetWorkerCount() const noexcept
{
    return impl_ ? impl_->GetWorkerCount() : 0;
}

bool JobSystem::IsWorkerThread() const noexcept
{
    return Impl::IsWorkerThread();
}

uint32_t JobSystem::GetPendingJobCount() const noexcept
{
    return impl_ ? impl_->GetPendingJobCount() : 0;
}

//----------------------------------------------------------------------------
// プロファイリング
//----------------------------------------------------------------------------

#ifdef _DEBUG
void JobSystem::SetProfileCallback(ProfileCallback callback)
{
    impl_->SetProfileCallback(std::move(callback));
}

JobSystem::Stats JobSystem::GetStats() const noexcept
{
    return impl_ ? impl_->GetStats() : Stats{};
}
#endif
