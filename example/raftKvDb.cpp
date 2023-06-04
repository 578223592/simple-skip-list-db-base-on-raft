//
// Created by swx on 23-6-4.
//

#include "kvServer.h"
#include <iostream>
void ShowArgsHelp();

int main(int argc, char **argv){
    //////////////////////////////////读取命令参数：节点数量、写入raft节点节点信息到哪个文件
    if (argc < 2)
    {
        ShowArgsHelp();
        exit(EXIT_FAILURE);
    }
    int c = 0;
    int nodeNum = 0;
    std::string configFileName;
    while((c = getopt(argc, argv, "n:f:")) != -1)
    {
        switch (c)
        {
            case 'n':
                nodeNum = atoi( optarg);
                break;
            case 'f':
                configFileName = optarg;
                break;
            default:
                ShowArgsHelp();
                exit(EXIT_FAILURE);

        }
    }

    for(int i = 0;i<nodeNum;i++){
        std::cout<<"start to create raftkv" <<i<<endl;
        pid_t pid = fork(); // 创建新进程
        if (pid == 0) { // 如果是子进程
            // 子进程的代码
            auto  kvServer = new KvServer(i,500,"\"test.conf\"");
            pause(); // 子进程进入等待状态，不会执行 return 语句
        } else if (pid > 0) { // 如果是父进程
            // 父进程的代码
            sleep(5); // 等待5秒钟
        } else { // 如果创建进程失败
            std::cerr << "Failed to create child process." << std::endl;
            exit(EXIT_FAILURE);
        }

    }


    return 0;
}

void ShowArgsHelp() {

    std::cout << "format: command -n <nodeNum> -f <configFileName>" << std::endl;

}
