#pragma once
#include "./Common.h"
namespace Memory_Pool
{
    class ThreadCache
    {
    public:
        static ThreadCache *getInstance()
        {
            static thread_local ThreadCache instance;
            return &instance;
        }
        void *allocate(size_t size);
        void deallocate(void *ptr, size_t size);

    private:
        ThreadCache() = default;
        // 从中心缓存获取内存
        void *fetchFromCentalCache(size_t index);
        // 将内存返回到中心缓存
        void returnToCentralCache(void *ptr, size_t size);
        // 计算批量获取内存快的数量
        size_t getBatchNum(size_t size) const;
        // 判断是否需要归还内存给中心缓存
        bool shouldReturnToCentralCache(size_t index);

    private:
        std::array<void *, FREE_LIST_SIZE> free_list;
        std::array<size_t, FREE_LIST_SIZE> free_list_size; // 自由链表大小统计
    };
}
