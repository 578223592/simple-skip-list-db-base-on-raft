# simple-skip-list-db-base-on-raft


完成任务：将go上的raft迁移到c++实现。
需要的大步骤：
1. rpc的实现
   迁移已经学习过的rpc库：rpc1

2. goruntime的替代
   好像没有办法很好替代，只能手动开多线程来处理。
    但是可以开线程池来实现，这样的话可能存在有的线程一直没执行完导致线程不够用卡死的情况。

3. go的chan的替代
   在本项目中往往只有一个线程来接收消息，不会存在多个线程来一起竞争chan的问题，因此c++中只需要简单的实现多线程通通信即可。
   简单来说就是使用锁（go中也实现了，因此这里不算多用） + 条件变量（替代管道）实现即可。


## todoList
- [ ] rpc的改造 预计今天实现 2023-5-26     实际实现：未完成，先延后，因为需要先完成cmake编译项目的学习。
- [x] raft迁移，预计两天  2023-5-28开始                实际实现： 2023年05月31日
- [ ] kvserver迁移，预计半天   2023年06月01日开始
- [ ] 配置整个项目的cmake，cmake需要学习  cmake学习参考：https://www.bilibili.com/video/BV18R4y127UV/?spm_id_from=333.337.search-card.all.click&vd_source=b39a7d56e3c8769f8d478b0c4cac403e 和同门哈哈哈
- [ ] goruntime更加优雅的实现，使用线程池
- [ ] 添加跳表，预计两天
- [ ] 使用智能指针和范围锁保证良好的资源管理 https://blog.csdn.net/yu_xiaoxian_2018/article/details/112689345
- [ ] 持久化方案，备选：手动实现|protobuf|boost的序列化库