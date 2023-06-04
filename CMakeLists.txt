cmake_minimum_required(VERSION 3.25)
project(skip_list_on_raft)

set(CMAKE_CXX_STANDARD 14)
# 生成debug版本，可以进行gdb调试
set(CMAKE_BUILD_TYPE "Debug")



# 设置项目可执行文件输出的路径
set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/out_bin)
# 设置项目库文件输出的路径
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/out_lib)


# 设置项目编译头文件搜索路径 -I
include_directories(rpc/example)
include_directories(rpc/experiment/lock_free_queue)
include_directories(rpc/experiment/zkutils)
include_directories(rpc/lib)
include_directories(rpc/lib/include)
include_directories(rpc/src/include)
include_directories(rpc/test/protobuf)
include_directories(raft/include)

# 设置项目库文件搜索路径 -L
link_directories(${PROJECT_SOURCE_DIR}/out_lib)



# src包含了mprpc框架所有的相关代码
add_subdirectory(raft)
# example包含了mprpc框架使用的示例代码
add_subdirectory(example)
add_subdirectory(rpc)


target_link_libraries(skip_list_on_raft muduo_net muduo_base pthread raft rpc)

#
#add_executable(skip_list_on_raft
#        rpc/mprpc/example/callee/friendservice.cc
#        rpc/mprpc/example/callee/userservice.cc
#        rpc/mprpc/example/caller/callfriendservice.cc
#        rpc/mprpc/example/caller/calluserservice.cc
#        rpc/mprpc/example/friend.pb.cc
#        rpc/mprpc/example/friend.pb.h
#        rpc/mprpc/example/raftRPC.pb.cc
#        rpc/mprpc/example/raftRPC.pb.h
#        rpc/mprpc/example/user.pb.cc
#        rpc/mprpc/example/user.pb.h
#        rpc/mprpc/experiment/hs/hash1.cpp
#        rpc/mprpc/experiment/lock_free_queue/lock_free.h
#        rpc/mprpc/experiment/lock_free_queue/main.cpp
#        rpc/mprpc/experiment/zkutils/zk_cpp.cpp
#        rpc/mprpc/experiment/zkutils/zk_cpp.h
#        rpc/mprpc/experiment/zkutils/zk_cpp_test.cpp
#        rpc/mprpc/src/include/lockqueue.h
#        rpc/mprpc/src/include/logger.h
#        rpc/mprpc/src/include/mprpcapplication.h
#        rpc/mprpc/src/include/mprpcchannel.h
#        rpc/mprpc/src/include/mprpcconfig.h
#        rpc/mprpc/src/include/mprpccontroller.h
#        rpc/mprpc/src/include/rpcheader.pb.h
#        rpc/mprpc/src/include/rpcprovider.h
#        rpc/mprpc/src/include/zookeeperutil.h
#        rpc/mprpc/src/logger.cc
#        rpc/mprpc/src/mprpcapplication.cc
#        rpc/mprpc/src/mprpcchannel.cc
#        rpc/mprpc/src/mprpcconfig.cc
#        rpc/mprpc/src/mprpccontroller.cc
#        rpc/mprpc/src/rpcheader.pb.cc
#        rpc/mprpc/src/rpcprovider.cc
#        rpc/mprpc/src/zookeeperutil.cc
#        rpc/mprpc/test/protobuf/main.cc
#        rpc/mprpc/test/protobuf/test.pb.cc
#        rpc/mprpc/test/protobuf/test.pb.h
#        raft/include/ApplyMsg.h
#        raft/include/raft.h
#        raft/include/raftrpc.h
#        raft/include/util.h
#        raft/raft.cpp
#        test/defer_test.cpp
#        test/format.cpp
#        test/sleep_test.cpp raft/include/config.h test/thread_test.cpp test/random_test.cpp raft/include/Persister.h raft/include/kvServer.h raft/kvServer.cpp test/timout_pop.cpp test/serialization.cpp raft/include/clerk.h raft/clerk.cpp raft/include/raftserverRpc.h example/callerMain.cpp example/raftKvDb.cpp)
