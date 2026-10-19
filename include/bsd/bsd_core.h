/*
 * bsd_core public C API.
 *
 * This header is the only header a library consumer (e.g. a GUI) needs.
 * The whole downloader lives in bsd_core.dll / libbsd_core.so; the CLI in
 * src/cli is just one client of this API.
 *
 * All strings are UTF-8, NUL terminated, and owned by the caller unless
 * stated otherwise. Returned const char* pointers stay valid until the next
 * call on the same thread (bsd_last_error) or forever (bsd_version).
 *
 * Typical use:
 *
 *   bsd_init();
 *   bsd_options o = {0};
 *   o.struct_size = sizeof(o);
 *   o.threads = 8;
 *   bsd_handle* h = bsd_handle_create(&o);
 *   bsd_request r = {0};
 *   r.struct_size = sizeof(r);
 *   r.url = "https://example.com/file.bin";
 *   bsd_download(h, &r, progress_cb, user);
 *   bsd_handle_destroy(h);
 *   bsd_cleanup();
 *
 * bsd_download blocks until the file is finished; call it from a worker
 * thread in a GUI. bsd_handle_cancel() may be called from any thread and
 * makes the running download stop as soon as possible.
 */
#ifndef BSD_BSD_CORE_H
#define BSD_BSD_CORE_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#  if defined(BSD_CORE_BUILD)
#    define BSD_API __declspec(dllexport)
#  else
#    define BSD_API __declspec(dllimport)
#  endif
#else
#  define BSD_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define BSD_CORE_API_VERSION 1

typedef enum bsd_status {
  BSD_OK = 0,
  BSD_ERROR = 1,            /* non retryable error                        */
  BSD_ERROR_RETRYABLE = 2,  /* transient error, the caller may retry      */
  BSD_ERROR_CANCELLED = 3,  /* bsd_handle_cancel() was requested          */
  BSD_ERROR_INVALID_ARG = 4 /* bad handle / request / options             */
} bsd_status;

typedef enum bsd_log_level {
  BSD_LOG_TRACE = 0,
  BSD_LOG_DEBUG = 1,
  BSD_LOG_INFO = 2,
  BSD_LOG_WARN = 3,
  BSD_LOG_ERROR = 4,
  BSD_LOG_OFF = 5
} bsd_log_level;

/* Download session options. Zero the struct, then set struct_size. */
typedef struct bsd_options {
  size_t struct_size;                     /* sizeof(bsd_options)             */
  const char* dir;                        /* download directory, "." default */
  int threads;                            /* worker threads, default 16      */
  int retry;                              /* retries, used by clients        */
  int timeout_seconds;                    /* per request timeout, default 90 */
  int continue_download;                  /* resume from .meta/.part         */
  uint64_t limit_rate_bytes_per_second;   /* global limit, 0 = unlimited     */
  const char* proxy;                      /* may be NULL                     */
  const char* user_agent;                 /* may be NULL                     */
  const char* const* headers;             /* may be NULL, UTF-8 "K: V" lines */
  size_t header_count;
} bsd_options;

/* One download request. Zero the struct, then set struct_size. */
typedef struct bsd_request {
  size_t struct_size;                     /* sizeof(bsd_request)             */
  const char* url;                        /* required                        */
  const char* output;                     /* NULL = take name from server    */
  const char* checksum;                   /* NULL = no hash check            */
} bsd_request;

/* Called from the internal progress thread roughly every 500 ms and once
 * with finished != 0 after a successful download. */
typedef void (*bsd_progress_fn)(void* user,
                                uint64_t downloaded,
                                uint64_t total,
                                double bytes_per_second,
                                int finished);

/* Called for every log line that is not silenced. */
typedef void (*bsd_log_fn)(void* user, int level, const char* message);

typedef struct bsd_handle bsd_handle;

/* Version string of the core library (includes build info). */
BSD_API const char* bsd_version(void);

/* Initialize / release the core. Reference counted; bsd_handle_create and
 * bsd_handle_destroy also retain / release it, so calling these is
 * optional but recommended. bsd_init returns 0 on success. */
BSD_API int  bsd_init(void);
BSD_API void bsd_cleanup(void);

/* Create a download session. Returns NULL on failure (see bsd_last_error). */
BSD_API bsd_handle* bsd_handle_create(const bsd_options* options);

/* Destroy a session. No download may be running on it at this point. */
BSD_API void bsd_handle_destroy(bsd_handle* handle);

/* Ask the download currently running on this handle to stop. Thread safe. */
BSD_API void bsd_handle_cancel(bsd_handle* handle);

/* Run one download (blocking). Returns a bsd_status value. */
BSD_API int bsd_download(bsd_handle* handle,
                         const bsd_request* request,
                         bsd_progress_fn progress,
                         void* progress_user);

/* Human readable message for the last failing call on this thread. */
BSD_API const char* bsd_last_error(void);

/* Logging configuration (process wide, shared by all handles). */
BSD_API void bsd_set_log_silent(int silent);
BSD_API void bsd_set_log_console(int enabled);
BSD_API void bsd_set_log_level(int level);          /* bsd_log_level        */
BSD_API void bsd_set_log_file(const char* path);    /* NULL / "" = no file  */
BSD_API void bsd_set_log_callback(bsd_log_fn callback, void* user);
/* Write one message through the core logger (handy for client code). */
BSD_API void bsd_log_message(int level, const char* message);

#ifdef __cplusplus
}
#endif

#endif /* BSD_BSD_CORE_H */
