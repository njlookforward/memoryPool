#ifndef V_1_0_MEMORYPOOL_H
#define V_1_0_MEMORYPOOL_H

#define HASH_BACKET_SIZE 64
#define SLOT_BASE_SIZE 8
#define SLOT_MAX_SIZE 512

// 大胆尝试，大胆实践，不要怂，错了改过去不就好了，我就是一名工程师，我要一小块一小块的敲掉他

// #include <vector>
#include <mutex>
#include <new>  // use operator new and operator delete
// #include <cstddef>
#include <cstdint>

// 在大型项目中，必须使用namespace进行命名空间的分割
namespace memoryPool {

/// @param Slot代表每一个内存块，同时充分地利用内存块，记录信息，目前只包含下一个空闲的内存块地址
struct Slot {
    Slot *_next;    // 记录下一个空闲的狭槽在哪里
};

class MemoryPool {
public:
    MemoryPool() = default;
    ~MemoryPool();
    MemoryPool(const MemoryPool&) = delete;
    MemoryPool(MemoryPool &&) = delete;
    MemoryPool &operator=(const MemoryPool &) = delete;
    MemoryPool &operator=(MemoryPool &&) = delete;

/// @remarks 哪些是临界区，需要使用互斥量进行同步，这需要我自己慢慢体会

    void init(int Slotsize);
    void *allocate();
    void deallocate(Slot *);

private:
    void newBlock();
    int paddingBlock(Slot *, int); // 进行内存对齐
    void *popFreeList();
    void pushFreeList(Slot *);

private:
    int _blockSize = 4096;            // 每个内存池真正的整块内存是固定的4KB
    int _slotSize = 0;                // 待分配的每一个小内存块的大小
    Slot *_firstBlock = nullptr;      // 可用的内存块地址
    Slot *_freeList = nullptr;        // 指向空闲链表的表头
    Slot *_curSlot = nullptr;         // 指向目前的第一个unused Slot
    Slot *_lastSlot = nullptr;        // 代表该memoryPool的最后一个内存块的位置标识
    std::mutex _mutexForFreeList;     // 该互斥量用于操作_freeList
    std::mutex _mutexForBlock;        // 该互斥量用于操作block
};

}
#endif