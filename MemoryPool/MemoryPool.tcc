#ifndef MEMORY_BLOCK_TCC
#define MEMORY_BLOCK_TCC
 
//计算对齐所需补的空间
template <typename T, size_t BlockSize>
inline typename MemoryPool<T, BlockSize>::size_type
MemoryPool<T, BlockSize>::padPointer(data_pointer_ p, size_type align)
const noexcept
{
  uintptr_t result = reinterpret_cast<uintptr_t>(p);//占有的空间
  return ((align - result) % align);//多余不够一个Slot槽大小的空间，需要跳过
}
 
//构造函数 初始化所有指针为 nullptr
template <typename T, size_t BlockSize>
MemoryPool<T, BlockSize>::MemoryPool()
noexcept
{
  currentBlock_ = nullptr;//指向第一块Block区 即Block内存块链表的头指针
  currentSlot_ = nullptr;//当前第一个可用槽的位置
  lastSlot_ = nullptr;//最后可用Slot的位置
  freeSlots_ = nullptr;//空闲链表头指针
}
 
//拷贝构造函数
template <typename T, size_t BlockSize>
MemoryPool<T, BlockSize>::MemoryPool(const MemoryPool& memoryPool)
noexcept :
MemoryPool()
{}
 
//移动构造函数
template <typename T, size_t BlockSize>
MemoryPool<T, BlockSize>::MemoryPool(MemoryPool&& memoryPool)
noexcept
{
  currentBlock_ = memoryPool.currentBlock_;
  memoryPool.currentBlock_ = nullptr;
  currentSlot_ = memoryPool.currentSlot_;
  lastSlot_ = memoryPool.lastSlot_;
  freeSlots_ = memoryPool.freeSlots;
}
 
//拷贝构造函数
template <typename T, size_t BlockSize>
template<class U>
MemoryPool<T, BlockSize>::MemoryPool(const MemoryPool<U>& memoryPool)
noexcept :
MemoryPool()
{}
 
//移动赋值函数
template <typename T, size_t BlockSize>
MemoryPool<T, BlockSize>&
MemoryPool<T, BlockSize>::operator=(MemoryPool&& memoryPool)
noexcept
{
  if (this != &memoryPool)//若不是自己给自己赋值,进入if主体
  {
    std::swap(currentBlock_, memoryPool.currentBlock_);//交换第一块Block的位置
    //swap函数内部使用的也是移动赋值机制，确保了不会有多个指针指向一块空间，这样就不会出现一块空间被重复释放的问题
    currentSlot_ = memoryPool.currentSlot_;//其余指针赋值
    lastSlot_ = memoryPool.lastSlot_;
    freeSlots_ = memoryPool.freeSlots;
  }
  return *this;
}
 
template <typename T, size_t BlockSize>
MemoryPool<T, BlockSize>::~MemoryPool()//析构函数
noexcept
{
  slot_pointer_ curr = currentBlock_;//指向第一块内存区block
  while (curr != nullptr) {
    slot_pointer_ prev = curr->next;//保存下一个槽的位置
    operator delete(reinterpret_cast<void*>(curr));//释放空间 转为 void* 不需要调用析构函数
    curr = prev;//往前走
  }
}
 
//返回引用类型变量地址
template <typename T, size_t BlockSize>
inline typename MemoryPool<T, BlockSize>::pointer
MemoryPool<T, BlockSize>::address(reference x)
const noexcept
{
  return &x;
}
 
//返回常量引用类型变量地址
template <typename T, size_t BlockSize>
inline typename MemoryPool<T, BlockSize>::const_pointer
MemoryPool<T, BlockSize>::address(const_reference x)
const noexcept
{
  return &x;
}
 
//申请一块新的分区block
template <typename T, size_t BlockSize>
void
MemoryPool<T, BlockSize>::allocateBlock()
{
  // Allocate space for the new block and store a pointer to the previous one
  //申请 BlockSize字节大小的一块空间并用char* 类型指针接收
  data_pointer_ newBlock = reinterpret_cast<data_pointer_>
                           (operator new(BlockSize));
  //newBlock成为新的block内存首址              
  reinterpret_cast<slot_pointer_>(newBlock)->next = currentBlock_;
 
  //currentBlock 更新为 newBlock的位置
  currentBlock_ = reinterpret_cast<slot_pointer_>(newBlock);
 
  // Pad block body to staisfy the alignment requirements for elements
  //保留第一个slot 用于Block链表的链接
  data_pointer_ body = newBlock + sizeof(slot_pointer_);
 
  //求解空间对齐需要的字节数
  size_type bodyPadding = padPointer(body, alignof(slot_type_));
 
  //若出现残缺不可以作为一个slot的空间，跳过这块空间作为第一个可用slot的位置 
  currentSlot_ = reinterpret_cast<slot_pointer_>(body + bodyPadding);
  lastSlot_ = reinterpret_cast<slot_pointer_>
              (newBlock + BlockSize - sizeof(slot_type_) + 1);
   //始址: newBlock  块大小 BlockSize 末址 newBlock + BlockSize 末址减去一个slot槽大小
   //得到倒数第二个slot的末址 再加一得到最后一块slot的始址
}
 
