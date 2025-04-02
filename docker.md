**(需要先编译出satellite和sta_main的可执行文件)**

1. 在项目目录下创建Dockerfile.satellite文件  添加内容
   
   ```dockerfile
   FROM ubuntu:latest
   WORKDIR /app
   
   COPY satellite /app/satellite
   
   RUN chmod +x satellite
   
   CMD ["./satellite"]
   ```

2. 创建Dockerfile.sta_main 添加内容:

```dockerfile
FROM ubuntu:latest  

WORKDIR /app

COPY sta_main /app/sta_main

RUN chmod +x sta_main  

CMD ["./sta_main"]
```

3. 运行以下指令构建docker镜像：
   
   ```bash
   docker build -t satellite-image -f Dockerfile.satellite .
   docker build -t sta-main-image -f Dockerfile.sta_main .
   ```

4. 使用 **Host 网络模式**运行容器
   
   ```bash
   docker run --rm --net=host --name satellite satellite-image
   docker run --rm --net=host --name sta-main sta-main-image
   ```

5. 查看终端输出(检查运行结果)
   
   ```bash
   docker logs -f sta_main
   docker logs -f satellite
   ```