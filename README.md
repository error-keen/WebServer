# WebServer

用C++实现的高性能web服务器。

## 功能

- 利用IO复用技术Epoll与线程池实现多线程的Reactor高并发模型；
- 利用正则与状态机解析HTTP请求报文，实现处理静态资源的请求；
- 利用标准库容器封装char，实现自动增长的缓冲区；
- 基于堆结构实现的定时器，关闭超时的非活动连接；
- 改进了线程池的实现，QPS提升了40%+；

## 项目详解

### 1.本项目的事件处理模式为：从Reactor加线程池的模式

![image-20231031170635587](https://github.com/error-keen/data-structure/blob/main/img/web1.png)

项目基于 HTTPconnection 类、HTTPrequest 类、HTTPresponse 类、timer 类、epoller 类、threadpool 类实现一个完整的高性能 web 服务器的所有功能。

- **HTTPconnection 类:**这个类就是对一个 HTTP 连接的抽象，负责对一个 HTTP 请求的解析和回复，以及提供读写的接口。这个读写接口的底层就是靠 buffer 缓冲区来实现的，这个缓冲区提供了读写的接口。但是，写借口照样用了分散写的方法实现。然后就是对从 socket 连接中读取的数据进行解析，以及对请求做出响应。这部分的实现主要依赖于 HTTPrequest 和 HTTPresponse 来完成。

- **HTTPrequest 类:**这个类主要的功能是解析 HTTP 的请求信息。

- **HTTPresponse 类:**这个类和 HTTPrequest 相反，是给相应的连接生成相应报文的。HTTPrequest 是解析请求行，请求头和数据体，那么 HTTPresponse 就是生成请求行，请求头和数据体。

- **timer 类:**为了提高 Web 服务器的效率，我们考虑给每一个 HTTP 连接加一个定时器。定时器给每一个 HTTP 连接设置一个过期时间，然后我们定时清理超过过期时间的连接，会减少服务器的无效资源的耗费，提高服务器的运行效率。我们还需要考虑一下如何管理和组织这些定时器。设置定时器的主要目的是为了清理过期连接，为了方便找到过期连接，首先考虑使用优先队列，按过期时间排序，让过期的排在前面就可以了。但是这样的话，虽然处理过期连接方便了，当时没法更新一个连接的过期时间。最后，选择一个折中的方法。用 vector 容器存储定时器，然后在这之上实现堆结构，这样各个操作的代价就得到了相对均衡。

- **epoller 类:**web 服务器需要与客户端之间发生大量的 IO 操作，这也是性能的瓶颈之一。在这个项目中，我们用 IO 多路复用技术中的 epoll 来尽可能地提高一下性能。epoll 区别于 select 和 poll，不需要每次轮询整个描述符集合来查找哪个描述符对应的 IO 已经做好准备了，epoll 采用事件驱动的方式，当有事件准备就绪后就会一次返回已经做好准备的所有描述符集合。

- **threadpool 类:**线程池是由服务器预先创建的一组子线程，线程池中的线程数量应该和 CPU 数量差不多。线程池中的所有子线程都运行着相同的代码。当有新的任务到来时，主线程将通过某种方式选择线程池中的某一个子 线程来为之服务。相比与动态的创建子线程，选择一个已经存在的子线程的代价显然要小得多。

  本项目使用的是随机算法工作队列。主线程使用某种算法来主动选择子线程，最简单、最常用的算法是随机算法和 Round Robin（轮流 选取）算法，但更优秀、更智能的算法将使任务在各个工作线程中更均匀地分配，从而减轻服务器 的整体压力。

  线性池模型：

  ![image-20231031173927837](https://github.com/error-keen/data-structure/blob/main/img/web2.png)

  

- **webserver类：**初始化服务器，为 HTTP 的连接做好准备，然后处理每一个 HTTP 连接，用定时器给每一个 HTTP 连接定时，并且处理掉过期的连接，运用 IO 多路复用技术提升 IO 性能，运用线程池技术提升服务器性能。

### 2.内存池(MemoryPool)

内存池中哈希桶的思想借鉴了STL allocator，具体每个⼩内存池的实现参照了GitHub上的项⽬：[cacay/MemoryPool: An easy to use and efficient memory pool allocator written in C++. (github.com)](https://github.com/cacay/MemoryPool)

![image-20231031170635589](https://github.com/error-keen/data-structure/blob/main/img/web3.png)

主要框架如上图所示，主要就是维护⼀个哈希桶 MemoryPools ，⾥⾯每项对应⼀个内存池 MemoryPool ，哈希桶中每个内存池的块⼤⼩ BlockSize 是相同（4096字节，当然也可以设置为不同的），但是每个内存池⾥每个块分割的⼤⼩（槽⼤⼩） SlotSize 是不同的，依次为8,16,32,...,512字节（需要的内存超过512字节就
⽤ new/malloc ），这样设置的好处是可以保证内存碎⽚在可控范围内。

![image-20231031170635589](https://github.com/error-keen/data-structure/blob/main/img/web4.png)

内存池的内部结构如上图所示，主要的对象有：指向第⼀个可⽤内存块的指针 Slot currentBlock （也是图中的 ptr to firstBlock ，图⽚已经上传懒得改了），被释放对象的slot链表 Slot freeSlot ，未使⽤的slot链 表 Slot* currentSlot ，下⾯讲下具体的作⽤： 

1. Slot currentBlock ：内存池实际上是⼀个⼀个的 Block 以链表的形式连接起来，每⼀个 Block 是⼀块 ⼤的内存，当内存池的内存不⾜的时候，就会向操作系统申请新的 block 加⼊链表。 
2. Slot freeSlot ：链表⾥⾯的每⼀项都是对象被释放后归还给内存池的空间，内存池刚创建时 freeSlot 是空的。⽤户创建对象，将对象释放时，把内存归还给内存池，此时内存池不会将内存归还给系统 （ delete/free ），⽽是把指向这个对象的内存的指针加到 freeSlot 链表的前⾯（前插），之后⽤户每次 申请内存时， memoryPool 就先在这个 freeSlot 链表⾥⾯找。 
3. Slot curretSlot ：⽤户在创建对象的时候，先检查 freeSlot 是否为空，不为空的时候直接取出⼀项作 为分配出的空间。否则就在当前 Block 将 currentSlot 所指的内存分配出去，如果 Block ⾥⾯的内存已 经使⽤完，就向操作系统申请⼀个新的 Block 。

### 3.日志系统

服务器的⽇志系统是⼀个多⽣产者，单消费者的任务场景：多⽣产者负责把⽇志写⼊缓冲区，单消费者负责把缓冲区中数据写⼊⽂件。如果只⽤⼀个缓冲区，不光要同步各个⽣产者,还要同步⽣产者和消费者。⽽且最重要的是需要保证⽣产者与消费者的并发，也就是前端不断写⽇志到缓冲区的同时，后端可以把缓冲区写⼊⽂件。日志系统通过单例模式还有双缓冲区、异步线程，实现日志文件的异步写入。通过使用宏定义简化日志系统的使用。
流程：

1. 单例模式（局部静态变量懒汉方法）获取实例
2. 主程序一开始Log::get_instance()->init()初始化实例。初始化后：服务器启动按当前时刻创建日志（前缀为时间，后缀为自定义log文件名，并记录创建日志的时间day和行数count）。如果是异步(通过是否设置队列大小判断是否异步，0为同步)，工作线程将要写的内容放进阻塞队列，还创建了写线程用于在阻塞队列里取出一个内容(指针)，写入日志。
3. 其他功能模块调用write_log()函数写日志。（write_log：实现日志分级、分文件、按天分类，超行分类的格式化输出内容。）里面会根据异步、同步实现不同的写方式。


## 环境要求

- Linux
- C++11

## 项目启动

```
mkdir bin
make
./bin/myserver
```

## 压力测试

```
./webbench-1.5/webbench -c 100 -t 10 http://ip:port/
./webbench-1.5/webbench -c 1000 -t 10 http://ip:port/
./webbench-1.5/webbench -c 5000 -t 10 http://ip:port/
./webbench-1.5/webbench -c 10000 -t 10 http://ip:port/
```

- 测试环境: Ubuntu:20 cpu:i7-4790 内存:16G

## 性能表现

与[markparticle](https://github.com/markparticle/WebServer/)的C++服务器做一个比较(表格中的为QPS的值)：

|      |  10   |  100  | 1000  | 10000 |
| :--: | :---: | :---: | :---: | :---: |
| old  | 8837  | 9231  | 9312  |  155  |
| new  | 10342 | 13442 | 13284 |  105  |

性能提升了40%

