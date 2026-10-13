# BSD Downloader

一个用 C++20 写的多线程 HTTP/HTTPS 下载器，支持断点续传、分片下载、哈希校验、限速、代理。

## 特性

- **多线程分片下载** —— 按 `chunk_size` 切片，多个 worker 并发下载
- **断点续传** —— 中断后 `-c` 从上次进度继续，靠 `.meta` + ETag/Last-Modified 校验服务器文件是否变化
- **哈希校验** —— 支持 `md5` / `sha1` / `sha256` / `sha512`，校验失败自动删除重下
- **重试机制** —— 网络类错误自动重试，指数退避
- **限速** —— 每个线程的下载速率可限制
- **代理** —— 支持 HTTP/HTTPS 代理
- **进度条** —— 实时显示百分比、速度、ETA
- **静默模式** —— `-s` 关闭所有输出
- **日志文件** —— 可选写入日志文件
- **自定义请求头** —— `-H` 可重复添加
- **自动文件名** —— 优先 `Content-Disposition`，退回 URL 尾段

## 依赖

- **CMake** >= 3.21
- **C++20** 编译器（MSVC 19.29+ / GCC 10+ / Clang 12+）
- **vcpkg** 管理第三方库

vcpkg 依赖：

- [`CLI11`](https://github.com/CLIUtils/CLI11) —— 命令行解析
- [`libcurl`](https://curl.se/libcurl/) —— HTTP 客户端
- [`OpenSSL`](https://www.openssl.org/) —— 哈希计算
- `zlib` —— 间接依赖

## 构建

### 1. 设置 vcpkg

```bash
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh       # Linux / macOS
.\bootstrap-vcpkg.bat      # Windows
```

设置环境变量：

```bash
export VCPKG_ROOT=/path/to/vcpkg    # Linux / macOS
setx VCPKG_ROOT D:\path\to\vcpkg    # Windows
```

### 2. 克隆并构建

```bash
git clone <repo-url> bsd-downloader
cd bsd-downloader

# Windows (MSVC)
cmake --preset=msvc
cmake --build --preset=msvc

# Linux / macOS (GCC)
cmake --preset=gcc
cmake --build --preset=gcc

# Linux / macOS (Clang)
cmake --preset=clang
cmake --build --preset=clang
```

产物：

- Windows: `build/msvc/src/Release/bsd.exe`
- Linux / macOS: `build/gcc/src/bsd`

## 使用

### 基本用法

```bash
bsd <url> [options]
```

### 示例

```bash
# 下载到当前目录，自动推断文件名
bsd https://example.com/video.mp4

# 指定输出文件名
bsd https://example.com/video.mp4 -o video.mp4

# 下载到指定目录
bsd https://example.com/video.mp4 -d ./downloads

# 16 线程下载
bsd https://example.com/video.mp4 -t 16

# 断点续传
bsd https://example.com/video.mp4 -c

# 限速 1MB/s（每线程）
bsd https://example.com/video.mp4 --limit-rate 1M

# 校验 sha256
bsd https://example.com/file.iso \
  --checksum sha256:e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855

# 带自定义请求头
bsd https://example.com/file -H "Cookie: a=1" -H "Referer: https://example.com"

# 走代理
bsd https://example.com/file --proxy http://127.0.0.1:8080

# 静默模式 + 日志文件
bsd https://example.com/file -s --log-file download.log

# 多文件下载
bsd https://example.com/a.zip https://example.com/b.zip -o a.zip b.zip

# 多文件 + 各自校验
bsd https://example.com/a https://example.com/b \
  --checksum sha256:aaa sha256:bbb
```

## 命令行选项

| 选项 | 说明 | 默认值 |
|---|---|---|
| `url` | 下载 URL（可多个，位置参数） | 必填 |
| `-o, --output` | 输出文件名（可多个） | 自动推断 |
| `-d, --dir` | 下载目录 | `.` |
| `-t, --threads` | 每个文件的并发线程数 | `16` |
| `-c, --continue` | 断点续传 | `false` |
| `--limit-rate` | 每线程限速，如 `1M` / `100K` / `2G` | 不限 |
| `--retry` | 失败重试次数 | `3` |
| `--timeout` | 请求超时（秒） | `30` |
| `--proxy` | 代理 URL | 无 |
| `-H, --header` | 自定义请求头，可重复 | 无 |
| `--user-agent` | User-Agent | `bsd-downloader/1.0` |
| `--checksum` | 校验格式 `type:hash`，可重复 | 无 |
| `-s, --silent` | 静默模式 | `false` |
| `--log-file` | 日志文件路径 | 无 |
| `-h, --help` | 显示帮助 | — |
| `-v, --version` | 显示版本 | — |

## 工作原理

### 分片下载

1. 发 `HEAD` 请求拿响应头，解析 `Content-Length`、`ETag`、`Last-Modified`、`Accept-Ranges`
2. 按 `chunk_size`（默认 10MB，大文件 64MB）切片
3. 每个 chunk 独立发 `Range: bytes=start-end` 请求
4. 每个 worker 用 `seekp(offset)` 写入同一个 `.part` 文件
5. 全部完成 → `rename` `.part` 为最终文件，删 `.meta`

**关键点**：

- 每个 worker 独立打开一个 `ofstream`（`in|out` 模式），避免共享流导致的竞态
- **不用 `app` 模式**，否则 `seekp` 失效
- 所有 worker 结束后才 `rename`，避免文件占用

### 断点续传

下载中会生成两个文件：

- `{name}.part` —— 数据文件
- `{name}.meta` —— JSON 元数据，记录 URL、ETag、总大小、每个 chunk 的 `done`

续传时：

1. 读 `.meta`，发 `HEAD` 校验：
   - URL 一致
   - 总大小一致
   - ETag 或 Last-Modified 至少一个能对上
2. 校验通过 → 从每个 chunk 的 `begin + done` 继续
3. 校验失败 → 丢弃 `.part` 和 `.meta`，从头下载

**中途崩溃保护**：每下 8MB 自动保存一次 `.meta`，崩溃后重启能恢复大部分进度。

### 哈希校验

下载完成后计算文件的哈希：

- 与 `--checksum` 提供的值比对
- 不一致 → **删除文件** + 抛非重试异常
- 一致 → 通过

**注意**：哈希校验失败**不重试**，因为服务器上的文件就是坏的，重试还是同一个。

## 项目结构

```
bsd-downloader/
├── CMakeLists.txt
├── CMakePresets.json
├── CMakeUserPresets.json
├── include/
│   ├── core.hpp
│   ├── curl.hpp          # libcurl 封装
│   ├── download.hpp      # Downloader 类
│   ├── file_name.hpp     # 文件名解析
│   ├── hash.hpp          # 哈希计算
│   ├── json.hpp          # 轻量 JSON
│   ├── log.hpp           # 日志
│   ├── meta.hpp          # Meta 结构 + 序列化
│   ├── option.hpp        # Options 结构 + 解析
│   ├── thread_pool.hpp   # 线程池
│   └── utils.hpp         # 工具函数
└── src/
    ├── main.cpp
    ├── bsd.rc            # Windows 资源
    └── core/
        ├── curl.cpp
        ├── download.cpp
        ├── file_name.cpp
        ├── hash.cpp
        ├── option.cpp
        ├── thread_pool.cpp
        └── utils.cpp
```

## 架构

```
main
 ├─ parse_options()        解析命令行
 ├─ Curl::global_init()    初始化 libcurl
 └─ for each url
     └─ retry loop
         └─ Downloader
             ├─ recover()  续传校验（可选）
             ├─ prepare()  HEAD + buildMeta + createChunks
             ├─ dispatch() 提交 chunk 任务到 ThreadPool，等完成
             │   └─ download_chunk()  worker 函数
             └─ finalize() rename + 哈希校验
```

**线程模型**：

- 一个全局 `ThreadPool`（`-t` 控制大小）
- 一个进度线程（`progress_loop`，每 500ms 刷一次）
- 每个 chunk 一个 worker 任务
- `downloaded_bytes` 用 `std::atomic<uint64_t>`
- `meta_` 用 `std::mutex` 保护

## 已知限制

- **不支持 HTTP/2**：libcurl 默认 HTTP/1.1
- **不支持多个 URL 并发下载**：默认串行，避免抢带宽
- **`--limit-rate` 是每线程限制**：总速率 ≈ 线程数 × limit_rate
- **`ETag` / `Last-Modified` 都缺失时无法续传**：会从头下载
- **不支持 Cookie 文件**：只能用 `-H "Cookie: ..."` 手动传

## 许可

MIT[LICENSE]

## 致谢

- [CLI11](https://github.com/CLIUtils/CLI11)
- [libcurl](https://curl.se/libcurl/)
- [OpenSSL](https://www.openssl.org/)
- [vcpkg](https://github.com/microsoft/vcpkg)