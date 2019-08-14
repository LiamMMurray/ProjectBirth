#pragma once
#include <mutex>
inline namespace JobSchedulerInternalUtility
{
        template <class C>
        struct ClosureTraits : public ClosureTraits<decltype(&C::operator())>
        {};
        template <class R, class C, class... Args>
        struct ClosureTraits<R (C::*)(Args... args)>
        {
                using SetableArgs = std::tuple<Args...>;
        };
        template <class R, class C, class... Args>
        struct ClosureTraits<R (C::*)(Args... args) const>
        {
                using SetableArgs = std::tuple<Args...>;
        };

        template <typename CopyConstructableType = void>
        inline void InPlaceForwardConstruct(void* mem)
        {}
        template <typename CopyConstructableType>
        inline void InPlaceForwardConstruct(void* mem, CopyConstructableType&& object)
        {
#pragma push_macro("new");
#undef new
                new (mem) CopyConstructableType(std::forward<CopyConstructableType>(object));
#pragma pop_macro("new");
        }
        template <typename CopyConstructableType, typename... CopyConstructableTypes>
        inline typename std::enable_if<sizeof...(CopyConstructableTypes)>::type
        InPlaceForwardConstruct(void* mem, CopyConstructableType&& object, CopyConstructableTypes&&... objects)
        {
#pragma push_macro("new");
#undef new
                new (mem) CopyConstructableType(std::forward<CopyConstructableType>(object));
#pragma pop_macro("new");
                char* memAlias = static_cast<char*>(mem);
                InPlaceForwardConstruct(memAlias + sizeof(CopyConstructableType),
                                        std::forward<CopyConstructableTypes>(objects)...);
        }

        inline constexpr auto CACHE_LINE_SIZE = std::hardware_destructive_interference_size;

        inline std::mutex DebugPrintMutex;
} // namespace JobSchedulerInternalUtility

