#include "../include/CentralCache.h"

// 南江，拿出你的激情，拿出你的勇气，大胆向前，不要退缩，不要退缩，不要，大胆地去尝试，失败了又如何
// 况且，你是天才，你一定可以写出来的
// 怕什么，自己写的，自己设计的，就跟自己的孩子一样，要勇敢地去修改它

namespace memoryPool {

constexpr size_t CentralCache::SPAN_PAGES;

void *CentralCache::fetchRange(size_t index) {
    /**
     * @details
     * 1) 首先判断index是否有效
     * 2) 自旋锁进行加锁保护
     * 3) 首先尝试从自由链表中申请内存
     * 4) 若内存不足，则向PageCache申请内存，然后分割成小块
     * 5) 若内存充足，则更新自由链表之后，返回内存
     * @note 第一次编写基本上是成功的，但是存在2点不足
     * 1) 没有异常处理
     * 2) 只有当可以分成2个内存块以上，才需要进行小块的连接处理
    */
   if(index >= FREE_LIST_SIZE)
        return nullptr; 
    
    // 抢夺自旋锁，用于同步操作
    while (_locks[index].test_and_set(std::memory_order_acquire))
    {
        // 线程让步，避免消耗CPU资源
        std::this_thread::yield();
    }

    void *result = nullptr;
    try {
        result = _centralFreeList[index].load(std::memory_order_relaxed);
        if(!result) {
            // 自由链表中没有空闲内存块，只能去PageCache请求分配内存
            size_t size = (index + 1) * ALIGNS;
            result = fetchFromPageCache(size);
            if(!result) {
                // 说明分配失败
                _locks[index].clear(std::memory_order_release);
                return nullptr;
            }

            // 分配成功，需要将大内存切分成一个又一个小块，组成自由链表
            char *start = reinterpret_cast<char*>(result);
            size_t blockNum = SPAN_PAGES * PageCache::PAGE_SIZE / size;
            // 只有1块以上才有意义进行内存间的连接，并更新中心缓存的自由链表，仅仅1块不需要做接下来的事情
            if(blockNum > 1) {
                for(size_t i = 1; i < blockNum; i++) {
                    void *current = reinterpret_cast<void*>(start + (i - 1) * size);
                    void *next = reinterpret_cast<void*>(start + i * size);
                    *reinterpret_cast<void**>(current) = next;
                }
                *reinterpret_cast<void**>(start + (blockNum-1) * size) = nullptr;

                // 切分完小块后，需要更新自由链表
                _centralFreeList[index].store(
                    *reinterpret_cast<void**>(result), std::memory_order_release
                );
            }
        }
        else {
            // 自由链表中有空闲的内存，只需要更新自由链表即可
            while (_centralFreeList[index].compare_exchange_weak(
                result, *reinterpret_cast<void**>(result), 
                std::memory_order_release, std::memory_order_relaxed
            ));
        }
    } catch(...) {
        _locks[index].clear(std::memory_order_release);
        throw;
    }

    // 完成分配工具后，需要解决自旋锁，返回值
    _locks[index].clear(std::memory_order_release);
    return result;
}

void CentralCache::returnRange(void *start, size_t index) {
    /**
     * @brief 将空闲块start插入到自由链表中
     * 1) 首先仍然进行形参检验是否有效，包括指针非空和索引在FREE_LIST_SIZE范围内
     * 2) 需要先加锁，因为只有一个自由链表，需要进行同步操作
     * 3) 形参有效，就尝试将空闲块插入到空闲链表中 
     * 4) 插入成功后，需要释放自旋锁
     * 
     * @attention 原项目中returnRange含有3个形参(void *start, size_t size, size_t index)
     * 但在实际实现过程中，发现仅仅需要两个形参void *start and size_t index
    */

    if(!start || index >= FREE_LIST_SIZE)
        return;
    
    while (_locks[index].test_and_set(std::memory_order_acquire)) {
        std::this_thread::yield();  // 线程让步
    }
    
    try {
        void *current = _centralFreeList[index].load(std::memory_order_relaxed);
        *reinterpret_cast<void**>(start) = current;
        _centralFreeList[index].store(start, std::memory_order_release);
    } catch(...) {
        _locks[index].clear();
        return;
    }

    _locks[index].clear(std::memory_order_release);
    return;
}

CentralCache::CentralCache() {
    /**
     * @breif 作为构造函数，我的疑问是：需不需要进行同步操作？或者原子操作就足够吗？
    */
   for (auto &list : _centralFreeList)
   {
        list.store(nullptr, std::memory_order_relaxed);
   }
   for (auto &lock : _locks)
   {
        lock.clear();
   }
}

void *CentralCache::fetchFromPageCache(size_t size) {
    // TODO
}

void CentralCache::returnToPageCache(void *ptr, size_t pageNums) {
    // TODO
}

}