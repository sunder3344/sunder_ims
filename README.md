<div align="right">
    <a href="./README_CN.md">中文</a>
</div>

## Sunder ims
an IMS module in Linux(based on epoll), it's combined with server module and client module(such as Wechat, QQ etc)

## how to use it

* compile the C code:
    *  gcc -o server_epoll server_epoll.c cJSON.c -lpthread -lgdbm
    *  gcc -o clientA clientA.c cJSON.c -lpthread
    
    
* run the exec file
    * ./server_epoll 
    <p align="left">
		<img src="./pic/server.png" width="690" height="230" alt="Server"/>
	</p>
    
    * ./clientA [your friend nickname] [your nickname](./clientA derek sunder)
    <p align="left">
		<img src="./pic/clientA.png" width="400" height="100" alt="client A"/>
	</p>
	
    * ./clientB [[your friend nickname] [your nickname](./clientB sunder derek)
    <p align="left">
		<img src="./pic/clientB.png" width="400" height="100" alt="client B"/>
	</p>
	
	
* now clientA can chat with clientB through the server


* thanks for the project [cJSON](https://github.com/DaveGamble/cJSON)