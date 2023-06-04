


## 原来是将一个src目录下所有文件打包成mprrpc库，但是现在是将多个分散的文件夹里面的文件打包成库
aux_source_directory(. SRC_LIST)
add_library(example ${SRC_LIST})