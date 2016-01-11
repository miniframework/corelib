#include "mini_log.h"
#include "mini_conf.h"


int main() {
	struct mini_logstat_t logstat; 

	logstat.events = MINI_LOG_ALL;
	logstat.to_syslog =  0;
	logstat.spec = 0;
	if(mini_openlog("./log", "openconf", &logstat, 1024, NULL) != 0) {
		printf("open log error\r\n");
		return -1;
	}
	
	struct mini_confdata_t *confdata;
	confdata = mini_initconf(1);	
	if(confdata == NULL ) {
		mini_writelog(MINI_LOG_WARNING, "mini_initconf(1) error)");
		return -1;
	}
	mini_readconf("./conf", "testconf.conf", confdata);
	char value1[128];
	char value2[128];
	mini_getconfstr(confdata, "key1", value1);
	mini_getconfstr(confdata, "key2", value2);
	printf("key1:%s\n", value1);
	printf("key2:%s\n", value2);
	
	mini_freeconf(confdata);
	mini_closelog(0);
	return 0;

}
