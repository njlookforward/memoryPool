#ifndef MEMORYPOOL_PAGECACHE_H
#define MEMORYPOOL_PAGECACHE_H

#include <cstddef>
using std::size_t;

#include <map>
using std::map;

#include <mutex>    // 因为申请PageCache次数并不多，因此采用互斥锁进行同步操作就可以
using std::mutex; using std::lock_guard;

#include <utility>
using std::pair;

/**
 * @brief 作为内存池三级缓存的最底层，所有ThreadCache共享一个PageCache
 * 是大内存的管理员，负责向CentralCache分配内存和回收内存，其中为减少内存申请的系统调用的次数
 * 还会进行大内存块的合并和分割，同时注意其析构函数是真正的需要将申请的内存还给操作系统
*/

/**
 * @njlookforward 南江，请相信自己，一定可以写出来的，不要逃避，不要想着去看视频，
 * 你想想通过认认真真的思考，能够完整的写出来，那是多么令自己自豪的事情啊
 * 每当想要逃避的时候，就往下再多写一行好不好
 * 一个函数一个函数的完成，不要贪多，不要着急，一步一步
 * 任何想要让我放弃的都给我滚蛋，任何想要干扰我的也都给我滚蛋，我是不会动摇的
*/

namespace memoryPool {

class PageCache {
public:
    /**
     * @public 因为PageCache在内存池中是唯一的，因此PageCache是静态局部对象
     * 同时使用静态公有成员函数作为对外接口
    */
   static PageCache &getInstance();
   void *allocateSpan(size_t pageNums);
   void deallocateSpan(void *ptr, size_t pageNums);

private:
    PageCache() = default;
    ~PageCache();
    void *systemAlloc(size_t pageNums);
    
private:
    /**
     * @param Span是大内存块的管理员，Span对象需要在堆空间中单独申请，用来记录大内存块的所有信息，包括之后添加到自由链表中
     * 下一个大内存块管理员Span的地址信息
    */
    struct Span {
        void *_pageAddr = nullptr;    // 起始页的地址
        size_t _pageNums = 0;         // 该大块内存存储的页数
        Span *_next = nullptr;        // 下一个大内存块的地址
    };

    /**
     * @param _freeSpans; 键值对集合<页数pageNums, 自由链表表头指针Span*>, 每一种页数对应自由链表表头指针
     * @param _spanMap 用来记录每一个内存页起始地址 -> Span管理员指针的映射，保存所有的向操作系统申请的内存块的信息
    */
    std::map<size_t, Span*> _freeSpans;     // PageCache的自由链表
    std::map<void*, Span*> _spanMap;        // <分配内存地址_pageAddr, 链表指针_span*>
    std::mutex _mutex;  // 互斥锁

    static const size_t PAGE_SIZE = 4096;
};

}

#endif