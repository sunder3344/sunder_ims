#Sunder ims
an IMS module in Linux(based on epoll)£¬ it's combined with server module and client module(such as Wechat, QQ etc)

#how to use it

* compile the C code:
    *  gcc -o server_epoll server_epoll.c cJSON.c -lpthread -lgdbm
    *  gcc -o clientA clientA.c cJSON.c -lpthread
* run the exec file
    * ./server_epoll 
    * ./clientA 192.168.0.1 sunder
    * ./clientB 192.168.0.2 derek
* now clientA can chat with clientB through the server