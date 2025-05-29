#include "../include/CentralCache.h"
#include "../include/PageCache.h"
#include <thread>

namespace Memory_Pool
{
    // 每次从PageCache获取span大小（以页为单位）
    static const size_t SPAN_PAGES = 8;

    void *CentralCache::fetchRange(size_t index, size_t batchNum)
    {
        if (index >= FREE_LIST_SIZE || batchNum == 0)
        {
            return nullptr; // 索引越界或批量数为0
        }
        // 自旋锁保护
        while (locks[index].test_and_set(std::memory_order_acquire))
        {
            std::this_thread::yield(); // 添加线程让步，避免忙等待，避免过度消耗CPU
        }

        void *result = nullptr;
        try
        {
            result = central_free_list[index].load(std::memory_order_relaxed);
            if (result == nullptr)
            {
                // 如果中心缓存为空，从页缓存获取新的内存块
                size_t size = (index + 1) * ALIGNMENT;
                result = fetchFromPageCache(size);
                if (result == nullptr)
                {
                    locks[index].clear(std::memory_order_release);
                    return nullptr;
                }
                // 将从PageCache获取的内存块切分成小块
                char *start = static_cast<char *>(result);
                size_t totalBlocks = (SPAN_PAGES * PageCache::PAGE_SIZE) / size;
                size_t allocBlocks = std::min(batchNum, totalBlocks);
                // 构建返回给ThreadCache的内存块链表
                if (allocBlocks > 1)
                {
                    // 确保至少有两个块才构建链表
                    // 构建链表
                    for (size_t i = 1; i < allocBlocks; i++)
                    {
                        void *current = start + (i - 1) * size;
                        void *next = start + i * size;
                        *reinterpret_cast<void **>(current) = next;
                    }
                    *reinterpret_cast<void **>(start + (allocBlocks - 1) * size) = nullptr; // 最后一个块指向nullptr
                }
                if (totalBlocks > allocBlocks)
                {
                    void *remainStart = start + allocBlocks * size;
                    for (size_t i = allocBlocks + 1; i < totalBlocks; i++)
                    {
                        void *current =start + (i - 1) * size;
                        void *next = start + i * size;
                        *reinterpret_cast<void **>(current) = next;
                    }
                    *reinterpret_cast<void **>(start + (totalBlocks - 1) * size) = nullptr; // 最后一个块指向nullptr
                    // 将剩余的内存块返回到中心缓存
                    central_free_list[index].store(remainStart, std::memory_order_release);
                }
            }
            else // 如果中心缓存有index对应大小的内存块
            {
                // 从现有链表中获取指定数量的块
                void *current = result;
                void *prev = nullptr;
                size_t count = 0;
                while (current != nullptr && count < batchNum)
                {
                    prev = current;
                    current = *reinterpret_cast<void **>(current);
                    count++;
                }
                if (prev)
                {
                    *reinterpret_cast<void **>(prev) = nullptr; // 最后一个块指向nullptr
                }
                central_free_list[index].store(current, std::memory_order_release); // 更新中心缓存
            }
        }
        catch (...)
        {
            locks[index].clear(std::memory_order_release);
            throw; // 重新抛出异常
        }
        locks[index].clear(std::memory_order_release); // 释放锁
        return result;                                 // 返回获取的内存块
    }

    void CentralCache::returnRange(void *ptr, size_t index, size_t batchNum)
    {
        if (ptr == nullptr || index >= FREE_LIST_SIZE)
        {
            return;
        }
        while (locks[index].test_and_set(std::memory_order_acquire))
        {
            std::this_thread::yield(); // 添加线程让步，避免忙等待，避免过度消耗CPU
        }
        try
        {
            // 找到要归还的链表的最后一个节点
            void *end = ptr;
            size_t count = 1;
            while (*reinterpret_cast<void **>(end) != nullptr && count < batchNum)
            {
                end = *reinterpret_cast<void **>(end);
                count++;
            }
            // 将归还的链表连接到中心缓存的链表头部
            void *current = central_free_list[index].load(std::memory_order_relaxed);
            *reinterpret_cast<void **>(end) = current;                      // 将归还的链表的最后一个节点指向中心缓存的链表头部
            central_free_list[index].store(ptr, std::memory_order_release); // 更新中心缓存
        }
        catch (...)
        {
            locks[index].clear(std::memory_order_release);
            throw; // 重新抛出异常
        }
        locks[index].clear(std::memory_order_release);
    }

    void *CentralCache::fetchFromPageCache(size_t size)
    {
        // 1. 计算实际需要的页数
        size_t numPages = (size + PageCache::PAGE_SIZE - 1) / PageCache::PAGE_SIZE;
        // 2. 根据大小决定分配策略
        if (size <= SPAN_PAGES * PageCache::PAGE_SIZE)
        {
            // 小于等于32KB的请求，使用固定8页
            return PageCache::getInstance().allocatePage(SPAN_PAGES);
        }
        else
        {
            // 大于32KB的请求，按实际需求分配
            return PageCache::getInstance().allocatePage(numPages);
        }
    }
}
