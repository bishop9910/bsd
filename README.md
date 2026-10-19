# BSD Downloader

一个用 C++20 写的多线程 HTTP/HTTPS 下载器，支持断点续传、分片下载、哈希校验、全局限速、代理。

项目拆成两部分：

- **`bsd_core`** —— 下载核心，编译为 `bsd_core.dll`（Windows）/ `libbsd_core.so`（Linux、macOS），只暴露 [`include/bsd/bsd_core.h`](include/bsd/bsd_core.h) 里的 `extern "C"` C API，方便被 GUI 或其它调用方嵌入。
- **`bsd`** —— 命令行客户端，用 CLI11 解析参数，只通过 C API 调用核心，不直接依赖 libcurl / fmt / CLI11 的 C++ 接口。

## 特性

- **多线程分片下载** —— 按线程数与文件大小自适应切片，多个 worker 并发下载
- **核心 / CLI 分离** —— 核心是动态库 + 纯 C 接口，CLI 只是它的一个调用方，可以另写 GUI
- **断点续传** —— 中断后 `-c` 从上次进度继续，靠 `.meta` + ETag/Last-Modified 校验服务器文件是否变化
- **哈希校验** —— 支持 `md5` / `sha1` / `sha256` / `sha512`，校验失败自动删除
- **重试机制** —— 网络类错误由调用方重试，指数退避
- **全局限速** —— `--limit-rate` 限制所有线程的合计速率，而不是每条连接
- **自适应分片** —— 分片数与分片大小由线程数和文件大小决定，尽量让所有 worker 都有活干、减少尾部空等
- **代理** —— 支持 HTTP/HTTPS 代理
- **进度回调** —— 核心每 500ms 回调一次进度，CLI 用它画进度条，GUI 可以直接拿数据
- **静默模式** —— `-s` 关闭所有输出
- **日志文件 / 日志回调** —— 可选写入日志文件，GUI 可注册日志回调
- **自定义请求头** —— `-H` 可重复添加
- **自动文件名** —— 优先 `Content-Disposition`，退回 URL 尾段

## 依赖

- **CMake** >= 3.31
- **C++20** 编译器（MSVC 19.29+ / GCC 10+ / Clang 12+）
- **vcpkg** 管理第三方库

vcpkg 依赖：

