#include "mini_log.h"



static char *mini_ctime(char *t_ime, size_t t_ime_size)
{
	time_t tt;
	struct tm vtm;

	time(&tt);
	localtime_r(&tt, &vtm);
	snprintf(t_ime, t_ime_size, "%02d-%02d %02d:%02d:%02d",
			vtm.tm_mon+1, vtm.tm_mday, vtm.tm_hour, vtm.tm_min, vtm.tm_sec);
	return t_ime;
}


static void log_fd_init()
{
    pthread_key_create(&g_log_fd, NULL);
}

//启动全局线程
static int  mini_threadlog_sup()
{
    pthread_once(&g_log_unit_once, log_fd_init);
    return 0;
} 

/**
 * @brief 打开自定义日志
 *
 * @param [in] user   自定义日志类型
 * @return  日志打开成功与否
 * @retval  -1 自定义日志打开失败，0自定义日志打开成功
**/
static int mini_check_userlog(const mini_log_user_t *user)
{
    //check user define log
    if (user != NULL) {
        if (user->log_number > MAX_USER_DEF_LOG || user->log_number < 0) {
            fprintf(stderr,"in mini_log.cpp :user define log_number error!log_number = %d\n",user->log_number);
            fprintf(stderr,"in mini_log.cpp:open error!\n");
            return -1;
        }
        for (int i =0 ;i < user->log_number; i++) {
            if (strlen(user->name[i]) == 0 && user->flags[i] != 0) {
                fprintf(stderr,"in mini_log.cpp :user define log error![%d]\n",i);
                fprintf(stderr,"in mini_log.cpp:open error!\n");
                return -1;
            }
        }
    }
    return 0;
}

static mini_log_t *mini_alloc_log_unit()
{
    mini_log_t   *log_fd;

    log_fd = (mini_log_t *)calloc(1, sizeof(mini_log_t));
    if (NULL == log_fd)
        return NULL;
    log_fd->tid = pthread_self();
   // log_fd->tid = gettid();

    if (pthread_setspecific(g_log_fd, log_fd) != 0) {
        free(log_fd);
        return NULL;
    }


    return log_fd;
}

static mini_log_t *mini_get_log_unit()
{
    return (mini_log_t *)pthread_getspecific(g_log_fd);
}

static void mini_free_log_unit()
{
    mini_log_t *log_fd;

    log_fd = mini_get_log_unit();
    if (log_fd != NULL) {
        pthread_setspecific(g_log_fd, NULL);
        free(log_fd);
    }
}
static FILE *mini_open_file(const char *name, char *mode)
{
    size_t path_len;
    FILE   *fp;
    char   *path_end;
    char   path[MAX_FILENAME_LEN+1];

    fp = fopen(name, mode);
    if (fp != NULL)
        return fp;

    path_end = strrchr(name, '/');
    if (path_end != NULL) {
        path_len = (path_end>name+MAX_FILENAME_LEN) ? MAX_FILENAME_LEN : (size_t)(path_end-name);
    } else {
        path_len = strlen(name)>MAX_FILENAME_LEN ? MAX_FILENAME_LEN : strlen(name);
    }
    strncpy(path, name, path_len);
    path[path_len] = '\0';

    mkdir(path, 0700);

    return fopen(name, mode);
}
static mini_file_t *mini_open_file_unit(const char *file_name, int flag, int max_size)
{
    int i;
    mini_file_t *file_free = NULL;

    pthread_mutex_lock(&g_file_lock);
    //找到打开的文件，比较file_name 是否一样，返回 
    for (i=0; i<LOG_FILE_NUM; i++) {
        if (NULL == g_file_array[i].fp) {
            if (NULL == file_free) {
                file_free = &g_file_array[i];
            }
            continue;   
        }
        if (!strcmp(g_file_array[i].file_name, file_name)) {
            g_file_array[i].ref_cnt++;
            pthread_mutex_unlock(&g_file_lock);
            return &g_file_array[i];
        }

    }
    //打开开文件rw
    if (file_free != NULL) {
        file_free->fp = mini_open_file(file_name, "a");
        if (NULL == file_free->fp) {
            pthread_mutex_unlock(&g_file_lock);
            return (mini_file_t *)ERR_OPENFILE;
        }
        pthread_mutex_init(&(file_free->file_lock), NULL);
        file_free->flag = flag;
        file_free->ref_cnt = 1;
        file_free->max_size = max_size;
        snprintf(file_free->file_name, MAX_FILENAME_LEN, "%s", file_name);
    }

    pthread_mutex_unlock(&g_file_lock);

    return file_free;
}

