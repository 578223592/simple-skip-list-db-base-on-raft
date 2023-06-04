# set(SRC_LIST userservice.cc ../user.pb.cc)
aux_source_directory(. SRC_LIST)

add_executable(callerMain ${SRC_LIST})
target_link_libraries(callerMain skip_list_on_raft )




add_executable(raftKvDb ${SRC_LIST})
target_link_libraries(raftKvDb skip_list_on_raft )


