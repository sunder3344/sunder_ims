<div align="right">
    <a href="./README_CN.md">English</a>
</div>

## Sunder ims
一个基于linux的即时聊天系统（包括服务端和客户端），当然功能很简单，需要复杂功能的，可以接着写下去，包括登陆，群聊什么的

## 编译使用

* 编译如下代码:
    *  gcc -o server_epoll server_epoll.c cJSON.c -lpthread -lgdbm
    *  gcc -o clientA clientA.c cJSON.c -lpthread
    
    
* 执行文件
    * ./server_epoll 
    <p align="left">
		<img src="./pic/server.png" width="690" height="230" alt="Server"/>
	</p>
    
    * ./clientA [你要聊天对象的昵称] [你的昵称](./clientA derek sunder)
    <p align="left">
		<img src="./pic/clientA.png" width="400" height="100" alt="client A"/>
	</p>
	
    * ./clientB [你要聊天对象的昵称] [你的昵称](./clientB sunder derek)
    <p align="left">
		<img src="./pic/clientB.png" width="400" height="100" alt="client B"/>
	</p>
	
	
* 请保证昵称的唯一性，不然就会发到别人那边去了。这样客户端A，B之间就可以通过服务器来建立聊天了。


* thanks for the project [cJSON](https://github.com/DaveGamble/cJSON)