template <typename T, size_t BlockSize>
inline typename MemoryPool<T, BlockSize>::pointer
MemoryPool<T, BlockSize>::allocate(size_type n, const_pointer hint)
{//在内存池中申请 n 个Node，hint 默认设置为 空
  if (freeSlots_ != nullptr) {//空闲链表不为空，表示链表中有slot可以使用
    pointer result = reinterpret_cast<pointer>(freeSlots_);//将头结点分配出去
    freeSlots_ = freeSlots_->next;//头结点更新
    return result;
  }
  else {//空闲链表没有 slot 
  //如果当前可用slot槽已经到了最后的位置,用完之后需要再创建一块Block方便后续操作
    if (currentSlot_ > lastSlot_)
      allocateBlock();
    return reinterpret_cast<pointer>(currentSlot_++);//返回分配的空间并更新 currentSlot
    //currentSlot 指向Block内存块中最前面可用的Slot槽 (freeSlot链表中不算)
  }
}
 
template <typename T, size_t BlockSize>
inline void
MemoryPool<T, BlockSize>::deallocate(pointer p, size_type n)
{
  if (p != nullptr) {
    reinterpret_cast<slot_pointer_>(p)->next = freeSlots_;//union指针域被赋值 之前的数据销毁
    freeSlots_ = reinterpret_cast<slot_pointer_>(p);//空闲链表头结点更新
  }
}
 
//计算最大可用槽Slot的个数
template <typename T, size_t BlockSize>
inline typename MemoryPool<T, BlockSize>::size_type
MemoryPool<T, BlockSize>::max_size()
const noexcept
{ //无符号整型 unsiged int 类型运算,先将 -1 转换为无符号整型 即无符号整型最大值
  size_type maxBlocks = -1 / BlockSize;//无符号类型数值可以表示的Block的个数
  //unsigned int MAX / BlockSize 向下取整
 
  return (BlockSize - sizeof(data_pointer_)) / sizeof(slot_type_) * maxBlocks;
     //  (BlockSize - sizeof(data_pointer)) / sizeof(slot_type_) →指每一块Block中可以存放的slot槽个数
    // 最大可以有 maxBlocks 块内存块
    //因此最大的可用槽个数 为每一块Block可以使用的Slot槽个数乘以最大可用Block
}
 
 
template <typename T, size_t BlockSize>
template <class U, class... Args>
inline void
MemoryPool<T, BlockSize>::construct(U* p, Args&&... args)
{
//完美转发 保存参数原有的属性 是左值还是左值 是右值还是右值
  new (p) U (std::forward<Args>(args)...);
  //placement new 的应用，即在已经申请的空间上申请内存分配
}
 
//调用析构函数
template <typename T, size_t BlockSize>
template <class U>
inline void
MemoryPool<T, BlockSize>::destroy(U* p)
{//调用析构函数释放这一块内存
  p->~U();
}
 
template <typename T, size_t BlockSize>
template <class... Args>
inline typename MemoryPool<T, BlockSize>::pointer
MemoryPool<T, BlockSize>::newElement(Args&&... args)
{
//为每个元素分配空间
  pointer result = allocate();
  construct<value_type>(result, std::forward<Args>(args)...);//参数包解包过程
  return result;//返回每一个新申请空间的地址
}
 
 
//删除元素
template <typename T, size_t BlockSize>
inline void
MemoryPool<T, BlockSize>::deleteElement(pointer p)
{//                                       ↑传入 T* 类型的指针
  if (p != nullptr) {//判断指针为非空
    p->~value_type();//调用值类型的析构函数  typedef T  value_type;
    deallocate(p);//将这块内存还给内存池
  }
}
 
#endif // MEMORY_BLOCK_TCC