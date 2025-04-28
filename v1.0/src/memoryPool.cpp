#include "../include/memoryPool.h"
#include "memoryPool.h"
using std::lock_guard;
using std::mutex;
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
void MemoryPool::init(int SlotSize) {
    _slotSize = SlotSize;
}

void *MemoryPool::allocate() {
/// @brief 需要在大内存块中分配出一个slot，因此_freeList and _curSlot就是临界资源，进行分配的操作就是临界区，需要进行同步操作
/// @details 首先应该在_freeList中申请空闲slot，若分配失败，再在_curSlot中分配slot，若继续失败，那么应该向操作系统申请新的
/// 一整块大内存，然后进行分配
    {
        lock_guard<mutex> freeListLock(_mutexForFreeList);
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

void MemoryPool::deallocate(Slot *freeSlot) {
/// @brief _freeList是临界资源，需要进行同步操作
    lock_guard<mutex> freeListLock(_mutexForFreeList);
    freeSlot->_next = _freeList;
    _freeList = freeSlot;
}

/// @attention 是不是需要将加锁操作放在实现函数中，这样同步意图更加明显，更加具体
void MemoryPool::newBlock() {
/// @brief 因为是具体的实现函数，而加锁操作已经在接口函数完成了，所以仍然保持同步
    Slot *newblock = reinterpret_cast<Slot*>(operator new(_blockSize));
    // 接下来要充分的利用这一大块内存
    newblock->_next = _firstBlock;
    _firstBlock = newblock;
    // 接下来需要重设_curSlot and _lastSlot
    // 但在此之前，需要进行对齐操作
    _curSlot = _firstBlock + paddingBlock(_firstBlock, _slotSize);
    // 因为已经进行内存对齐操作了，所以内存池中能够存放的最后一个内存块的实际位置是不确定的，唯一能够确定的就是
    // 不足够存放一个内存块的起始位置，作为需要进行申请新的内存池块的位置标识
    _lastSlot = reinterpret_cast<Slot *>(reinterpret_cast<char *>(_firstBlock) + _blockSize - _slotSize + 1);
}

int MemoryPool::paddingBlock(Slot *slot, int align) {
/// @remark 有优化的空间，可以使用位运算比模运算快10倍
    return (align - (reinterpret_cast<uintptr_t>(slot) % align)) % align;
}

void *MemoryPool::popFreeList() {
/// @brief 先从空闲链表中分配slot，然后更新_freeList，同样属于实现函数，加锁操作已经在接口函数中完成了
    if(_freeList == nullptr)
        return nullptr;
    Slot *curFreeSlot = _freeList;
    _freeList = curFreeSlot->_next;
    return reinterpret_cast<void*>(curFreeSlot);
}

void MemoryPool::pushFreeList(Slot *slot) {
    slot->_next = _freeList;
    _freeList = slot;
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
    if(sz == 0)
        throw runtime_error("the allocated size is empty that is wrong.");
    return _memoryPoolSet[(sz + SLOT_BASE_SIZE - 1) / SLOT_BASE_SIZE - 1];
}
}