- [`libcurl`](https://curl.se/libcurl/) —— HTTP 客户端
- [`OpenSSL`](https://www.openssl.org/) —— 哈希计算
- `fmt` —— 日志格式化
- [`CLI11`](https://github.com/CLIUtils/CLI11) —— 仅 CLI 使用，核心库不依赖它
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

# Windows (MSVC / Visual Studio 2026)
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

- Windows：`build/msvc/src/cli/Release/bsd.exe`、`build/msvc/src/core/Release/bsd_core.dll`
- Linux / macOS：`build/gcc/src/cli/bsd`、`build/gcc/src/core/libbsd_core.so`

构建后 CMake 会把 `bsd_core.dll` 以及它依赖的 vcpkg 运行时 DLL（libcurl、OpenSSL、fmt、zlib）复制到 `bsd.exe` 旁边，所以 `bsd.exe` 可以直接双击 / 命令行运行。

### 3. 只构建 DLL 给 GUI 用

```bash
cmake --build --preset=msvc --target bsd_core
```

GUI 侧只需要：

1. 包含 `include/bsd/bsd_core.h`；
2. 链接 `bsd_core.lib`（MSVC）或 `libbsd_core.so` / `libbsd_core.a`；
3. 运行时把 `bsd_core.dll` 和 libcurl / OpenSSL / fmt 的 DLL 放在一起。

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

# 全局限速 1MB/s（所有线程合计）
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
| `--limit-rate` | 所有线程合计限速，如 `1M` / `100K` / `2G` | `1G`（约等于不限） |
| `--retry` | 失败重试次数 | `3` |
| `--timeout` | 请求超时（秒） | `90` |
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
2. 自适应计算分片：
   - 目标片数 = `threads × 8`；
   - 片大小 = `文件大小 / 目标片数`，再夹到 `[256KB, 16MB]`；
   - 片数上限 4096，最后把文件均分。
   这样大文件有足够多的任务喂满线程池，小文件也不会切得过碎。
3. 每个分片独立发 `Range: bytes=start-end` 请求
4. 每个 worker 用 `seekp(offset)` 写入同一个 `.part` 文件
5. 全部完成 → `rename` `.part` 为最终文件，删 `.meta`

**关键点**：

- 每个 worker 独立打开一个 `ofstream`（`in|out` 模式），避免共享流导致的竞态
- **不用 `app` 模式**，否则 `seekp` 失效
- 所有 worker 结束后才 `rename`，避免文件占用

### 全局限速

所有 worker 共享一个 `RateLimiter`：每收到一段数据，先为这段数据在共享时间轴上预约 `bytes / limit_rate` 秒并等待。因为大家都排在同一条时间轴上，总速率就是 `--limit-rate`，而不是“每线程 × limit_rate”。等待过程是分小段 sleep 的，并且可以被取消打断。

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

**中途崩溃保护**：每下 4MB 自动 flush 数据并保存一次 `.meta`，崩溃后重启能恢复大部分进度。

### 哈希校验

下载完成后计算文件的哈希：

- 与 `--checksum` 提供的值比对
- 不一致 → **删除文件** + 返回不可重试错误
- 一致 → 通过

**注意**：哈希校验失败**不重试**，因为服务器上的文件就是坏的，重试还是同一个。

## 作为库使用（C API）

最简示例（完整定义见 [`include/bsd/bsd_core.h`](include/bsd/bsd_core.h)）：

```c
#include <bsd/bsd_core.h>
#include <stdio.h>
#include <stdint.h>

static void on_progress(void* user, uint64_t done, uint64_t total,
                        double speed, int finished) {
  printf("\r%llu/%llu  %5.1f%%  %.1f MB/s",
         (unsigned long long)done, (unsigned long long)total,
         total ? 100.0 * (double)done / (double)total : 0.0,
         speed / 1048576.0);
  if (finished) printf("\n");
}

int main(void) {
  if (bsd_init() != 0) return 1;

  bsd_options opt = {0};
  opt.struct_size = sizeof(opt);
  opt.dir = "downloads";
  opt.threads = 8;
  opt.timeout_seconds = 90;
  opt.limit_rate_bytes_per_second = 4ULL * 1024 * 1024;  /* 4 MiB/s，全局 */

  bsd_handle* handle = bsd_handle_create(&opt);
  if (!handle) {
    fprintf(stderr, "handle: %s\n", bsd_last_error());
    bsd_cleanup();
    return 1;
  }

  bsd_request req = {0};
  req.struct_size = sizeof(req);
  req.url = "https://example.com/big.iso";

  int rc = bsd_download(handle, &req, on_progress, NULL);
  if (rc != BSD_OK) fprintf(stderr, "download failed: %s\n", bsd_last_error());

  bsd_handle_destroy(handle);
  bsd_cleanup();
  return rc == BSD_OK ? 0 : 1;
}
```

要点：

- `bsd_download` 是阻塞调用；GUI 请在单独的工作线程里调用它。
- `bsd_handle_cancel` 线程安全，可从 UI 线程调用；下载会尽快停下并返回 `BSD_ERROR_CANCELLED`。
- 返回码：`BSD_OK` / `BSD_ERROR`（不可重试）/ `BSD_ERROR_RETRYABLE`（可重试）/ `BSD_ERROR_CANCELLED` / `BSD_ERROR_INVALID_ARG`；本线程的错误文本用 `bsd_last_error()` 取。
- 一个 `bsd_handle` 持有一个线程池；在同一个 handle 上串行调用 `bsd_download` 可以复用线程池。
- 日志默认输出到控制台，可用 `bsd_set_log_file` / `bsd_set_log_callback` 接管；`bsd_set_log_silent(1)` 全部关闭。

## 项目结构

```
bsd-downloader/
├── CMakeLists.txt
├── CMakePresets.json
├── CMakeUserPresets.json
├── include/
│   ├── bsd/bsd_core.h   # 公开 C API（GUI 只需要这个）
│   ├── core.hpp
│   ├── curl.hpp         # libcurl 封装
│   ├── download.hpp     # Downloader 类
│   ├── file_name.hpp    # 文件名解析
│   ├── hash.hpp         # 哈希计算
│   ├── json.hpp         # 轻量 JSON
│   ├── log.hpp          # 日志
│   ├── meta.hpp         # Meta 结构 + 序列化
│   ├── option.hpp       # Options 结构（CLI 解析实现在 src/cli）
│   ├── rate_limiter.hpp # 全局限速
│   ├── thread_pool.hpp  # 线程池
│   └── utils.hpp        # 工具函数
└── src/
    ├── cli/
    │   ├── main.cpp     # 命令行客户端（只调 C API）
    │   ├── option.cpp   # CLI11 参数解析
    │   └── CMakeLists.txt
    ├── core/
    │   ├── c_api.cpp    # extern "C" API 实现
    │   ├── chunk.cpp
    │   ├── curl.cpp
    │   ├── download.cpp
    │   ├── file_name.cpp
    │   ├── hash.cpp
    │   ├── meta.cpp
    │   ├── rate_limiter.cpp
    │   ├── thread_pool.cpp
    │   ├── utils.cpp
    │   └── CMakeLists.txt
    └── res/
        ├── res.rc
        ├── manifest.xml
        └── appIcon.ico
```

## 架构

```
bsd (CLI, CLI11)                     GUI / 其它调用方
      │                                     │
      └──────────────┬──────────────────────┘
                     ▼
            bsd_core.dll（extern "C" C API）
                     │
                Downloader
                 ├─ recover()  续传校验（可选）
                 ├─ prepare()  HEAD + buildMeta + planChunksAdaptive
                 ├─ dispatch() 提交分片任务到 ThreadPool，等完成
                 │   └─ download_chunk()  worker 函数
                 │        ├─ RateLimiter::acquire()  全局限速
                 │        └─ 每 4MB flush + 保存 .meta
                 └─ finalize() rename + 哈希校验
```

**线程模型**：

- 每个 `bsd_handle` 一个 `ThreadPool`（`-t` 控制大小）
- 一个进度线程（核心内部，每 500ms 调用一次进度回调）
- 每个分片一个 worker 任务
- `downloaded_bytes` 用 `std::atomic<uint64_t>`
- `meta_` 用 `std::mutex` 保护

## 已知限制

- **不支持 HTTP/2**：libcurl 默认 HTTP/1.1
- **不支持多个 URL 并发下载**：CLI 内串行，避免抢带宽
- **限速是客户端节流**：按字节数在写盘前等待，瞬时速度会有小幅波动，长期平均等于 `--limit-rate`
- **取消是协作式的**：正在阻塞的网络读要等本次写回调或超时才退出
- **续传沿用旧的分片计划**：只有新建任务才会按当前线程数重新规划分片
- **`ETag` / `Last-Modified` 都缺失时无法续传**：会从头下载
- **不支持 Cookie 文件**：只能用 `-H "Cookie: ..."` 手动传

## 许可

[MIT](LICENSE)

## 致谢

- [CLI11](https://github.com/CLIUtils/CLI11)
- [libcurl](https://curl.se/libcurl/)
- [OpenSSL](https://www.openssl.org/)
- [vcpkg](https://github.com/microsoft/vcpkg)
