#ifndef __MINI_CONF_H_
#define __MINI_CONF_H_

#define WORD_SIZE 2048
#define LINE_SIZE 4096

#define SEND			(char)0 	/**< '\0' */
#define TAB				(char)9		/**< tab  */
#define CR				(char)10	/**< '\n' */
#define LF				(char)13	/**< '\r' */
#define SPACE			(char)32	/**< ' '  */

static const int LIMIT_CONF_ITEM_NUM = 1024;

struct mini_confitem_t {
	char name[WORD_SIZE];
	char value[WORD_SIZE];
};
struct mini_confdata_t {
	int num;
	int size;
	mini_confitem_t *item;
};
/**
 * 初始化配置文件结构
 *
 * @param conf_num 配置条目数，如果小于1024，则自动使用1024
 * @return NULL失败，否则为配置文件结构指针
 */
extern mini_confdata_t *mini_initconf(int num);
extern int mini_freeconf(mini_confdata_t * pd_conf);
extern int mini_readconf(const char *work_path, const char *fname, mini_confdata_t * pd_conf) ;
extern int mini_getconfstr(mini_confdata_t * pd_conf, const char *c_name, char *c_value);

#endif
