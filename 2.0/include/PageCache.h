#pragma once
#include "./Common.h"
#include <mutex>
#include <map>
namespace Memory_Pool
{
    // 页缓存类，负责管理内存页的分配和回收
    class PageCache
    {
    public:
        static const size_t PAGE_SIZE = 4096; // 假设每页大小为4KB

        static PageCache &getInstance()
        {
            static PageCache instance;
            return instance;
        }

        // 分配一页内存
        void *allocatePage(size_t numPages);

        // 释放一页内存
        void deallocatePage(void *ptr, size_t numPages);

    private:
        PageCache() = default;
        //向系统申请内存
        void *systemAlloc(size_t size);

    private:
        struct Span
        {
            void *pageAddr;  // 页起始地址
            size_t numPages; // 页数
            Span *next;      // 链表指针
        };
        // 按页数管理空闲span，不同页数对应不同Span链表
        std::map<size_t, Span *> freeSpans;
        // 页号到span的映射，用于回收
        std::map<void *, Span *> spanMap;
        std::mutex mtx; // 互斥锁，保护多线程访问
    };
}
