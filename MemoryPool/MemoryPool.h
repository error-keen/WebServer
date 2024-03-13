#ifndef MEMORY_POOL_H
#define MEMORY_POOL_H
 
#include <climits>
#include <cstddef>
 
//allocate    分配一个对象所需的内存空间
//
//deallocate   释放一个对象的内存（归还给内存池，不是给操作系统）
//
//construct   在已申请的内存空间上构造对象
//
//destroy  析构对象
//
//newElement  从内存池申请一个对象所需空间，并调用对象的构造函数
//
//deleteElement  析构对象，将内存空间归还给内存池
//
//allocateBlock  从操作系统申请一整块内存放入内存池
 
 
template <typename T, size_t BlockSize = 4096>
class MemoryPool
{
public:
    /* Member types */
    typedef T               value_type;//值类型
    typedef T*              pointer;//指针类型
    typedef T&              reference;//非常量引用
    typedef const T*        const_pointer;//常量指针
    typedef const T&        const_reference;//常量引用
    typedef size_t          size_type;//无符号整型 unsigned int
    typedef ptrdiff_t       difference_type;//普通int类型
    typedef std::false_type propagate_on_container_copy_assignment;
    typedef std::true_type  propagate_on_container_move_assignment;
    typedef std::true_type  propagate_on_container_swap;
 
    template <typename U> struct rebind {//结构体rebind
        typedef MemoryPool<U> other;//将 Memory<U> 改名为 other
    };
 
    /* Member functions */
    MemoryPool() noexcept;
    MemoryPool(const MemoryPool& memoryPool) noexcept;
    MemoryPool(MemoryPool&& memoryPool) noexcept;
    template <class U> MemoryPool(const MemoryPool<U>& memoryPool) noexcept;
 
    ~MemoryPool() noexcept;
 
    MemoryPool& operator=(const MemoryPool& memoryPool) = delete;//删除默认拷贝构造函数
    MemoryPool& operator=(MemoryPool&& memoryPool) noexcept;//移动赋值函数
 
    pointer address(reference x) const noexcept;//返回非常量引用类型变量地址
    const_pointer address(const_reference x) const noexcept;//返回常量引用类型变量地址
 
    // Can only allocate one object at a time. n and hint are ignored
    //一次只能为一个目标分配空间 常量指针（const T*  -> 不可以修改该地址存放的数据）
    pointer allocate(size_type n = 1, const_pointer hint = 0);//均为可缺省参数
    void deallocate(pointer p, size_type n = 1);//回收内存 归还给Block内存块
 
    size_type max_size() const noexcept;//计算最大可使用的 Slot 槽
 
    template <class U, class... Args> void construct(U* p, Args&&... args);
    template <class U> void destroy(U* p);//调用析构函数释放 p 所指空间
 
    template <class... Args> pointer newElement(Args&&... args);
    void deleteElement(pointer p);
 
private:
    union Slot_ {
        value_type element;//使用时为 value_type 类型
        Slot_* next;//需要回收时为 Slot_* 类型并加入 空闲链表中
    };
 
    typedef char* data_pointer_;//字符类型指针
    typedef Slot_ slot_type_;//槽类型
    typedef Slot_* slot_pointer_;//槽类型指针
 
    slot_pointer_ currentBlock_;//指向第一块 Block 内存块
    slot_pointer_ currentSlot_;//指向第一个可用元素 Slot  元素链表的头指针
    slot_pointer_ lastSlot_;//可存放元素的最后指针
    slot_pointer_ freeSlots_;//被释放的元素存放于空闲链表， freeSlots_为链表头指针
 
    size_type padPointer(data_pointer_ p, size_type align) const noexcept;
    void allocateBlock();//申请一块新的Block
 
    //静态断言：当申请的BlockSize小于两个slot槽的内存时报错
    static_assert(BlockSize >= 2 * sizeof(slot_type_), "BlockSize too small.");
};
 
#include "MemoryPool.tcc"
 
#endif // MEMORY_POOL_H