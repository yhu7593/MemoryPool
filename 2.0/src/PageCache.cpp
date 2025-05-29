#include "../include/PageCache.h"
#include <sys/mman.h>
#include <cstring>
namespace Memory_Pool
{

    void *PageCache::allocatePage(size_t numPages)
    {
        std::lock_guard<std::mutex> lock(mtx);
        // 查找合适的空闲span
        // lower_bound函数返回第一个大于等于numPages的元素的迭代器
        auto it = freeSpans.lower_bound(numPages);
        if (it != freeSpans.end())
        {
            Span *span = it->second;
            // 将取出的span从原有的空闲链表freeSpans_[it->first]中移除
            if (span->next)
            {
                freeSpans[it->first] = span->next;
            }
            else
            {
                freeSpans.erase(it);
            }
            // 如果span大于需要的numPages则进行分割
            if (span->numPages > numPages)
            {
                Span *newSpan = new Span;
                newSpan->pageAddr = static_cast<char *>(span->pageAddr) +
                                     numPages * PAGE_SIZE;
                newSpan->numPages = span->numPages - numPages;
                newSpan->next = nullptr;

                // 将超出部分放回空闲Span*列表头部
                auto &list = freeSpans[newSpan->numPages];
                newSpan->next = list;
                list = newSpan;

                span->numPages = numPages;
                // 记录新分割出的span
                spanMap[newSpan->pageAddr] = newSpan;
            }

            // 记录span信息用于回收
            spanMap[span->pageAddr] = span;
            return span->pageAddr;
        }
        // 没有合适的span，向系统申请
        void *memory = systemAlloc(numPages);
        if (memory == nullptr)
        {
            return nullptr;
        }
        Span *span = new Span;
        span->pageAddr = memory;
        span->numPages = numPages;
        span->next = nullptr;

        // 记录span信息用于回收
        spanMap[memory] = span;
        return memory;
    }

    void PageCache::deallocatePage(void *ptr, size_t numPages)
    {
        std::lock_guard<std::mutex> lock(mtx);
        // 查找对应的span，没找到代表不是PageCache分配的内存，直接返回
        auto it = spanMap.find(ptr);
        if (it == spanMap.end())
        {
            return;
        }

        Span *span = it->second;
        // 尝试合并相邻的span
        void *nextAddr = static_cast<char *>(ptr) + numPages * PAGE_SIZE;
        auto nextIt = spanMap.find(nextAddr);

        if (nextIt != spanMap.end())
        {
            Span *nextSpan = nextIt->second;
            // 1. 首先检查nextSpan是否在空闲链表中
            bool found = false;
            auto &nextList = freeSpans[nextSpan->numPages];
            // 检查是否是头节点
            if (nextList == nextSpan)
            {
                nextList = nextList->next;
                found = true;
            }
            else if (nextList)
            { // 只有在链表非空时才遍历
                Span *prev = nextList;
                while (prev->next)
                {
                    if (prev->next == nextSpan)
                    {
                        // 将nextSpan从空闲链表中移除
                        prev->next = nextSpan->next;
                        found = true;
                        break;
                    }
                    prev = prev->next;
                }
            }
            // 2. 只有在找到nextSpan的情况下才进行合并
            if (found)
            {
                // 合并span
                span->numPages += nextSpan->numPages;
                spanMap.erase(nextAddr);
                delete nextSpan;
            }
        }

        auto &list = freeSpans[span->numPages];
        span->next = list;
        list = span;
    }

    void *PageCache::systemAlloc(size_t numPages)
    {
        size_t size = numPages * PAGE_SIZE;

        // 使用mmap分配内存
        void *ptr = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (ptr == MAP_FAILED)
        {
            return nullptr;
        }
        memset(ptr, 0, size);
        return ptr;
    }
}
