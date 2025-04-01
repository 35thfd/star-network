cat > ~/.docker/config.json <<EOF
{
  "auths": {},
  "HttpHeaders": {
    "User-Agent": "Docker-Client/20.10.23 (darwin)"
  },
  "credsStore": "desktop",
  "experimental": "enabled",
  "proxies": {},
  "registry-mirrors": [
    "https://registry.docker-cn.com",
    "https://docker.mirrors.ustc.edu.cn",
    "https://hub-mirror.c.163.com",
    "https://mirror.baidubce.com"
  ]
}
EOF