void mini_closefile(mini_file_t *file_fd)
{
    pthread_mutex_lock(&g_file_lock);
    file_fd->ref_cnt--;

    if(file_fd->ref_cnt <= 0) {
        if (file_fd->fp != NULL) {
            fclose(file_fd->fp);
        }
        memset(file_fd, 0, sizeof(mini_file_t));
    }
    pthread_mutex_unlock(&g_file_lock);
}
/**
 * @brief 将生成的buff输到实际的日志中
 *
 * @param [in] file_fd   : 文件句柄
 * @param [in] buff   : 要写的buff
 * @return  是否成功 
 * @retval  -1 失败， 0 成功 
**/
static int mini_vwritelog_buff(mini_file_t * file_fd, const char *buff, const int split_file)
{
	int check_flag = 1;	
	char tmp_filename[PATH_SIZE];		  /**< 临时文件名，日志文件回滚使用       */
	
	if (NULL == file_fd) {
		return -1;
	}
	
	pthread_mutex_lock(&(file_fd->file_lock));
	//check_flag = mini_check(file_fd, tmp_filename, sizeof(tmp_filename),
//			split_file);

	if (check_flag >= 0) {
		fprintf(file_fd->fp, "%s\n", buff);
		fflush(file_fd->fp);
	}
	pthread_mutex_unlock(&(file_fd->file_lock));


	if (1 == check_flag && 0 == split_file) {
		remove(tmp_filename);
	}

	return 0;
}
static int mini_vwritelog_ex(mini_log_t *log_fd, int event, const char *fmt, va_list args)
{
	size_t  bpos = 0;
	char buff[LOG_BUFF_SIZE_EX];
	char now[TIME_SIZE];
	mini_file_t *file_fd;
	buff[0] = '\0';
	file_fd = log_fd->pf;
	if (log_fd->mask < event) {
		return ERR_EVENT;
	}
	switch (event) {
		case MINI_LOG_START:
			break;

		case MINI_LOG_WFSTART:
			file_fd = log_fd->pf_wf;
			break;

		case MINI_LOG_NONE:
			break;
		case MINI_LOG_FATAL:
			memcpy(&buff[bpos], STRING_FATAL, sizeof(STRING_FATAL));
			file_fd = log_fd->pf_wf;
			break;

		case MINI_LOG_WARNING:
			memcpy(&buff[bpos], STRING_WARNING, sizeof(STRING_WARNING));
			file_fd = log_fd->pf_wf;
			break;

		case MINI_LOG_NOTICE:
			memcpy(&buff[bpos], STRING_NOTICE, sizeof(STRING_NOTICE));
			break;

		case MINI_LOG_TRACE:
			memcpy(&buff[bpos], STRING_TRACE, sizeof(STRING_TRACE));
			break;

		case MINI_LOG_DEBUG:
			memcpy(&buff[bpos], STRING_DEBUG, sizeof(STRING_DEBUG));
			break;

		default:
			break;
	}

	if (file_fd->flag&MINI_FILE_FULL) {
		return ERR_FILEFULL;
	}
	
	mini_ctime(now, sizeof(now));
	bpos += strlen(&buff[bpos]);
	bpos += snprintf(&buff[bpos], LOG_BUFF_SIZE_EX-bpos, "%s:  %s * %lu ",
			now, g_proc_name, (unsigned long)(log_fd->tid?log_fd->tid:pthread_self()));
	
	vsnprintf(&buff[bpos], LOG_BUFF_SIZE_EX-bpos, fmt, args);

	if (log_fd->log_syslog&event) {
		int	priority;

		switch (event) {
			case MINI_LOG_FATAL:
				priority = LOG_ERR;
				break; 

			case MINI_LOG_WARNING:
				priority = LOG_WARNING;
				break; 

			case MINI_LOG_NOTICE:
				priority = LOG_INFO;
				break;

			case MINI_LOG_TRACE:
			case MINI_LOG_DEBUG:
				priority = LOG_DEBUG;
				break;

			default:
				priority = LOG_NOTICE;
				break;
		}

		(void) syslog (priority, "%s\n", buff);
	}	
	
	return mini_vwritelog_buff(file_fd, buff, (log_fd->log_spec&MINI_LOGSIZESPLIT));	
}

static int mini_writelog_ex(mini_log_t *log_fd, int event, const char *fmt, ...)
{
	int ret = 0;
	va_list args;

	va_start(args, fmt);
	ret = mini_vwritelog_ex(log_fd, event, fmt, args);
	va_end(args);
	return ret; 
}



