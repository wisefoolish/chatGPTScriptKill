#ifndef THREAD_POOL_H
#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS
#define THREAD_POOL_H
#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>

class ThreadPool {
public:
    ThreadPool(size_t);
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args)
        ->std::future<typename std::invoke_result_t<F,Args...>>;
    ~ThreadPool();
private:
    // need to keep track of threads so we can join them
    std::vector< std::thread > workers;
    // the task queue
    std::queue< std::function<void()> > tasks;

    // synchronization
    std::mutex queue_mutex;
    std::condition_variable condition;
    bool stop;
};

// the constructor just launches some amount of workers
inline ThreadPool::ThreadPool(size_t threads)
    : stop(false)
{
    for (size_t i = 0; i < threads; ++i)
        workers.emplace_back(
            [this]          // 开辟线程
            {
                for (;;)            // 每个线程都用一个死循环
                {
                    std::function<void()> task;     // 每个死循环都有一个函数指针

                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);       // 得到队列锁
                        this->condition.wait(lock,
                            [this] { return this->stop || !this->tasks.empty(); }); // 等待队列锁，任务为空或者停止了都退出
                        if (this->stop && this->tasks.empty())
                            return;
                        task = std::move(this->tasks.front());      // 给task赋值
                        this->tasks.pop();          // 出队一个任务
                    }

                    task();     // 执行任务
                }
            }
            );
}

// add new work item to the pool
template<class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args)
    ->std::future<typename std::invoke_result_t<F,Args...>>
{
    using return_type = typename std::invoke_result_t<F, Args...>;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );      // 智能指针创建一个函数指针

    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(queue_mutex);

        // don't allow enqueueing after stopping the pool
        if (stop)
            throw std::runtime_error("enqueue on stopped ThreadPool");

        tasks.emplace([task]() { (*task)(); });
    }
    condition.notify_one();         // 执行完释放队列锁
    return res;
}

// the destructor joins all threads
inline ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }
    condition.notify_all();
    for (std::thread& worker : workers)
        worker.join();
}

#endif
