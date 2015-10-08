/*
 
 Video Core
 Copyright (c) 2014 James G. Hurley
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 
 */

#ifndef videocore_JobQueue_hpp
#define videocore_JobQueue_hpp

#ifdef __APPLE__
#include <dispatch/dispatch.h>
#define _USE_GCD 0
#else
#define _USE_GCD 0
#include <sys/prctl.h>
#define pthread_setname_np(thread_name) prctl(PR_SET_NAME, thread_name)
#endif
#include <condition_variable>
#include <atomic>
#include <mutex>
#include <thread>
#include <queue>
#include <functional>
#include <chrono>
#include <iostream>
#include <pthread.h>

// 为了避免非ARC使用的问题，把实现代码移动到cpp文件。
namespace videocore {
    
    typedef enum {
        kJobQueuePriorityDefault,
        kJobQueuePriorityHigh,
        kJobQueuePriorityLow
    } JobQueuePriority;
    
    struct Job {
        Job(std::function<void()> job) : m_isSynchronous(false) , m_job(job), m_dispatchDate(std::chrono::steady_clock::now()), m_done(false) {} ;
        
        Job(std::function<void()> job, std::chrono::steady_clock::time_point dispatch_date) : m_isSynchronous(false), m_job(job), m_dispatchDate(dispatch_date), m_done(false)  {};
        
        void operator ()() { m_job(); setDone(); };
        
        const std::chrono::steady_clock::time_point dispatchDate() const { return m_dispatchDate; };
        bool done() const { return m_done; }
        void setDone() { m_done = true; }
        
        bool m_isSynchronous;
        
    private:
        std::function<void()> m_job;
        std::chrono::steady_clock::time_point m_dispatchDate;
        bool m_done;
        
    };
    class JobQueue
    {
    public:
        JobQueue(const char *name = "", JobQueuePriority priority = kJobQueuePriorityDefault);
        ~JobQueue();
        void mark_exiting();
        void enqueue(std::function<void()> job);
        void enqueue(std::shared_ptr<Job> job);
        void enqueue_sync(std::function<void()> job);
        void enqueue_sync(std::shared_ptr<Job> job);
        void set_name(std::string name);
        bool thisThreadInQueue();
    private:
#if !_USE_GCD
        void thread();
#endif
    private:
#if !_USE_GCD
        std::thread                 m_thread;
        std::mutex                  m_mutex, m_jobMutex, m_doneMutex;
        std::condition_variable     m_cond, m_jobDoneCond;
        std::queue<std::shared_ptr<Job>>             m_jobs;
#else
        dispatch_queue_t            m_queue;
        void *IsOnSameJobQueueKey;
#endif
        std::atomic<bool>           m_exiting;
    };
}

#endif
