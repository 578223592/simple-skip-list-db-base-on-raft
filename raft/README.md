


关于raftrpc功能的思考：

最好raftrpc类可以实现发送rpc【已经可以实现】，和接受rpc

问题在于接受rpc请求锁provid.run()是一个一直阻塞的函数，

而且为了可以发送rpc，必须所有服务器先开启rpc接受的功能，因此必须先开启rpc的请求能力，再生成stub让其可以发送rpc


为了解决上面的问题，因此rpc接受功能的开启需要卸载raft类中，而不能全部隔离写在raftrpc类中