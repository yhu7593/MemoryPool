#pragma once

#include "./Common.h"

namespace Memory_Pool
{
    class CentralCache
    {
    public:
        static CentralCache &getInstance()
        {
            static CentralCache instance;
            return instance;
        }
        void *fetchRange(size_t index, size_t batchnum);
        void returnRange(void *ptr, size_t index, size_t batchnum);

    private:
        CentralCache()
        {   
            for(auto &ptr:central_free_list)
            {
                ptr.store(nullptr, std::memory_order_relaxed);
            }
            for(auto &lock:locks){
                lock.clear();
            }
        }

        // 从页缓存获取内存
        void *fetchFromPageCache(size_t size);

    private:
        // 中心缓存的自由链表
        std::array<std::atomic<void *>, FREE_LIST_SIZE> central_free_list;
        // 用于同步的自旋锁
        std::array<std::atomic_flag, FREE_LIST_SIZE> locks;
    };
}