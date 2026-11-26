/**
 * @file FiberMutex.hpp
 * @brief 协程感知的互斥锁
 * @author Christins
 * @date 2026-11-27
 * @copyright Apache 2.0
 */
#pragma once

#include <Chen/Fiber/Fiber.hpp>
#include <Chen/Util/Mutex.hpp>

#include <condition_variable>
#include <cstddef>
#include <mutex>

/**
 * @brief 协程感知互斥锁
 * @details 临界区内只要出现被 hook 的 IO（MySQL / Redis / connect / send 等），当前协程就会让出线程。
 *          这时如果用 std::mutex，等待方阻塞的是整个调度线程：线程被占住后，持锁协程等待的
 *          IO 事件再也没有线程去 epoll 里取，双方互等形成死锁（io.thread_num=1 时表现为全站超时）。
 *          本锁的等待方让出协程而不是线程，解锁方把所有权直接转交给一个等待者。
 * @note 协程与普通线程可以混用同一把锁：非协程上下文走条件变量等待，不会挂起调度线程以外的协程。
 */
class FiberMutex {
public:
    /**
     * @brief 作用域锁
     */
    class Lock {
    public:
        explicit Lock(FiberMutex& mutex)
            : mutex_(mutex)
        {
            mutex_.lock();
        }

        ~Lock() { mutex_.unlock(); }

        Lock(const Lock&) = delete;
        Lock& operator=(const Lock&) = delete;

    private:
        FiberMutex& mutex_;
    };

    FiberMutex() = default;

    void lock()
    {
        std::unique_lock<std::mutex> lock(state_mutex_);
        if (!locked_) {
            locked_ = true;
            return;
        }

        if (chen::Fiber::GetThis()) {
            // 协程：登记后让出，unlock 时把所有权直接转交给等待者
            ++fiber_waiters_;
            lock.unlock();
            fiber_semaphore_.wait();
            return;
        }

        // 普通线程：条件变量等待，unlock 会把 locked_ 置回 false 并唤醒
        ++thread_waiters_;
        thread_cv_.wait(lock, [this] {
            return !locked_;
        });
        locked_ = true;
        --thread_waiters_;
    }

    void unlock()
    {
        std::unique_lock<std::mutex> lock(state_mutex_);
        if (fiber_waiters_ > 0) {
            // 所有权转交，locked_ 保持 true，直到被唤醒的协程自己 unlock
            --fiber_waiters_;
            fiber_semaphore_.notify();
            return;
        }

        locked_ = false;
        if (thread_waiters_ > 0) {
            thread_cv_.notify_one();
        }
    }

private:
    /// 只在簿记时短暂持有，绝不跨协程让出
    std::mutex state_mutex_;
    /// 普通线程等待队列
    std::condition_variable thread_cv_;
    /// 协程等待队列（计数信号量，notify 对应一次唤醒）
    chen::FiberSemaphore fiber_semaphore_{0};
    bool locked_ = false;
    size_t fiber_waiters_ = 0;
    size_t thread_waiters_ = 0;
};
