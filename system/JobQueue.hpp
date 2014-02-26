/*
 
 Video Core
 Copyright (C) 2014 James G. Hurley
 
 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.
 
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.
 
 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 USA
 
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
        Job(std::function<void()> job) : m_job(job), m_dispatchDate(std::chrono::steady_clock::now()), m_isSynchronous(false) {} ;
        
        Job(std::function<void()> job, std::chrono::steady_clock::time_point dispatch_date) : m_job(job), m_dispatchDate(dispatch_date), m_isSynchronous(false) {};
    
        void operator ()() const { m_job(); };
        
        const std::chrono::steady_clock::time_point dispatchDate() const { return m_dispatchDate; };
        
        bool m_isSynchronous;
        
    private:
        std::chrono::steady_clock::time_point m_dispatchDate;
        
        std::function<void()> m_job;
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
