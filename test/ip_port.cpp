//#include <iostream>
//#include <unistd.h>
//#include <cstring>
//#include <arpa/inet.h>
//#include <netdb.h>
//
//using namespace std;
//
//#define SERVER_PORT 1999
//int main()
//{
//    int sfd;
//    int ret;
//    struct sockaddr_in ser_addr;//服务器端的结构体
//
//
//    sfd= socket(AF_INET, SOCK_DGRAM, 0);
//    if (sfd < 0)
//    {
//        cout << "socket created fail" << endl;
//        return -1;
//    }
//    cout << "socket created successfully!" << endl;
//
//    char* ip;
//    int i;
//    char hname[128];
//    struct hostent* hent;
//    gethostname(hname, sizeof(hname));
//    hent = gethostbyname(hname);
//    for (i = 0; hent->h_addr_list[i]; i++)
//    {
//        ip = inet_ntoa(*(struct in_addr*)(hent->h_addr_list[i]));//IP地址
//    }
//    cout<<"ip:"<<ip<<endl;
//    memset(&ser_addr, 0, sizeof(ser_addr));
//    ser_addr.sin_family = AF_INET;
//    ser_addr.sin_addr.s_addr = inet_addr(ip);
//    ser_addr.sin_port = htons(SERVER_PORT);  //注意网络序转换
//
//    ret = bind(sfd, (struct sockaddr*)&ser_addr, sizeof(ser_addr));
//    if (ret < 0)
//    {
//        cout << "bind fail" << endl;
//        return -1;
//    }
//    cout << "bind successfully!" << endl;
//
//    char buf[128];
//    socklen_t len = sizeof(ser_addr);
//    int rre = recvfrom(sfd, &buf, sizeof(buf), 0, (struct sockaddr*)&ser_addr, &len);
//    if (rre < 0)
//    {
//        cout << "recvie fail" << endl;
//        return -1;
//    }
//    cout << "recvie successfully!" << endl;
//    cout <<"The acepted information is: " <<buf << endl;
//
//    char buff[128] = "192.168.8.129";
//    int cre = sendto(sfd, &buff, sizeof(buff), 0, (struct sockaddr*)&ser_addr, sizeof(ser_addr));
//    if (cre < 0)
//    {
//        cout << "send fail" << endl;
//        return -1;
//    }
//    cout << "send successfully!" << endl;
//    close(sfd);
//    return 0;
//}


#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>

bool isReleasePort(unsigned short usPort)
{
    int s = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(usPort);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    int ret = ::bind(s, (sockaddr*)&addr, sizeof(addr));
    if (ret != 0)
    {
        close(s);
        return false;
    }
    close(s);
    return true;
}

bool getReleasePort(short& port)
{
    short num = 0;
    while (!isReleasePort(port) && num<30)
    {
        ++port;
        ++num;
    }
    if (num >= 30)
    {
        port = -1;
        return false;
    }
    return true;
}

int main(int argc, char** argv)
{
    short port = 9060;
    if(getReleasePort(port)) //在port的基础上获取一个可用的port
    {
        std::cout << "可用的端口号为：" << port << std::endl;
    }
    else
    {
        std::cout << "获取可用端口号失败！" << std::endl;
    }
    return 0;
}
