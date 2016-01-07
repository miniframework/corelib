#ifndef __MINI_LOG_H_
#define __MINI_LOG_H_


#include <sys/time.h>
#include <sys/timeb.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/ipc.h>
#include <sys/wait.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <syslog.h>
#include <signal.h>
#include <pthread.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>


#define ERR_NOSPACE     -1
#define ERR_OPENFILE    -2
#define ERR_EVENT       -3
#define ERR_FILEFULL    -4

#define LOG_FILE_NUM    2048              /**< 最多可同时打开的文件数      */
#define LOG_BUFF_SIZE_EX 2048             /**< 一条日志buff大小       */
#define PATH_SIZE		256

#define MAX_FILE_SIZE       2045          /**< 最大文件大小单位(MB)      */
#define MAX_FILENAME_LEN    1024          /**< 日志文件名的最大长度       */

#define MAX_USER_DEF_LOG 8        /**< 单个线中自定义日志的日志数上限      */
/** @brief 事件类型  */

#define MINI_LOG_WFSTART  -2
#define MINI_LOG_START    -1
#define MINI_LOG_END      MINI_LOG_START
#define MINI_LOG_WFEND    MINI_LOG_WFSTART

#define MINI_LOG_NONE     0
#define MINI_LOG_FATAL    0x01    /**<   fatal errors */
#define MINI_LOG_WARNING  0x02    /**<   exceptional events */
#define MINI_LOG_NOTICE   0x04    /**<   informational notices */
#define MINI_LOG_TRACE    0x08    /**<   program tracing */
#define MINI_LOG_DEBUG    0x10    /**<   full debugging */
#define MINI_LOG_ALL      0xff    /**<   everything     */

#define MINI_LOG_USER_BEGIN   0x100
#define MINI_LOG_USER_END     0x107
#define MINI_LOG_USER_MASK    0xff

/* ul_log_t  log_sepc */
#define MINI_LOGTTY       0x02    /**<   日志在输出到日志文件的同时输出到标准出错(stderr)中 */
#define MINI_LOGNEWFILE   0x08    /**<   创建新的日志文件,可以使每个线程都把日志打到不同文件中*/
#define MINI_LOGSIZESPLIT 0x10    /**<  按大小分割日志文件，不回滚*/
/* ul_file_t  flag */
#define MINI_FILE_TRUNCATE    0x01
#define MINI_FILE_FULL        0x02


#define TIME_SIZE 20		  /**< 时间buff的长度        */

#define STRING_FATAL	"FATAL: "
#define STRING_WARNING	"WARNING: "
#define STRING_NOTICE	"NOTICE: "
#define STRING_TRACE	"TRACE: "
#define STRING_DEBUG	"DEBUG: "


struct mini_logstat_t {
        int events;     /**< 需要打的日志级别 0-15 */
        int to_syslog;  /**< 输出到syslog 中的日志级别, 0-15 */
        int spec;       /**< 扩展开关 0 or @ref MINI_LOGTTY or @ref MINI_LOGNEWFILE */
};


struct mini_file_t {
    FILE *fp;                           /**< 文件句柄 */
    int  flag;                          /**< 标志  @ref MINI_FILE_TRUNCATE | @ref MINI_FILE_FULL */
    int  ref_cnt;                       /**< 引用计数 */
    int  max_size;                      /**< 文件可以记录的最大长度 */
    pthread_mutex_t file_lock;          /**< 写文件锁 */
    char file_name[MAX_FILENAME_LEN+1]; /**< 文件名字 */
};

struct mini_log_t {
    char used;                          /**< 0-未使用  1-已使用 */
    mini_file_t *pf;                      /**< log */
    mini_file_t *pf_wf;                   /**< log.wf */
    pthread_t tid;                      /**< 线程id  实际使用上是使用gettid,而非pthread_self*/
    int  mask;                          /**< 可以记录的事件的掩码 */
    int  log_syslog;                    /**< 输出到系统日志的事件掩码 */
    int  log_spec;                      /**< MINI_LOGTTY | MINI_LOGNEWFILE */
    mini_file_t *spf[MAX_USER_DEF_LOG];   /**< 自定义日志文件句柄 */
};
struct mini_log_user_t {                    /**< 兼容旧模式，定义不加 _t       */
    char name[MAX_USER_DEF_LOG][PATH_SIZE];     /**< 自定义日志文件名，系统自动在文件名后加后缀.sdf */
    char flags[MAX_USER_DEF_LOG];               /**<决定当前自定义的日志是否输出,设置为1则生成自定义日志,0则不生成 */
    int  log_number;                            /**< 自定义文件的数目,当设置为0时,不生成自定义文件 */
}; //自定义日志的设置,可在mini_openlog以及mini_openlog_r中作为参数传入,设置MINI_LOGNEWFILE时,自定义日志与正常日志一样,
             //为线程文件,否则,自定义日志为进程级日志
static int g_file_size;
static char g_log_path[MAX_FILENAME_LEN+1] = "./";
static char g_proc_name[MAX_FILENAME_LEN+1] = "";

static mini_file_t g_file_array[LOG_FILE_NUM];  //文件列表
static pthread_key_t g_log_fd = PTHREAD_KEYS_MAX;
static pthread_once_t g_log_unit_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t g_file_lock = PTHREAD_MUTEX_INITIALIZER;  //文件list全局锁
static mini_logstat_t g_default_logstate = {MINI_LOG_ALL, MINI_LOG_FATAL, MINI_LOGTTY};
static mini_file_t g_file_stderr = {stderr, 0, 1, 0, PTHREAD_MUTEX_INITIALIZER, "/dev/stderr"}; 
static mini_log_t  g_log_stderr = {1, &g_file_stderr, &g_file_stderr, 0, MINI_LOG_ALL, 0, 0, {NULL}};


/**
 * @brief 打开日志文件（log和log.wf）并初始化日志对象(包括attach共享内存)
 *
 * @param [in] log_path : 日志文件所在目录 
 * @param [in] log_procname : 日志文件名前缀。如果文件名中包含'_'，则截断为'_'之前的字符串
 * @param [in] log_state : 日志相关参数(用来设置log的特性)
 * @param [in] max_size : 单个日志文件的最大长度（unit: MB）
 * @param [in] user :  设置自定义log,具体使用方式请参见结构说明
 * @return 0成功，-1失败
 * @note 退出时需要调用mini_closelog释放资源
 * @see mini_closelog mini_openlog_r mini_closelog_r
 */
extern int mini_openlog(const char *log_path, const char *proc_name, mini_logstat_t  *log_state,
        int max_size, mini_log_user_t *user) ;
 
extern int mini_openlog_ex(mini_log_t *log_fd, const char *file_name, int mask, int flag,
        int maxlen, mini_log_user_t * user );
extern void mini_closefile(mini_file_t *file_fd);
extern int mini_writelog(const int event, const char* fmt, ...)  __attribute__ ((format (printf,2,3)));
#endif