int mini_openlog(const char *log_path, const char *proc_name, mini_logstat_t  *log_state,
        int max_size, mini_log_user_t *user) 
{
    char *end;
    char file_name[MAX_FILENAME_LEN+1];
    mini_log_t *log_fd;

    mini_threadlog_sup();

    if (mini_check_userlog(user) == -1) {
        return -1;
    }
    
    if (NULL == log_state) {
        log_state = &g_default_logstate;
    }
    if (max_size <= 0 || max_size >= MAX_FILE_SIZE) {
        //最大2G
        g_file_size = (MAX_FILE_SIZE<<20);
    } else {
        g_file_size = (max_size<<20);
    }

    if (NULL == log_path || '\0' == log_path[0]) {
        //当前文件
        g_log_path[0] = '.';
        g_log_path[1] = '\0';
    } else {
        strncpy(g_log_path, log_path, MAX_FILENAME_LEN);
        g_log_path[MAX_FILENAME_LEN] = '\0';
    }
    if (NULL == proc_name || '\0' == proc_name[0]) {
        //如果NULL 程序名为null
        snprintf(g_proc_name, sizeof(g_proc_name), "%s", "null");
    } else {
        strncpy(g_proc_name, proc_name, MAX_FILENAME_LEN);
        g_proc_name[MAX_FILENAME_LEN] = '\0';
        //程序名中不能出现_
        end = strchr(g_proc_name, '_');
        if(end != NULL) {
            *end = '\0';
        }
    }

    snprintf(file_name, MAX_FILENAME_LEN, "%s/%s", g_log_path, g_proc_name);
    
    log_fd = mini_alloc_log_unit();
    if (NULL == log_fd) {
        fprintf(stderr,"in mini_log.cpp :no space!\n");
        fprintf(stderr,"in mini_log.cpp:open error!\n");
        return -1;
    } 
    if (mini_openlog_ex(log_fd, file_name, log_state->events, MINI_FILE_TRUNCATE, g_file_size,user) != 0) {
        //已经打不开日志文件了，直接输出到终端
        //if(log_state->spec&MINI_LOGTTY)
        fprintf(stderr, "in mini_log.cpp:Can't open log file : %slog, exit!\n", proc_name);
        mini_free_log_unit();
        return -1;
    }         
    //写入log_fd参数
    log_fd->log_spec = log_state->spec;
    log_fd->log_syslog = log_state->to_syslog;
     
    //输出文件成功打开
    if (log_state->spec & MINI_LOGTTY) {
        fprintf(stdout, "Open log file %slog success!\n", proc_name);
    }
    mini_writelog_ex(log_fd, MINI_LOG_START, "* Open process log by----%s\n============="
		    "====================================", proc_name);
    mini_writelog_ex(log_fd, MINI_LOG_WFSTART, "* Open process log by----%s for wf\n======"
		    "==================================================", proc_name);
    
     
    return 0;
}

