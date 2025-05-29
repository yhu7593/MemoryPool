#include "./MemoryPool.h"

namespace MemoryPool
{
    MemoryPool::MemoryPool(size_t Blocksize)
        : Block_Size(Blocksize),
          Slot_Size(0),
          first_Block(nullptr),
          cur_Slot(nullptr),
          free_Slot(nullptr),
          last_Slot(nullptr)
    {
    }

    MemoryPool::~MemoryPool()
    {
        Slot *cur = first_Block;
        while (cur)
        {
            Slot *next = cur->next;
            operator delete(reinterpret_cast<void *>(cur));
            cur = next;
        }
    }

    void MemoryPool::init(size_t size)
    {
        assert(size > 0);
        Slot_Size = size;
        first_Block = nullptr;
        cur_Slot = nullptr;
        free_Slot = nullptr;
        last_Slot = nullptr;
    }

    void *MemoryPool::allocate()
    {
        Slot *slot = popFreelist();
        if (slot)
        {
            return reinterpret_cast<void *>(slot);
        }
        Slot *temp;
        {
            std::lock_guard<std::mutex> lock(mutex_for_Block);
            if (cur_Slot >= last_Slot)
            {
                // 当前内存块已无内存槽可用，开辟一块新的内存
                allocate_New_Block();
            }
            temp = cur_Slot;
            cur_Slot += Slot_Size / sizeof(Slot);
        }
        return reinterpret_cast<void *>(temp);
    }
    void MemoryPool::deallocate(void *ptr)
    {
        if (ptr == nullptr)
        {
            return;
        }
        Slot *slot = reinterpret_cast<Slot *>(ptr);
        pushFreelist(slot);
    }

    void MemoryPool::allocate_New_Block()
    {
        // 头插法
        void *newBlock = operator new(Block_Size);
        reinterpret_cast<Slot *>(newBlock)->next = first_Block;
        first_Block = reinterpret_cast<Slot *>(newBlock);

        char *body = reinterpret_cast<char *>(newBlock) + sizeof(Slot); // 获取内存块的槽位部分
        size_t paddingSize = padPointer(body, Slot_Size);               // 计算块头部Slot*指针需要的填充字节数
        cur_Slot = reinterpret_cast<Slot *>(body + paddingSize);        // 更新当前空闲槽位指针

        last_Slot = reinterpret_cast<Slot *>(reinterpret_cast<size_t>(newBlock) + Block_Size - Slot_Size + 1); // 更新当前块的最后一个槽位指针
        free_Slot = nullptr;                                                 // 重置空闲槽位指针,调用此函数时，free_Slot指向的槽位已经被分配
    }

    // 让指针对齐到槽大小的倍数位置
    size_t MemoryPool::padPointer(char *ptr, size_t size)
    {
        return (size - reinterpret_cast<size_t>(ptr)) % size;
    }

    // 实现无锁入队操作
    bool MemoryPool::pushFreelist(Slot *slot)
    {
        while (true)
        {
            Slot *oldHead = free_Slot.load(std::memory_order_relaxed);
            slot->next.store(oldHead, std::memory_order_relaxed);
            // atomic.compare_exchange_weak(expected, desired, success_order, failure_order);
            // 如果 atomic == expected，就把 desired 写入 atomic 并返回 true；
            // 否则，把 atomic 当前值写回 expected，返回 false。
            // weak可能因伪失败而返回 false,strong不会伪失败，weak更快
            if (free_Slot.compare_exchange_weak(oldHead, slot, std::memory_order_release, std::memory_order_relaxed))
            {
                return true;
            }
            // 写在循环中，失败说明free_Slot已经被其他线程修改了，重试
        }
    }

    // 实现无锁出队操作
    Slot *MemoryPool::popFreelist()
    {
        while (true)
        {
            Slot *oldHead = free_Slot.load(std::memory_order_relaxed);
            if (oldHead == nullptr)
            {
                return nullptr;
            }
            Slot *newHead = nullptr;
            try
            {
                newHead = oldHead->next.load(std::memory_order_relaxed);
            }
            catch (...)
            {
                // 如果返回失败，则continue重新尝试申请内存
                continue;
            }
            if (free_Slot.compare_exchange_weak(oldHead, newHead, std::memory_order_release, std::memory_order_relaxed))
            {
                return oldHead;
            }
            // 失败：说明另一个线程可能已经修改了 freeList_
            // CAS 失败则重试
        }
    }

    void HashBucket::initMemoryPool()
    {
        for (int i = 0; i < MEMORY_POOL_NUM; i++)
        {
            getMemoryPool(i).init(SLOT_SIZE * (i + 1));
        }
    }

    MemoryPool &HashBucket::getMemoryPool(int index)
    {
        static MemoryPool memoryPool[MEMORY_POOL_NUM];
        return memoryPool[index];
    }
} // namespace MemoryPool
