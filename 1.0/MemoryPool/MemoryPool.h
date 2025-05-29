#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <cassert>
namespace MemoryPool
{

#define MEMORY_POOL_NUM 64
#define SLOT_SIZE 8
#define MAX_SOLT_SIZE 512

    struct Slot
    {
        std::atomic<Slot *> next;
    };

    class MemoryPool
    {
    public:
        MemoryPool(size_t Blocksize = 4096);
        ~MemoryPool();
        void init(size_t);
        void *allocate();
        void deallocate(void *);

    private:
        void allocate_New_Block();
        size_t padPointer(char *ptr, size_t size);

        bool pushFreelist(Slot *slot);
        Slot *popFreelist();

    private:
        size_t Block_Size;             // 内存块大小
        size_t Slot_Size;              // 槽位大小
        Slot *first_Block;             // 第一个内存块
        Slot *cur_Slot;                // 当前未被使用的槽位
        std::atomic<Slot *> free_Slot; // 空闲槽位
        Slot *last_Slot;               // 最后一个槽位
        std::mutex mutex_for_Block;    // 内存块锁,防止多线程情况下重复开辟内存块
    };
    class HashBucket
    {
    public:
        static void initMemoryPool();
        static MemoryPool &getMemoryPool(int index);
        static void *useMemory(size_t size)
        {
            if (size <= 0)
            {
                return nullptr;
            }
            if (size > MAX_SOLT_SIZE)
            {
                return operator new(size);
            }
            return getMemoryPool(((size + 7) / SLOT_SIZE) - 1).allocate();
        }

        static void freeMemory(void *ptr, size_t size)
        {
            if (ptr == nullptr)
            {
                return;
            }
            if (size > MAX_SOLT_SIZE)
            {
                operator delete(ptr);
                return;
            }
            getMemoryPool(((size + 7) / SLOT_SIZE) - 1).deallocate(ptr);
        }

        template <typename T, typename... Args>
        friend T *newElement(Args &&...args);
        template <typename T>
        friend void deleteElement(T *ptr);
    };

    template <typename T, typename... Args>
    T *newElement(Args &&...args)
    {
        T *p = nullptr;
        if (p = reinterpret_cast<T *>(HashBucket::useMemory(sizeof(T))))
        {
            new (p) T(std::forward<Args>(args)...);
        }
        return p;
    }

    template <typename T>
    void deleteElement(T *ptr)
    {
        if (ptr)
        {
            ptr->~T();
            HashBucket::freeMemory(reinterpret_cast<void *>(ptr), sizeof(T));
        }
    }
}
