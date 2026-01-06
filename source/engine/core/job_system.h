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
#include <string>
#include <string_view>

//! @brief ジョブ優先度
enum class JobPriority : uint8_t {
    High = 0,    //!< 高優先度（フレーム内で必ず完了）
    Normal = 1,  //!< 通常
    Low = 2,     //!< 低優先度（バックグラウンド処理）
    Count = 3
};

//! @brief ジョブ実行結果
enum class JobResult : uint8_t {
    Pending = 0,    //!< 未完了（実行中または待機中）
    Success = 1,    //!< 正常完了
    Cancelled = 2,  //!< キャンセルされた
    Exception = 3   //!< 例外が発生した
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

    void Increment() noexcept;
    void Decrement() noexcept;
    void Wait() const noexcept;
    [[nodiscard]] bool IsComplete() const noexcept;
    [[nodiscard]] uint32_t GetCount() const noexcept;
    void Reset(uint32_t count) noexcept;

    //! @brief 結果を設定
    void SetResult(JobResult result) noexcept;

    //! @brief 結果を取得
    [[nodiscard]] JobResult GetResult() const noexcept;

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

    //! @brief ジョブの実行結果を取得
    [[nodiscard]] JobResult GetResult() const noexcept {
        return counter_ ? counter_->GetResult() : JobResult::Pending;
    }

    //! @brief エラーが発生したか
    [[nodiscard]] bool HasError() const noexcept {
        if (!counter_) return false;
        JobResult result = counter_->GetResult();
        return result == JobResult::Cancelled || result == JobResult::Exception;
    }

    //! @brief 成功したか
    [[nodiscard]] bool IsSuccess() const noexcept {
        return counter_ && counter_->GetResult() == JobResult::Success;
    }

private:
    friend class JobSystem;
    friend class JobDesc;

    //! @brief 内部カウンター取得（内部使用のみ）
    [[nodiscard]] JobCounterPtr GetCounter() const noexcept { return counter_; }

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

    //------------------------------------------------------------------------
    //! @name ファクトリ関数
    //------------------------------------------------------------------------
    //!@{

    //! @brief メインスレッドジョブを作成
    //! @code
    //!   JobSystem::Get().SubmitJob(JobDesc::MainThread([]{ UploadToGPU(); }));
    //! @endcode
    [[nodiscard]] static JobDesc MainThread(JobFunction func) {
        return JobDesc(std::move(func)).SetMainThreadOnly();
    }

    //! @brief 高優先度ジョブを作成
    [[nodiscard]] static JobDesc HighPriority(JobFunction func) {
        return JobDesc(std::move(func)).SetPriority(JobPriority::High);
    }

    //! @brief 低優先度ジョブを作成（バックグラウンド処理用）
    [[nodiscard]] static JobDesc LowPriority(JobFunction func) {
        return JobDesc(std::move(func)).SetPriority(JobPriority::Low);
    }

    //! @brief 依存関係付きジョブを作成
    //! @code
    //!   auto load = JobSystem::Get().SubmitJob(JobDesc([]{ Load(); }));
    //!   auto process = JobSystem::Get().SubmitJob(JobDesc::After(load, []{ Process(); }));
    //! @endcode
    [[nodiscard]] static JobDesc After(const JobHandle& dependency, JobFunction func) {
        return JobDesc(std::move(func)).AddDependency(dependency);
    }

    //! @brief 複数依存関係付きジョブを作成
    [[nodiscard]] static JobDesc AfterAll(const std::vector<JobHandle>& dependencies, JobFunction func) {
        return JobDesc(std::move(func)).AddDependencies(dependencies);
    }

    //! @brief キャンセル可能ジョブを作成（トークン自動生成）
    //! @param func キャンセル対応関数
    //! @param outToken 生成されたトークンを受け取るポインタ（省略可）
    //! @code
    //!   CancelTokenPtr token;
    //!   auto handle = JobSystem::Get().SubmitJob(
    //!       JobDesc::Cancellable([](const CancelToken& ct) {
    //!           while (!ct.IsCancelled()) { DoWork(); }
    //!       }, &token));
    //!   token->Cancel();  // キャンセル
    //! @endcode
    [[nodiscard]] static JobDesc Cancellable(CancellableJobFunction func, CancelTokenPtr* outToken = nullptr) {
        auto token = std::make_shared<CancelToken>();
        if (outToken) *outToken = token;
        return JobDesc().SetCancellableFunction(std::move(func)).SetCancelToken(std::move(token));
    }