#define WIN_32_LEAN_AND_MEAN
#include <Windows.h>
#include <atomic>
#include <thread>
#include <vector>
#include "ContainerUtility.h"
inline namespace JobSchedulerInternal
{
        // The JobSchedulerInternals are a modified implementation of the lock free JobQueue specified by Stefan Reinalter on
        // his blog, molecular-matters. It should be noted that code has been modified/added as seen fit.
        //
        // https://blog.molecular-matters.com/2015/09/25/job-system-2-0-lock-free-work-stealing-part-3-going-lock-free/
        //
        // The MIT License(MIT)
        //
        // Copyright(c) 2012-2017 Stefan Reinalter
        //
        // Permission is hereby granted, free of charge, to any person obtaining a copy
        // of this software and associated documentation files (the "Software"), to deal
        // in the Software without restriction, including without limitation the rights
        // to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
        // copies of the Software, and to permit persons to whom the Software is
        // furnished to do so, subject to the following conditions :
        //
        // The above copyright notice and this permission notice shall be included in
        // all copies or substantial portions of the Software.
        //
        // THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
        // IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
        // FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
        // AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
        // LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
        // OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
        // THE SOFTWARE.
        //

        struct Job
        {
                void (*invokeImpl)(Job*);
                Job*                  parent;
                std::atomic<unsigned> unfinishedJobs;

                static constexpr auto Size               = CACHE_LINE_SIZE;
                static constexpr auto invokeImplSize     = sizeof(invokeImpl);
                static constexpr auto ParentSize         = sizeof(parent);
                static constexpr auto UnfinishedJobsSize = sizeof(unfinishedJobs);
                static constexpr auto PaddingSize        = Size - ParentSize - invokeImplSize - UnfinishedJobsSize;
                char                  buffer[PaddingSize];

                inline void Invoke()
                {
                        invokeImpl(this);
                }
                inline void Reset()
                {
                        parent         = 0;
                        unfinishedJobs = 1;
                }
                inline void Reset(Job* parent)
                {
                        this->parent   = parent;
                        unfinishedJobs = 1;
                        this->parent->unfinishedJobs++;
                }
                inline void Finish()
                {

                        const int32_t _unfinishedJobs = --unfinishedJobs;

                        if ((_unfinishedJobs == 0) && (parent))
                        {
                                parent->Finish();
                        }
                }
        };

        //.
        //	This data structure acts as a normal ring buffer but with atomics and a steal() function
        //	that pop's from the queue concurrently from another thread
        template <unsigned MaxJobs>
        struct JobQueue
        {
                static constexpr unsigned MaxJobs = nextPowerOf2(MaxJobs);
                static constexpr unsigned Mask    = MaxJobs - 1;

                volatile long m_bottom;
                volatile long m_top;
                Job**         m_jobs;
                JobQueue()
                {
                        m_bottom = 0;
                        m_top    = 0;
                        m_jobs   = static_cast<Job**>(_aligned_malloc(MaxJobs * sizeof(Job*), 64));
                }
                ~JobQueue()
                {
                        _aligned_free(m_jobs);
                }

                void Push(Job* job)
                {
                        long b           = m_bottom;
                        m_jobs[b & Mask] = job;

                        // ensure the job is written before b+1 is published to other threads.
                        // on x86/64, a compiler barrier is enough.
                        _ReadWriteBarrier();

                        m_bottom = b + 1;
                }

                Job* Steal(void)
                {
                        long t = m_top;

                        // ensure that top is always read before bottom.
                        // loads will not be reordered with other loads on x86, so a compiler barrier is enough.
                        _ReadWriteBarrier();

                        long b = m_bottom;
                        if (t < b)
                        {
                                // non-empty queue
                                Job* job = m_jobs[t & Mask];

                                // the interlocked function serves as a compiler barrier, and guarantees that the read happens
                                // before the CAS.
                                if (_InterlockedCompareExchange(&m_top, t + 1, t) != t)
                                {
                                        // a concurrent steal or pop operation removed an element from the deque in the
                                        // meantime.
                                        return 0;
                                }

                                return job;
                        }
                        else
                        {
                                // empty queue
                                return 0;
                        }
                }

                Job* Pop(void)
                {
                        long b   = m_bottom - 1;
                        m_bottom = b;

                        long t = m_top;
                        if (t <= b)
                        {
                                // non-empty queue
                                Job* job = m_jobs[b & Mask];
                                if (t != b)
                                {
                                        // there's still more than one item left in the queue
                                        return job;
                                }

                                // this is the last item in the queue
                                if (_InterlockedCompareExchange(&m_top, t + 1, t) != t)
                                {
                                        // failed race against steal operation
                                        job = 0;
                                }

                                m_bottom = t + 1;
                                return job;
                        }
                        else
                        {
                                // deque was already empty
                                m_bottom = t;
                                return 0;
                        }
                }
        };


        inline DWORD              TLSIndex_GenericIndex = TlsAlloc();
        inline auto               NumWorkerThreads{std::thread::hardware_concurrency() - 1};
        inline constexpr unsigned MaxJobs          = 1024;
        inline volatile bool      RunWorkerThreads = true;

        inline AllocVector<RingBuffer<Job, MaxJobs>> TempJobAllocatorImpl;
        inline AllocVector<RingBuffer<Job, MaxJobs>> StaticJobAllocatorImpl;

        inline AllocVector<std::thread, CACHE_LINE_SIZE> WorkerThreads;
        inline AllocVector<JobQueue<MaxJobs>>            JobQueues;
        inline bool                                             IsEmpty(Job* job)
        {
                return !job;
        }

        inline const unsigned GetThreadIndex()
        {
                return (unsigned)TlsGetValue(TLSIndex_GenericIndex);
        }

        template <typename Allocator = TempJobAllocator, typename CallableType, typename... Args>
        inline Job* CreateJobData(CallableType&& callable, Args&&... args)
        {
                Job* thisJob = Allocator::Allocate();

                static_assert(sizeof(std::tuple<Args&&...>) + sizeof(CallableType &&) <= Job::PaddingSize,
                              "lambda is too large to fit the Job's padding buffer");
                char* bufferAlias = thisJob->buffer;
                InPlaceForwardConstruct(bufferAlias, std::forward<CallableType>(callable));
                InPlaceForwardConstruct(bufferAlias + sizeof(CallableType), std::tuple<Args&&...>(std::forward<Args>(args)...));

                thisJob->invokeImpl = [](Job* job) {
                        char*         bufferAlias    = job->buffer;
                        CallableType& callableObject = *reinterpret_cast<CallableType*>(bufferAlias);
                        using ArgsTupleType          = std::tuple<Args&&...>;

                        bufferAlias += sizeof(CallableType);
                        ArgsTupleType& argsTuple = *reinterpret_cast<ArgsTupleType*>(bufferAlias);
                        std::apply(callableObject, std::forward<ArgsTupleType>(argsTuple));

                        callableObject.~CallableType();
                        argsTuple.~ArgsTupleType();
                };

                return thisJob;
        }

        inline JobQueue<MaxJobs>& GetStealJobQueue()
        {
                unsigned WorkerThreadIndex = rand() % (NumWorkerThreads + 1);
                return JobQueues[WorkerThreadIndex];
        }

        inline void Launch(Job* job)
        {
                unsigned InactiveJobQueueIndex = NumWorkerThreads;
                JobQueues[InactiveJobQueueIndex].Push(job);
        }

        inline Job* PopOrStealJob()
        {
                JobQueue<MaxJobs>& MyJobQueue = JobQueues[GetThreadIndex()];

                Job* job = MyJobQueue.Pop();
                if (IsEmpty(job))
                {
                        // this is not a valid job because our own queue is empty, so try stealing from some other queue
                        JobQueue<MaxJobs>& StealJobQueue = GetStealJobQueue();

                        if (&StealJobQueue == &MyJobQueue)
                        {
                                // don't try to steal from ourselves
                                Yield();
                                return 0;
                        }

                        Job* stolenJob = StealJobQueue.Steal();
                        if (IsEmpty(stolenJob))
                        {
                                // we couldn't steal a job from the other queue either, so we just yield our time slice for
                                // now
                                Yield();
                                return 0;
                        }

                        return stolenJob;
                }

                return job;
        }

        inline void Wait(Job* job)
        {
                while (job->unfinishedJobs > 0)
                {
                        Job* nextJob = PopOrStealJob();

                        if (nextJob)
                        {
                                nextJob->Invoke();
                                nextJob->Finish();
                        }
                }
        }

        inline void WorkerThreadMain(unsigned ThreadIndex)
        {
                TlsSetValue(TLSIndex_GenericIndex, (LPVOID)ThreadIndex);
                while (RunWorkerThreads)
                {
                        Job* ThisJob = PopOrStealJob();

                        if (ThisJob)
                        {
                                ThisJob->Invoke();
                                ThisJob->Finish();
                        }
                }
        }
        template <typename Allocator, typename R, typename Lambda, typename... Args>
        struct ParallelForJobImpl
        {
            protected:
                Job*              root;
                std::vector<Job*> children;
                ParallelForJobImpl(Lambda&& lambda, unsigned begin, unsigned end, unsigned chunkSize)
                {
                        root     = CreateJobData([]() {});
                        children = CreateParallelForSubJobs(std::forward<Lambda>(lambda), begin, end, chunkSize);
                }

            private:
                template <unsigned... Is>
                static R ParallelForApplyImpl(Lambda&&              lambda,
                                              unsigned              index,
                                              std::tuple<Args...>&& tuple,
                                              std::index_sequence<Is...>)
                {
                        lambda(index, std::get<Is>(tuple)...);
                }

                static void ParallelForApply(Lambda&& lambda, unsigned index, std::tuple<Args...>&& tuple)
                {
                        ParallelForApplyImpl(std::forward<Lambda>(lambda),
                                             index,
                                             std::forward<std::tuple<Args...>>(tuple),
                                             std::make_index_sequence<sizeof...(Args)>{});
                }

                Job* CreateParallelForSubJobImpl(Lambda&& lambda, unsigned begin, unsigned end)
                {
                        Job*  thisJob     = Allocator::Allocate();
                        char* bufferAlias = thisJob->buffer;

                        InPlaceForwardConstruct(bufferAlias, std::forward<Lambda>(lambda));
                        bufferAlias += sizeof(Lambda);

                        InPlaceForwardConstruct(bufferAlias, std::tuple<unsigned, unsigned>(begin, end));

                        thisJob->invokeImpl = [](Job* job) {
                                char* bufferAlias = job->buffer;
                                auto* lambda      = reinterpret_cast<Lambda*>(bufferAlias);
                                bufferAlias += sizeof(Lambda);

                                auto [_begin, _end] = *reinterpret_cast<std::tuple<unsigned, unsigned>*>(bufferAlias);
                                if constexpr (sizeof...(Args))
                                {
                                        bufferAlias += sizeof(std::tuple<unsigned, unsigned>);
                                        auto _args = *reinterpret_cast<std::tuple<Args...>*>(bufferAlias);
                                        for (unsigned i = _begin; i < _end; i++)
                                        {
                                                ParallelForApply(
                                                    std::forward<Lambda>(*lambda), i, std::forward<std::tuple<Args...>>(_args));
                                        }
                                }
                                else
                                {
                                        for (unsigned i = _begin; i < _end; i++)
                                        {
                                                (*lambda)(i);
                                        }
                                }
                        };
                        return thisJob;
                }

                template <typename Lambda>
                std::vector<Job*> CreateParallelForSubJobs(Lambda&& lambda, unsigned begin, unsigned end, unsigned chunkSize)
                {
                        std::vector<Job*> output;
                        if (end - begin <= chunkSize)
                                output.push_back(CreateParallelForSubJobImpl(std::forward<Lambda>(lambda), begin, end));
                        else
                        {
                                unsigned leftBegin  = begin;
                                unsigned leftEnd    = ((end - begin) / 2) + begin;
                                unsigned rightBegin = leftEnd;
                                unsigned rightEnd   = end;

                                auto left =
                                    CreateParallelForSubJobs(std::forward<Lambda>(lambda), leftBegin, leftEnd, chunkSize);

                                auto right =
                                    CreateParallelForSubJobs(std::forward<Lambda>(lambda), rightBegin, rightEnd, chunkSize);

                                output.insert(output.end(), left.begin(), left.end());
                                output.insert(output.end(), right.begin(), right.end());
                        }

                        return output;
                }

                void ResetJobs()
                {
                        root->Reset();
                        for (const auto& itr : children)
                                itr->Reset(root);
                }

                void Launch()
                {
                        JobSchedulerInternal::Launch(root);
                        for (const auto& itr : children)
                                JobSchedulerInternal::Launch(itr);
                }

            public:
                void Wait()
                {
                        JobSchedulerInternal::Wait(root);
                }
                void SetArgs(Args... args)
                {
                        for (const auto& itr : children)
                        {
                                char* bufferAlias = itr->buffer;
                                bufferAlias += sizeof(Lambda) + sizeof(std::tuple<unsigned, unsigned>);
                                auto bufferArgsAlias = reinterpret_cast<std::tuple<Args...>*>(bufferAlias);
                                InPlaceForwardConstruct(bufferArgsAlias, std::tuple(args...));
                        }
                }
                void SetRange(unsigned begin, unsigned end, unsigned chunkSize = 256)
                {
                        char* bufferAlias = children[0]->buffer;
                        auto& lambda      = *reinterpret_cast<Lambda*>(bufferAlias);
                        // bufferAlias += sizeof(Lambda) + sizeof(std::tuple<unsigned, unsigned>);
                        // auto& args = *reinterpret_cast<std::tuple<Args...>*>(bufferAlias);
                        children = CreateParallelForSubJobs(std::forward<Lambda>(lambda), begin, end, chunkSize);
                        // std::apply(SetArgs, args);
                }
                void operator()(Args... args)
                {
                        SetArgs(args...);
                        ResetJobs();
                        Launch();
                }
                Job* GetRootJob()
                {
                        return root;
                }
        };

        template <typename Allocator, typename Lambda>
        struct ParallelForJob : public ParallelForJob<Allocator, decltype(&Lambda::operator())>
        {
                ParallelForJob(Lambda&& lambda, unsigned begin, unsigned end, unsigned chunkSize = 256) :
                    ParallelForJob<Allocator, decltype(&Lambda::operator())>(std::forward<Lambda>(lambda),
                                                                             begin,
                                                                             end,
                                                                             chunkSize)
                {}
                ParallelForJob(Lambda&& lambda) :
                    ParallelForJob<Allocator, decltype(&Lambda::operator())>(std::forward<Lambda>(lambda), 0, 0, 0)
                {}
        };
        template <typename Allocator, typename R, typename Lambda, typename... Args>
        struct ParallelForJob<Allocator, R (Lambda::*)(unsigned, Args...)>
            : public ParallelForJobImpl<Allocator, R, Lambda, Args...>
        {
                ParallelForJob(Lambda&& lambda, unsigned begin, unsigned end, unsigned chunkSize) :
                    ParallelForJobImpl<Allocator, R, Lambda, Args...>(std::forward<Lambda>(lambda), begin, end, chunkSize)
                {}
        };
        template <typename Allocator, typename R, typename Lambda, typename... Args>
        struct ParallelForJob<Allocator, R (Lambda::*)(unsigned, Args...) const>
            : public ParallelForJobImpl<Allocator, R, Lambda, Args...>
        {
                ParallelForJob(Lambda&& lambda, unsigned begin, unsigned end, unsigned chunkSize) :
                    ParallelForJobImpl<Allocator, R, Lambda, Args...>(std::forward<Lambda>(lambda), begin, end, chunkSize)
                {}
        };
} // namespace JobSchedulerInternal
inline namespace JobScheduler
{
        void Initialize();

        void Shutdown();

        struct TempJobAllocator
        {
            private:
                static inline auto& AllocatorImpl = TempJobAllocatorImpl;

            public:
                static inline Job* Allocate()
                {
                        return AllocatorImpl[GetThreadIndex()].Allocate();
                }
        };
        struct StaticJobAllocator
        {
            private:
                static inline auto& AllocatorImpl = StaticJobAllocatorImpl;

            public:
                static inline Job* Allocate()
                {
                        return AllocatorImpl[GetThreadIndex()].Allocate();
                }
        };

        // Usage example:
        //		/*multiplies all values in ArrayBar by 5*/
        //		auto JobFoo = ParallelFor([&ArrayBar](unsigned i, unsigned factor) {ArrayBar[i] *= factor;});
        //		JobFoo.SetRange(0, 1024);
        //		JobFoo(5);
        //		JobFoo.Wait();
        template <typename Allocator = TempJobAllocator, typename Lambda>
        auto ParallelFor(Lambda&& lambda, unsigned begin, unsigned end, unsigned chunkSize = 256)
        {
                return ParallelForJob<Allocator, Lambda>(std::forward<Lambda>(lambda), begin, end, chunkSize);
        }
        template <typename Allocator = TempJobAllocator, typename Lambda>
        auto ParallelFor(Lambda&& lambda)
        {
                return ParallelForJob<Allocator, Lambda>(std::forward<Lambda>(lambda));
        }

        // Usage example:
        //		/*sets integers foo, bar, and baz to 3, 777, and 42, respectively.
        //		TempJob job_A;
        //		TempJob job_A.Append([&foo]() { foo = 3; });
        //		TempJob job_B;
        //		job_B.Append([&bar]() { bar = 777; });
        //		job_B.Append(job_A.GetRootJob());
        //		job_B.Append([&baz](int qux) { baz = qux; }, 42);
        //		job_B();
        //		job_B.Wait();
        template <typename Allocator = TempJobAllocator>
        struct TempJob
        {
            private:
                Job* rootJob;

            public:
                TempJob()
                {
                        rootJob = CreateJobData([]() {});
                        rootJob->Reset();
                }
                template <typename Lambda, typename... Args>
                void Append(Lambda&& lambda, Args&&... args)
                {
                        Job* ThisJob;
                        if constexpr (sizeof...(Args))
                                ThisJob = CreateJobData(std::forward<Lambda>(lambda), std::forward<Args...>(args...));
                        else
                                ThisJob = CreateJobData(std::forward<Lambda>(lambda));

                        ThisJob->Reset(rootJob);
                        JobSchedulerInternal::Launch(ThisJob);
                }
                void Append(Job* job)
                {
                        job->Reset(rootJob);
                        JobSchedulerInternal::Launch(job);
                }
                void Wait()
                {
                        JobSchedulerInternal::Wait(rootJob);
                }
                void operator()()
                {
                        JobSchedulerInternal::Launch(rootJob);
                }
                Job* GetRootJob()
                {
                        return rootJob;
                }
        };
} // namespace JobScheduler