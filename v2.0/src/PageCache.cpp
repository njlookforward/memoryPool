#include "../include/PageCache.h"

// 南江，你是一个天才，只要你想，只要你去付出行动，就一定可以实现目标，不要忘了，20行20行的的向前走

namespace memoryPool {
    
    const size_t PageCache::PAGE_SIZE;  // 已经有类内初始值了，因此只需要在类外进行定义就好

PageCache &PageCache::getInstance() {
    static PageCache instance;
    return instance;
}

/**
 * @brief allocateSpan()的逻辑。总的来说，一个内存块归一个Span进行管理，要不停地更新_freeSpans ans _spanMap
 * 1) 首先默认形参是有效的，先进行加锁操作，在自由链表中寻找是否有大内存
 * 2) 如果自由链表中存在大于等于所需内存页大小的空闲内存块，就分配出来，然后更新自由链表
 * 3) 如果空闲内存块的大小大于所需大小，那么需要进行切割操作，需要申请新的Span管理员管理新的小的内存块，同时更新_freeSpans and _spanMap
 * 4) 如果自由链表中没有空闲内存块，就需要向操作系统申请所需大小，然后更新_spanMap
*/
void *PageCache::allocateSpan(size_t pageNums) {
    // 竟然忘了很重要的一步，因为只有一个PageCache，因此对于临界资源_freeSpan and _spanMap需要进行加锁操作
    std::lock_guard<std::mutex> _lock(_mutex);

    auto it = _freeSpans.lower_bound(pageNums);
    if(it != _freeSpans.end()) {
        // 说明存在满足的空闲内存块，需要更新空闲链表
        Span *aimSpan = it->second;
        if(aimSpan->_next != nullptr) {
            _freeSpans[it->first] = aimSpan->_next;
        } else 
            _freeSpans.erase(it);
        
        // 判断是否需要进行切割操作
        if(aimSpan->_pageNums > pageNums) {
            Span *newSpan = new Span;
            newSpan->_pageAddr = reinterpret_cast<void*>(
                reinterpret_cast<char*>(aimSpan) + pageNums * PAGE_SIZE
            );
            newSpan->_pageNums = aimSpan->_pageNums - pageNums;
            
            // 分别插入到_freeSpans and _spanMap
            /// @attention 语雀中竟然没有将新的newSpan加入到_spanMap中，匪夷所思
            auto &list = _freeSpans[newSpan->_pageNums];
            newSpan->_next = list;
            list = newSpan;
            _spanMap[newSpan->_pageAddr] = newSpan;

            // 修改aimSpan, 我认为对于aimSpan不需要更新对应的_spanMap，因为(aimSpan->_pageAddr, aimSpan)已经存在于_spanMap中了
            // 在像下面从操作系统新申请时或者新切割时，只要在新申请Span时就已经将新的Span插入到_spanMap中了
            aimSpan->_pageNums = pageNums;
            aimSpan->_next = nullptr;
        }
        return aimSpan->_pageAddr;
    }

    // 如果没有符合标准的空闲内存块，需要向操作系统申请所需要的大小
    void *memory = systemAlloc(pageNums);
    if(!memory)
        return nullptr; // 需要进行判断操作，一旦系统内存不够分配，可能分配失败的

    Span *newSpan = new Span;
    newSpan->_pageAddr = memory;
    newSpan->_pageNums = pageNums;
    _spanMap[memory] = newSpan;

    return memory;
}

/**
 * @brief void deallocateSpan(void*, size_t)
 * 1) 大块内存回收操作同样需要更新_freeSpans and _spanMap，因此先进行加锁操作
 * 2) 判断待回收的内存是否是PageCache分配的，如果不是立刻返回
 * 3) 待回收的内存需要添加到_freeSpans中，但是可能在自由链表中存在相邻内存块，如果存在需要进行合并操作
 * 4) 如果存在相邻内存块，那么需要先在自由链表中找到目标内存块，然后更新自由链表，将相邻内存块合并到待回收内存块中，然后更新_spanMap
 * 也就是删除被合并的内存块管理员，然后把已经合并后的span管理员添加到自由链表中
*/
void PageCache::deallocateSpan(void *ptr, size_t pageNums) {
    std::lock_guard<std::mutex> _lock(_mutex);

    auto it = _spanMap.find(ptr);
    if(it == _spanMap.end()) return;    // 说明该内存块不是PageCache分配的

    Span *span = it->second;    // 找到目标内存块的管理员
    // 接下来就是如果在自由链表中存在相邻内存块，就不停地更新本目标内存块的管理员的信息
    // 因为有时候先归还靠前的内存块，再归还靠后的内存块，自由链表中实际上存在相邻的内存块，
    // 因此需要使用while循环将所有的相邻内存块合并到目标内存块的span管理员信息中
    while (true)
    {
        void *nextAddr = reinterpret_cast<void*>(
            reinterpret_cast<char*>(span->_pageAddr) + span->_pageNums * PAGE_SIZE
        );

        auto nextIter = _spanMap.find(nextAddr);
        if(nextIter == _spanMap.end()) break;
        Span *nextSpan = nextIter->second;
        // 然后在对应的自由链表中寻找是否存在
        auto nextIterInFree = _freeSpans.find(nextSpan->_pageNums);
        if(nextIterInFree == _freeSpans.end()) break;
        auto &nextList = nextIterInFree->second;
        if(nextList == nextSpan) {
            // 头结点就是目标Span
            if(nextSpan->_next) 
                nextList = nextSpan->_next;
            else
                _freeSpans.erase(nextIterInFree);
        } else {
            Span *prev = nextList;
            while (prev && prev->_next != nextSpan)
            {
                prev = prev->_next;
            }
            if(prev == nullptr) break;
            prev->_next = nextSpan->_next;
        }
        span->_pageNums += nextSpan->_pageNums;
        // 需要删除_spanMap中的相邻内存块管理员
        _spanMap.erase(nextSpan->_pageAddr);
        delete nextSpan;
    }
    
    auto &list = _freeSpans[span->_pageNums];
    span->_next = list;
    list = span;
    return;
}

PageCache::~PageCache() {
    // TODO
}

void *PageCache::systemAlloc(size_t pageNums) {
    //TODO
}

}