    //!@}

    //------------------------------------------------------------------------
    //! @name ビルダーメソッド
    //------------------------------------------------------------------------
    //!@{

    //! @brief ジョブ関数を設定
    //! @note SetCancellableFunctionと排他。両方設定不可
    JobDesc& SetFunction(JobFunction func) {
        assert(!cancellableFunction_ && "SetFunction and SetCancellableFunction are mutually exclusive");
        function_ = std::move(func);
        return *this;
    }

    //! @brief キャンセル対応ジョブ関数を設定
    //! @note SetFunctionと排他。両方設定不可
    //! @note SetCancelToken()も必ず呼び出すこと
    JobDesc& SetCancellableFunction(CancellableJobFunction func) {
        assert(!function_ && "SetFunction and SetCancellableFunction are mutually exclusive");
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

    //!@}

private:
    friend class JobSystem;

    JobFunction function_;
    CancellableJobFunction cancellableFunction_;
    JobPriority priority_ = JobPriority::Normal;
    std::vector<JobCounterPtr> dependencies_;
    CancelTokenPtr cancelToken_;
    bool mainThreadOnly_ = false;
#ifdef _DEBUG
    std::string name_;
#endif
};

//============================================================================
//! @brief キャンセルトークンを作成するヘルパー関数
//! @code
//!   auto token = MakeCancelToken();
//!   JobSystem::Get().SubmitJob(
//!       JobDesc().SetCancellableFunction([](const CancelToken& ct) {
//!           while (!ct.IsCancelled()) { DoWork(); }
//!       }).SetCancelToken(token));
//!   token->Cancel();
//! @endcode
//============================================================================
[[nodiscard]] inline CancelTokenPtr MakeCancelToken() {
    return std::make_shared<CancelToken>();
}

//============================================================================
//! @brief ジョブシステムインターフェース
//!
//! テスト用モックや異なる実装への差し替えを可能にする。
//============================================================================
class IJobSystem
{
public:
    virtual ~IJobSystem() = default;

    //------------------------------------------------------------------------
    //! @name ジョブ投入
    //------------------------------------------------------------------------
    //!@{
    virtual void Submit(JobFunction job, JobPriority priority = JobPriority::Normal) = 0;
    [[nodiscard]] virtual JobHandle SubmitJob(JobDesc desc) = 0;
    [[nodiscard]] virtual std::vector<JobHandle> SubmitJobs(std::vector<JobDesc> descs) = 0;
    //!@}

    //------------------------------------------------------------------------
    //! @name メインスレッドジョブ
    //------------------------------------------------------------------------
    //!@{
    virtual uint32_t ProcessMainThreadJobs(uint32_t maxJobs = 0) = 0;
    [[nodiscard]] virtual bool IsMainThread() const noexcept = 0;
    //!@}

    //------------------------------------------------------------------------
    //! @name フレーム同期
    //------------------------------------------------------------------------
    //!@{
    virtual void BeginFrame() = 0;
    virtual void EndFrame() = 0;
    virtual void WaitAll() = 0;
    //!@}

    //------------------------------------------------------------------------
    //! @name 並列ループ
    //------------------------------------------------------------------------
    //!@{
    [[nodiscard]] virtual JobHandle ParallelFor(uint32_t begin, uint32_t end,
                                                const std::function<void(uint32_t)>& func,
                                                uint32_t granularity = 0) = 0;
    [[nodiscard]] virtual JobHandle ParallelForRange(uint32_t begin, uint32_t end,
                                                     const std::function<void(uint32_t, uint32_t)>& func,
                                                     uint32_t granularity = 0) = 0;
    //!@}

