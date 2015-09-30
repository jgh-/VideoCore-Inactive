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

#include <videocore/system/JobQueue.hpp>

namespace videocore {
    JobQueue::JobQueue(const char *name, JobQueuePriority priority) : m_exiting(false)
    {
#if !_USE_GCD
        m_thread = std::thread([&]() { thread(); });
        if(name) {
            enqueue([=]() { pthread_setname_np(name); });
        }
#else
        m_queue = dispatch_queue_create(name ? name : "", 0);
        IsOnSameJobQueueKey = &IsOnSameJobQueueKey;
        dispatch_queue_set_specific(m_queue, IsOnSameJobQueueKey, this, NULL);
        int p = 0;
        switch (priority) {
            case kJobQueuePriorityDefault:
                p = DISPATCH_QUEUE_PRIORITY_DEFAULT;
                break;
            case kJobQueuePriorityHigh:
                p = DISPATCH_QUEUE_PRIORITY_HIGH;
                break;
            case kJobQueuePriorityLow:
                p = DISPATCH_QUEUE_PRIORITY_LOW;
                break;
        }
        // 这里set target queue有神马用？
        dispatch_set_target_queue(m_queue, dispatch_get_global_queue(p, 0 ));
#endif
    }
    JobQueue::~JobQueue()
    {
        m_exiting = true;
#if !_USE_GCD
        m_cond.notify_all();
        m_thread.join();
#else
        dispatch_sync(m_queue, ^{});
        
        dispatch_release(m_queue);
#endif
    }
    void JobQueue::mark_exiting() {
        m_exiting = true;
    }
    void JobQueue::enqueue(std::function<void()> job) {
        enqueue(std::make_shared<Job>(job));
    }
    void JobQueue::enqueue(std::shared_ptr<Job> job) {
#if !_USE_GCD
        m_jobMutex.lock();
        m_jobs.push(job);
        m_jobMutex.unlock();
        m_cond.notify_all();
#else
        dispatch_async(m_queue, ^{
            if(!this->m_exiting.load()) {
                (*job)();
            }
        });
#endif
    }
    void JobQueue::enqueue_sync(std::function<void()> job) {
        enqueue_sync(std::make_shared<Job>(job));
    }
    void JobQueue::enqueue_sync(std::shared_ptr<Job> job) {
#if !_USE_GCD
        job->m_isSynchronous = true;
        enqueue(job);
        std::unique_lock<std::mutex> lk(m_doneMutex);
        if(!job->done()) {
            m_jobDoneCond.wait(lk);
        }
#else
        dispatch_sync(m_queue, ^{
            (*job)();
        });
#endif
    }
    void JobQueue::set_name(std::string name) {
#if !_USE_GCD
        enqueue([=]() { pthread_setname_np(name.c_str()); });
#endif
    }
    
    bool JobQueue::thisThreadInQueue() {
#if !_USE_GCD
        return std::this_thread::get_id() == m_thread.get_id();
#else
        return dispatch_get_specific(IsOnSameJobQueueKey) != nullptr;
#endif
    }
#if !_USE_GCD
    void JobQueue::thread() {
        do {
            std::unique_lock<std::mutex> l(m_mutex);
            
            m_jobMutex.lock();
            if(m_jobs.size() > 0) {
                auto j = m_jobs.front();
                m_jobs.pop();
                m_jobMutex.unlock();
                (*j)();
                if(j->m_isSynchronous) {
                    m_jobDoneCond.notify_one();
                }
            } else {
                m_jobMutex.unlock();
                m_cond.wait(l);
            }
        } while(!(m_exiting.load()));
    }
#endif
}
