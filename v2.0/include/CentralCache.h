#ifndef MEMORYPOOL_CENTRALCACHE_H
#define MEMORYPOOL_CENTRALCACHE_H

// 南江，你已经很棒了，知道不知道，你看，通过一个又一个小模块，你已经完成了PageCache这个大模块，因此
// 继续加油，拿出你的勇气，拿出你的行动，仍然是20行代码20行代码的敲，一个小模块一个小模块的思考与实现
// 我就是可以的

#include "common.h"
#include "PageCache.h"
#include <cstddef>
using std::size_t;

#include <array>
using std::array;

#include <atomic>
using std::atomic; using std::atomic_flag;

#include <thread>

namespace memoryPool {

class CentralCache {
public:
    // 有时候说明自己对代码真的还是不熟悉，因为编写函数声明竟然忘记写()，真是该打啊
    static CentralCache& getInstance() {
        static CentralCache instance;
        return instance;
    }

    /**
     * @brief fetchRange() and returnRange()是CentralCache对ThreadCache的接口
     * @public fetchRange() 接受ThreadCache的内存申请请求，并提供所需大小的内存块 
     * @public returnRange() 接受THreadCache的内存回收请求，将空闲内存块加入到空闲链表中
    */

    void *fetchRange(size_t index);
    void returnRange(void *start, size_t index);

private:
    /**
     * @private CentralCache() _centralFreeList and _locks需要进行初始化，初始化的过程中是否存在同步操作问题需要进行思考
     * @private ~CentralCache() 使用默认的析构函数就足够了，因为PageCache才负责内存块的回收
    */
    CentralCache();
    ~CentralCache() = default;

    /**
     * @brief fetchFromPageCache() and returnToPageCache是CentralCache对PageCache的接口
     * @private fetchFromPageCache() 当自由链表中没有空间内存时，需要向PageCache进行申请
     * @private returnToPageCache() 尽管CentralCache不负责将内存释放给操作系统，但是需要负责将内存回收给PageCache
    */

    void *fetchFromPageCache(size_t size);
    void returnToPageCache(void *ptr, size_t pageNums);

private:

    /**
     * @param _centralFreeList 这是一个哈希数组，根据索引值哈希到对应的自由链表的头指针
     * @param _locks 自旋锁数组，用于保护对应的链表
     * @param SPAN_PAGES 每次向PageCache申请的内存块页数，固定为8
    */

    array<atomic<void*>, FREE_LIST_SIZE> _centralFreeList;
    array<atomic_flag, FREE_LIST_SIZE> _locks;

    static constexpr size_t SPAN_PAGES = 8;
};

}


#endif