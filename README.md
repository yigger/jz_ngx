### JZ_NGX
> 洁账服务端的自定义 Nginx 模块，该模块的功能是对请求图片进行鉴权后返回

#### 依赖
+ Nginx 1.14.1  
+ CURL

### 本模块包含
+ hello_world_module (学习 Nginx 开发的一个小 Demo)
+ private_image_module (私有图片的鉴权模块)

### 程序逻辑描述
1. nginx 获取请求的 header 加密串
2. 利用该加密串作为参数请求服务端，如： curl xxxx
3. 获取返回值，从而确定加密串是否有效，返回对应用户的 ID
4. 利用 ID 和请求 URI 组成磁盘路径
5. 获取图片资源文件并返回
6. 完成响应

### 编译安装
```
./configure --prefix=install_path --add-module=jz_ngx/private_image  --with-ld-opt="-l curl"

make && make install
```

### 使用方法
```
location ~ ^/private/(.*)\.(jpg|jpeg|png|gif)$ {
  root /var/html/example_project; # 项目路径
  private_image;
}
```

### 使用 GDB 进行调试

1. 编译的时候务必带上 --with-debug
2. 修改 nginx.conf，加上以下配置项
```
...
daemon off; # 关闭守护进程
master_process off; # 只运行主进程来服务
...
```

3. 进入到 nginx 主程序目录
```
cd /usr/nginx/sbin

sudo gdb nginx

进入后打断点调试，详情参考使用 GDB 进行调试
```