    //------------------------------------------------------------------------
    //! @name 状態取得
    //------------------------------------------------------------------------
    //!@{
    [[nodiscard]] virtual uint32_t GetWorkerCount() const noexcept = 0;
    [[nodiscard]] virtual bool IsWorkerThread() const noexcept = 0;
    [[nodiscard]] virtual uint32_t GetPendingJobCount() const noexcept = 0;
    [[nodiscard]] virtual uint32_t GetMainThreadJobCount() const noexcept = 0;
    //!@}

protected:
    IJobSystem() = default;
    IJobSystem(const IJobSystem&) = delete;
    IJobSystem& operator=(const IJobSystem&) = delete;
};

//============================================================================
//! @brief ジョブシステム実装（シングルトン）
//!
//! ワーカースレッドプールを管理し、ジョブを並列実行する。
//!
//! @note 使用例:
//! @code
//!   // 単純なジョブ
//!   JobSystem::Get().Submit([]{ DoWork(); });
//!
//!   // 依存関係付きジョブ（ファクトリ関数使用）
//!   auto load = JobSystem::Get().SubmitJob(JobDesc([]{ LoadMesh(); }));
//!   auto process = JobSystem::Get().SubmitJob(JobDesc::After(load, []{ ProcessMesh(); }));
//!   process.Wait();
//!
//!   // メインスレッドジョブ（ファクトリ関数使用）
//!   JobSystem::Get().SubmitJob(JobDesc::MainThread([]{ UploadToGPU(); }));
//!
//!   // 高優先度/低優先度ジョブ
//!   JobSystem::Get().SubmitJob(JobDesc::HighPriority([]{ CriticalWork(); }));
//!   JobSystem::Get().SubmitJob(JobDesc::LowPriority([]{ BackgroundWork(); }));
//!
//!   // キャンセル可能ジョブ（ファクトリ関数使用）
//!   CancelTokenPtr token;
//!   auto handle = JobSystem::Get().SubmitJob(
//!       JobDesc::Cancellable([](const CancelToken& ct) {
//!           while (!ct.IsCancelled()) { DoWork(); }
//!       }, &token));
//!   token->Cancel();  // キャンセル要求
//!
//!   // 結果チェック
//!   handle.Wait();
//!   if (handle.HasError()) { /* エラー処理 */ }
//!
//!   // ゲームループ統合
//!   void GameLoop() {
//!       JobSystem::Get().BeginFrame();
//!       // ジョブ投入...
//!       JobSystem::Get().EndFrame();  // フレーム内ジョブ完了待機
//!   }
//! @endcode
//============================================================================
class JobSystem final : public IJobSystem
{
public:
    //! @brief インターフェース経由でアクセス（推奨）
    static IJobSystem& Get() noexcept {
        assert(instance_ && "JobSystem::Create() must be called first");
        return *instance_;
    }

    //! @brief 具象クラスでアクセス（プロファイリング等）
    static JobSystem& GetConcrete() noexcept {
        assert(instance_ && "JobSystem::Create() must be called first");
        return *instance_;
    }

    static void Create(uint32_t numWorkers = 0);
    static void Destroy();
    [[nodiscard]] static bool IsCreated() noexcept { return instance_ != nullptr; }

    //------------------------------------------------------------------------
    // IJobSystem 実装
    //------------------------------------------------------------------------

    void Submit(JobFunction job, JobPriority priority = JobPriority::Normal) override;
    [[nodiscard]] JobHandle SubmitJob(JobDesc desc) override;
    [[nodiscard]] std::vector<JobHandle> SubmitJobs(std::vector<JobDesc> descs) override;

    uint32_t ProcessMainThreadJobs(uint32_t maxJobs = 0) override;
    [[nodiscard]] bool IsMainThread() const noexcept override;

    void BeginFrame() override;
    void EndFrame() override;
    void WaitAll() override;

    [[nodiscard]] JobHandle ParallelFor(uint32_t begin, uint32_t end,
                                        const std::function<void(uint32_t)>& func,
                                        uint32_t granularity = 0) override;
    [[nodiscard]] JobHandle ParallelForRange(uint32_t begin, uint32_t end,
                                             const std::function<void(uint32_t, uint32_t)>& func,
                                             uint32_t granularity = 0) override;

    [[nodiscard]] uint32_t GetWorkerCount() const noexcept override;
    [[nodiscard]] bool IsWorkerThread() const noexcept override;
    [[nodiscard]] uint32_t GetPendingJobCount() const noexcept override;
    [[nodiscard]] uint32_t GetMainThreadJobCount() const noexcept override;

    //------------------------------------------------------------------------
    //! @name プロファイリング（デバッグビルドのみ、具象クラス専用）
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

    ~JobSystem() override;

private:
    JobSystem() = default;

    void Initialize(uint32_t numWorkers);
    void Shutdown();

    class Impl;
    std::unique_ptr<Impl> impl_;

    static inline std::unique_ptr<JobSystem> instance_ = nullptr;
};
