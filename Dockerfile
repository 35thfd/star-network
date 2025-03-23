FROM ubuntu:latest

#设置时区防止交互式问题
ENV TZ=Asia/Shanghai
RUN apt-get update && apt-get install -y tzdata

RUN apt-get install -y build-essential

WORKDIR /app
COPY . .

# 编译
RUN make

CMD ["/bin/bash"]
