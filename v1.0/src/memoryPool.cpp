#include "../include/memoryPool.h"
#include "memoryPool.h"
using std::lock_guard;
using std::mutex;
using std::memory_order_relaxed;
using std::memory_order_release;
using std::memory_order_acquire;

using std::uintptr_t;
using std::size_t;

using std::vector;
using std::runtime_error;
namespace memoryPool {

MemoryPool::~MemoryPool() {
/// @remark 我认为析构函数没有临界区，不需要进行同步操作
    if(_firstBlock) {
        Slot *curBlock = _firstBlock;
        while (curBlock)
        {
            Slot *nextBlock = _firstBlock->_next;
            // 需要使用operator delete/operator new，因为内存池只管内存的allocate and deallocate
            operator delete(reinterpret_cast<void *>(curBlock));
            curBlock = nextBlock;
        }
    }
}

/// @brief 该函数仅是指定狭槽的大小，只有需要进行allocate的时候才会真正的申请一整块大内存
void MemoryPool::init(size_t SlotSize) {
    _slotSize = SlotSize;
    _freeList.store(nullptr, memory_order_relaxed);
}

void *MemoryPool::allocate() {
/// @brief 需要在大内存块中分配出一个slot，因此_freeList and _curSlot就是临界资源，进行分配的操作就是临界区，需要进行同步操作
/// @details 首先应该在_freeList中申请空闲slot，若分配失败，再在_curSlot中分配slot，若继续失败，那么应该向操作系统申请新的
/// 一整块大内存，然后进行分配
/// 在示例文件中，尽管已经判断过_freeList是否为空，但是因为是多线程的环境，可能别的线程已经加锁导致为空
/// 因此需要再次进行判断_freeList是否为空
    {
        void *newSlot = popFreeList();
        if(newSlot != nullptr)
            return newSlot;
    }

    // 此时要竞争块的临界资源
    lock_guard<mutex> blockLock(_mutexForBlock);
    if(_curSlot >= _lastSlot) newBlock();
    Slot *nextSlot = reinterpret_cast<Slot *>(reinterpret_cast<char *>(_curSlot) + _slotSize);
    Slot *newSlot = _curSlot;
    _curSlot = nextSlot;
    return reinterpret_cast<void*>(newSlot);
}

void MemoryPool::deallocate(void *freeSlot) {
/// @brief _freeList是临界资源，需要进行同步操作
    pushFreeList(freeSlot);
}

/// @attention 是不是需要将加锁操作放在实现函数中，这样同步意图更加明显，更加具体
void MemoryPool::newBlock() {
/// @brief 因为是具体的实现函数，而加锁操作已经在接口函数完成了，所以仍然保持同步
    Slot *newblock = reinterpret_cast<Slot*>(operator new(_blockSize));
    // 接下来要充分的利用这一大块内存
    newblock->_next.store(_firstBlock, memory_order_relaxed);
    _firstBlock = newblock;
    // 接下来需要重设_curSlot and _lastSlot
    // 但在此之前，需要进行对齐操作
    _curSlot = _firstBlock + paddingBlock(_firstBlock, _slotSize);
    // 因为已经进行内存对齐操作了，所以内存池中能够存放的最后一个内存块的实际位置是不确定的，唯一能够确定的就是
    // 不足够存放一个内存块的起始位置，作为需要进行申请新的内存池块的位置标识
    _lastSlot = reinterpret_cast<Slot *>(reinterpret_cast<char *>(_firstBlock) + _blockSize - _slotSize + 1);
}

size_t MemoryPool::paddingBlock(Slot *slot, size_t align) {
/// @remark 有优化的空间，可以使用位运算比模运算快10倍
    return (align - (reinterpret_cast<uintptr_t>(slot) % align)) % align;
}

/**
 * @brief 对于临界资源_freeList的读写操作仍然是采用无锁队列，被atomic所包装的指针操作，都是使用成员函数，而不是其他的
*/
void MemoryPool::pushFreeList(void *ptr) {
    Slot *slot = reinterpret_cast<Slot *>(ptr);
    while (true)
    {
        Slot *oldHead = _freeList.load(memory_order_relaxed);
        slot->_next.store(oldHead, memory_order_relaxed);
        if(_freeList.compare_exchange_weak(oldHead, slot, memory_order_release, memory_order_relaxed))
            return;    
    }
}

/// @brief 先从空闲链表中分配slot，然后更新_freeList，同样属于实现函数，加锁操作已经在接口函数中完成了
/// @details 因为对于小内存对象的分配和回收是非常频繁的操作，因此优化方法是使用无锁队列取代互斥锁来进行加速
/// @attention 具体的并发操作我并没有理解透彻，先接受这样操作，为什么在popFreeList中使用memory_order_acquire，而不是memory_order_acq_rel??
void *MemoryPool::popFreeList() {
    while (true)
    {
        Slot *oldHead = _freeList.load(memory_order_relaxed);
        if(oldHead == nullptr)
            return nullptr;
        
        Slot *newHead = oldHead->_next.load(memory_order_relaxed);
        if(_freeList.compare_exchange_weak(oldHead, newHead, memory_order_acquire, memory_order_relaxed))
            return oldHead;
    }
}

vector<MemoryPool> HashBucket::_memoryPoolSet(HASH_BACKET_SIZE);
bool HashBucket::_initFlag = false;

void HashBucket::initMemoryPoolSet() {
    for(size_t i = 0; i < SLOT_MAX_SIZE; ++i) {
        _memoryPoolSet[i].init(SLOT_BASE_SIZE + i * SLOT_BASE_SIZE);
    }
}

MemoryPool &HashBucket::getMemoryPool(std::size_t sz)
{
    return _memoryPoolSet[(sz + SLOT_BASE_SIZE - 1) / SLOT_BASE_SIZE - 1];
}
}

/**
 * @warning 
 * 1. allocate(): _freeList != nullptr的判断
 * 2. allocate(): _curSlot值的更新 _curSlot += _slotSize / sizeof(Slot);
 * 3. deallocate(): 需要判断形参指针是否为空
 * 4. init(): 需要使用assert()进行判断
 * 
*/