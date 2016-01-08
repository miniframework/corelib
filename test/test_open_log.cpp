#include "mini_log.h"


int main() {
	
	struct mini_logstat_t logstat; 
	logstat.events = MINI_LOG_ALL;
	logstat.to_syslog =  0;
	logstat.spec = 0;
	if(mini_openlog("./log", "openlog", &logstat, 1024, NULL) != 0) {
		printf("open log error\r\n");
	}
	mini_writelog(MINI_LOG_WARNING,"mini_log");
	mini_closelog(0);
	return 0;

}
