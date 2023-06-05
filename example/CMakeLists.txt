
set(SRC_LIST1 callerMain.cpp)
add_executable(callerMain ${SRC_LIST1})
target_link_libraries(callerMain skip_list_on_raft  protobuf boost)



set(SRC_LIST2 raftKvDb.cpp)
add_executable(raftKvDb ${SRC_LIST2})
target_link_libraries(raftKvDb skip_list_on_raft  protobuf boost)


