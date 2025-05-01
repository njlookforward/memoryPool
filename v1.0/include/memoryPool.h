#ifndef V_1_0_MEMORYPOOL_H
#define V_1_0_MEMORYPOOL_H

// 大胆尝试，大胆实践，不要怂，错了改过去不就好了，我就是一名工程师，我要一小块一小块的敲掉他
// 就算做出一堆狗屎又如何，我要做出来

#include <vector>
#include <mutex>
#include <atomic>
#include <new>  // use operator new and operator delete
#include <cstddef>
#include <cstdint>
#include <utility>
#include <stdexcept>
#include <cassert>

// 在大型项目中，必须使用namespace进行命名空间的分割
namespace memoryPool {

/// @warning origin three define in the namespace memoryPool
/// @remark 这种预编译量定义在命名空间中
#define HASH_BACKET_SIZE 64
#define SLOT_BASE_SIZE 8
#define SLOT_MAX_SIZE 512

/// @param Slot代表内存池中的每一个内存块，同时充分地利用内存块，记录信息，目前只包含下一个空闲的内存块地址
struct Slot {
    std::atomic<Slot *> _next;    // 记录下一个空闲的狭槽在哪里
};

/// @brief memoryPool是每一个内存池管理员，记录这个内存池的关键信息，并真正负责内存池中每一个slot的分配与回收
class MemoryPool {
public:
/// @param MemoryPool() 构造函数的职能仅仅是确定内存池的块大小，而Slot大小是由hash bucket调用init()函数指定完成的
    MemoryPool() = default;
    ~MemoryPool();
    MemoryPool(const MemoryPool&) = delete;
    MemoryPool(MemoryPool &&) = delete;
    MemoryPool &operator=(const MemoryPool &) = delete;
    MemoryPool &operator=(MemoryPool &&) = delete;

/// @remarks 哪些是临界区，需要使用互斥量进行同步，这需要我自己慢慢体会
/// @public init() 用来确定Slot的大小
    void init(std::size_t Slotsize);
    void *allocate();
    void deallocate(void *);

private:
    void newBlock();
    std::size_t paddingBlock(Slot *, std::size_t); // 进行内存对齐
    void *popFreeList();
    void pushFreeList(void *);

private:
    std::size_t _blockSize = 4096;            // 每个内存池真正的整块内存是固定的4KB
    std::size_t _slotSize = 0;                // 待分配的每一个小内存块的大小
    Slot *_firstBlock = nullptr;      // 可用的内存块地址
    std::atomic<Slot *> _freeList;        // 指向空闲链表的表头
    Slot *_curSlot = nullptr;         // 指向目前的第一个unused Slot
    Slot *_lastSlot = nullptr;        // 代表该memoryPool的最后一个内存块的位置标识
    std::mutex _mutexForBlock;        // 该互斥量用于操作block
};

/** @param class HashBucket是一个静态类，里面的数据成员和成员函数都是static and private, HashBucket是实现类
 * 真正的对外接口是newElement and deleteElement
 */
class HashBucket {
private:
    template <typename T, typename... Args>
    friend T *newElement(Args... args);

    template <typename T>
    friend void deleteElement(T *ptr);

    static void initMemoryPoolSet();
    static MemoryPool &getMemoryPool(std::size_t);

private:
    static std::vector<MemoryPool> _memoryPoolSet;
    static bool _initFlag;  // 初始化为false
};

template <typename T, typename... Args>
inline T *newElement(Args... args) {
    if(HashBucket::_initFlag == false) {
        HashBucket::initMemoryPoolSet();
        HashBucket::_initFlag = true;
    }

    // 先分配内存，需要判断内存大小是否在SLOT_BASE_SIZE - SLOT_MAX_SIZE之间
    auto sz = sizeof(T);
    T *ptr;
    if(sz > SLOT_MAX_SIZE)
        ptr = operator new(sz);
    else if(sz <= SLOT_MAX_SIZE && sz > 0)
        ptr = reinterpret_cast<T*>(HashBucket::getMemoryPool(sizeof(T)).allocate());
    else 
        throw runtime_error("the allocated size is empty that is wrong.");
    // 再使用定位new构造元素
    new (ptr)(std::forward<Args>(args)...);
    return ptr;
}

template <typename T>
inline void deleteElement(T *ptr) {
/// @attention 对于指针，一定要有有效判断操作
    if(ptr == nullptr)  return;
    // 先析构元素
    ptr->~T();
    // 再回收内存
    if(sizeof(T) > 512)
        operator delete(ptr);
    else
        HashBucket::getMemoryPool(sizeof(T)).deallocate(reinterpret_cast<Slot *>(ptr));
}

}
#endif