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

#include <atomic>
#include <mutex>
#include <thread>
#include <queue>
#include <functional>
#include <chrono>
#include <iostream>
#include <pthread.h>

namespace videocore {
    
    struct Job {
        Job(std::function<void()> job) : m_isSynchronous(false) , m_job(job), m_dispatchDate(std::chrono::steady_clock::now()) {} ;
        
        Job(std::function<void()> job, std::chrono::steady_clock::time_point dispatch_date) : m_isSynchronous(false), m_job(job), m_dispatchDate(dispatch_date)  {};
    
        void operator ()() const { m_job(); };
        
        const std::chrono::steady_clock::time_point dispatchDate() const { return m_dispatchDate; };
        
        bool m_isSynchronous;
        
    private:
        std::function<void()> m_job;
        std::chrono::steady_clock::time_point m_dispatchDate;
        

    };
    class JobQueue
    {
    public:
        JobQueue() : m_exiting(false)
        {
            m_thread = std::thread([&]() { thread(); });
        }
        ~JobQueue()
        {
            m_exiting = true;
            m_cond.notify_all();
            m_thread.join();
        }
        void enqueue(std::function<void()> job) {
            enqueue(Job(job));
        }
        void enqueue(Job job) {
            
            m_jobMutex.lock();
            m_jobs.push(job);
            m_jobMutex.unlock();
            m_cond.notify_all();
        }
        void enqueue_sync(std::function<void()> job) {
            enqueue_sync(Job(job));
        }
        void enqueue_sync(Job job) {
            job.m_isSynchronous = true;
            enqueue(job);
            std::unique_lock<std::mutex> lk(m_doneMutex);
            m_jobDoneCond.wait(lk);
        }
        void set_name(std::string name) {
            enqueue([=]() { pthread_setname_np(name.c_str()); });
        }
    private:
        
        void thread() {
            do {
                std::unique_lock<std::mutex> l(m_mutex);
                
                m_jobMutex.lock();
                if(m_jobs.size() > 0) {
                    Job j = m_jobs.front();
                    m_jobs.pop();
                    m_jobMutex.unlock();
                    j();
                    if(j.m_isSynchronous) {
                        m_jobDoneCond.notify_one();
                    }
                } else {
                    m_jobMutex.unlock();
                    m_cond.wait(l);
                }
            } while(!(m_exiting.load()));
        }
    private:
        std::thread                 m_thread;
        std::mutex                  m_mutex, m_jobMutex, m_doneMutex;
        std::condition_variable     m_cond, m_jobDoneCond;
        std::atomic<bool>           m_exiting;
        std::queue<Job>             m_jobs;
    };
}

#endif
