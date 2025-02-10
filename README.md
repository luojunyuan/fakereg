#### 目的
1. 让依赖于注册表的游戏免注册表直接执行
2. 让会向注册表写入数据的游戏直接写入游戏目录下的 fakereg.ini 文件

`git clone https://github.com/luojunyuan/fakereg --recursive`

实现读写 `REG_DWORD` `REG_BINARY` `REG_SZ` 格式

`RegEnumValueA` 仅实现查key
 
