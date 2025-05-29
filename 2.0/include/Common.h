#pragma once

#include <cstddef>
#include <cstdlib>
#include <atomic>
#include <array>

namespace Memory_Pool
{
    // 对齐数和大小定义
    constexpr size_t ALIGNMENT = 8;                         // void*指针大小
    constexpr size_t MAX_SIZE = 256 * 1024;                 // 256KB
    constexpr size_t FREE_LIST_SIZE = MAX_SIZE / ALIGNMENT; // 最大槽位数

    // 内存块头部信息
    struct BlockHeader
    {
        size_t size;       // 内存块大小
        bool isUse;        // 使用标志
        BlockHeader *next; // 指向下一个内存块
    };

    class SizeClass
    {
    public:
        static size_t roundup(size_t bytes)
        {
            // bytes加上ALIGNMENT-1确保向上取整
            //~(ALIGNMENT - 1)创建了一个高位掩码
            // 通过与操作，清除多余的低位
            return (bytes + ALIGNMENT - 1) & ~(ALIGNMENT - 1);
        }
        static size_t getIndex(size_t bytes)
        {
            // 确保bytes至少为ALIGNMENT
            bytes = std::max(bytes, ALIGNMENT);
            // 向上取整后-1
            return (bytes + ALIGNMENT - 1) / ALIGNMENT - 1;
        }
    };

}