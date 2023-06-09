import socket

# 创建 TCP 客户端套接字
client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

# 连接服务器
server_address = ('127.0.1.1', 12938)


client_socket.connect(server_address)


local_address = client_socket.getsockname()[0]
local_port = client_socket.getsockname()[1]
print("Local address:", local_address)
print("Local port:", local_port)
# 发送数据
message = 'hello world3'


client_socket.sendall(message.encode())

# 关闭套接字
client_socket.close()