int mini_openlog_ex(mini_log_t *log_fd, const char *file_name, int mask, int flag,
        int maxlen, mini_log_user_t * user = NULL) 
{
    char tmp_name[MAX_FILENAME_LEN]; 
    log_fd->mask = mask;
    g_log_stderr.mask = mask;
    
    //打开日志
    snprintf(tmp_name, MAX_FILENAME_LEN, "%s.log", file_name);
    log_fd->pf = mini_open_file_unit(tmp_name, flag, maxlen);
    if (NULL == log_fd->pf) {
        return ERR_NOSPACE;
    } else if ((mini_file_t *)ERR_OPENFILE == log_fd->pf) {
        return ERR_OPENFILE;
    } 
    //打开wf日志 
    snprintf(tmp_name, MAX_FILENAME_LEN, "%s.log.wf", file_name);
    log_fd->pf_wf = mini_open_file_unit(tmp_name, flag, maxlen);
    
    if (NULL == log_fd->pf_wf) {
        mini_closefile(log_fd->pf);
        return ERR_NOSPACE;
    } else if ((mini_file_t *)ERR_OPENFILE == log_fd->pf_wf) {
        mini_closefile(log_fd->pf);
        return ERR_OPENFILE;
    } 
    //用户自定义日志 
    if (user != NULL) {
        for (int i = 0;i < user->log_number; i++) {
            if (strlen(user->name[i])!=0 && user->flags[i]) {
                snprintf(tmp_name,MAX_FILENAME_LEN, "%s%s.sdf.log", file_name,user->name[i]);
                log_fd->spf[i] = mini_open_file_unit(tmp_name,flag,maxlen);
                if (NULL == log_fd->spf[i]  || (mini_file_t *)ERR_OPENFILE == log_fd->spf[i]) {
                    //如果有一个失败,依次关闭已经打开的
                    for (int j = i - 1;j >= 0;j--) {
                        if (user->flags[j]) {
                            mini_closefile(log_fd->spf[j]);
                        }
                    }
                    mini_closefile(log_fd->pf);
                    mini_closefile(log_fd->pf_wf);
                    return (NULL == log_fd->spf[i])?ERR_NOSPACE:ERR_OPENFILE;
                }
            }
        }
    }
    return 0; 
     
}
static int mini_vwritelog_ex_user(mini_log_t *log_fd, int event, const char *fmt, va_list args)
{
	size_t  bpos = 0;
	char buff[LOG_BUFF_SIZE_EX];
	char now[TIME_SIZE];
	mini_file_t *file_fd;

	file_fd = log_fd->spf[event];

	if (file_fd->flag & MINI_FILE_FULL) {
		return ERR_FILEFULL;
	}

	mini_ctime(now, sizeof(now));
	bpos += snprintf(&buff[bpos], LOG_BUFF_SIZE_EX-bpos, "SDF_LOG: LEVEL:%d %s: %s * %lu",
			event, now, g_proc_name, (unsigned long)(log_fd->tid?log_fd->tid:pthread_self()));
	vsnprintf(&buff[bpos], LOG_BUFF_SIZE_EX-bpos, fmt, args);

	return mini_vwritelog_buff(file_fd, buff, (log_fd->log_spec&MINI_LOGSIZESPLIT));
}
int mini_writelog(const int event, const char* fmt, ...)
{
	int ret;
	va_list args;
	va_start(args, fmt);


	mini_log_t *log_fd;

	log_fd = mini_get_log_unit();
	if(log_fd == NULL) {
		log_fd = &g_log_stderr;
	}

	int user_log_id = event&MINI_LOG_USER_MASK;
	if (event >= MINI_LOG_USER_BEGIN && event <= MINI_LOG_USER_END && log_fd->spf[user_log_id] != NULL) {
		//写自定义log
		ret = mini_vwritelog_ex_user(log_fd, user_log_id, fmt, args);
	} else if (log_fd->mask < event) {
		ret = ERR_EVENT;
	} else {
		ret = mini_vwritelog_ex(log_fd, event, fmt, args);
	}
	va_end(args);
	
	if (log_fd->log_spec & MINI_LOGTTY) {
		va_start(args,fmt);
		ret = mini_vwritelog_ex(&g_log_stderr, event, fmt, args);
		va_end(args);
	}
	return ret;
}
static int mini_closelog_fd(mini_log_t *log_fd)
{

	if (NULL == log_fd) {
		return -1;
	}

	if (log_fd->pf != NULL) {
		mini_closefile(log_fd->pf);
	}
	if (log_fd->pf_wf != NULL) {
		mini_closefile(log_fd->pf_wf);
	}
	for (int i = 0; i < MAX_USER_DEF_LOG; i++) {
		if(log_fd->spf[i] != NULL) {
			mini_closefile(log_fd->spf[i]);
		}
	}

	return 1;
}
static int mini_closelog_ex(int iserr, const char * close_info)
{
	mini_log_t  *log_fd;

	log_fd = mini_get_log_unit();
	
	if (NULL == log_fd) {
		return -1;
	}


	if (iserr) {
		mini_writelog_ex(log_fd, MINI_LOG_END, 
				"< ! > Abnormally end %s\n================================================",
				close_info);
		mini_writelog_ex(log_fd, MINI_LOG_WFEND, 
				"< ! > Abnormally end %s\n================================================",
				close_info);
	} else {
		mini_writelog_ex(log_fd, MINI_LOG_END, 
				"< - > Normally end %s\n================================================",
				close_info);
		mini_writelog_ex(log_fd, MINI_LOG_WFEND, 
				"< - > Normally end %s\n================================================",
				close_info);
	}

	mini_closelog_fd(log_fd);

	if (log_fd->log_spec & MINI_LOGTTY) {
		fprintf(stderr,"Close log successed.\n");
	}
	mini_free_log_unit();


	return 0;
}

int mini_closelog(int iserr)
{
	int ret = mini_closelog_ex(iserr, "process");
	return ret;
}
