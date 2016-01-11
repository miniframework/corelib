#include "mini_log.h"
#include "mini_conf.h"




mini_confdata_t *mini_initconf(int num) {
    mini_confdata_t *confdata = NULL;
    confdata = (mini_confdata_t *) calloc(1, sizeof(mini_confdata_t));
    if (confdata == NULL) {
        mini_writelog(MINI_LOG_WARNING, "calloc(%zu,%zu) call failed.error[%d] info is '%s'.",
			    (size_t)1, sizeof(mini_confdata_t), errno, strerror(errno));
        return NULL;
    }
    confdata->size = num;
    if (confdata->size < LIMIT_CONF_ITEM_NUM) {
        confdata->size = LIMIT_CONF_ITEM_NUM;
    }

    confdata->item = (mini_confitem_t *) calloc((size_t)confdata->size, sizeof(mini_confitem_t));

    if (confdata->item == NULL) {
        free(confdata);
        mini_writelog(MINI_LOG_WARNING, "calloc(%zu,%zu) call failed.error[%d] info is '%s'.",
			    (size_t)confdata->size,sizeof(mini_confitem_t), errno, strerror(errno));
        return NULL;
    }
    return confdata;
}

int mini_freeconf(mini_confdata_t * pd_conf) {
    if (pd_conf == NULL) {
        return 1;
    }
    if (pd_conf->item) {
        free(pd_conf->item);
    }
    free(pd_conf);
    return 1;
}
int readconf_no_dir(const char *fullpath, mini_confdata_t * pd_conf, int start) { 
    struct stat info;
    FILE *pf;
    char inf_str[LINE_SIZE] = "";
    int item_num = start;

    int line_num = 0;
    int ret = -1;
    
     
    if (0 != stat(fullpath, &info) || !S_ISREG(info.st_mode)) {
	    mini_writelog(MINI_LOG_FATAL, "[readconf] file (%s) not found or not linked to a regular file", fullpath);
	    return -1;
    }
    pf = fopen(fullpath, "r");
    if (pf == NULL) {
        mini_writelog(MINI_LOG_FATAL, "[readconf] can't open file (%s)", fullpath);
        return -1;
    }
    char ch = 0;
    int status = 0;
    int sep_sign = 0;
    int i = 0;
    int j = 0;
    int sublen = 0;
    char str_tmp1[WORD_SIZE] = "";
    char str_tmp2[WORD_SIZE] = "";
    while (fgets(inf_str, LINE_SIZE - 1, pf) != NULL) {

	    if (item_num >= pd_conf->size) {
		    ret = -1;
		    goto end;
	    }
	    line_num++;

	    /* explain line */
	    if (inf_str[0] == '#') {
		    continue;
	    }

	    /* analysis line */
	    sublen = (int)strlen(inf_str);
	    if (sublen <= 0) {
		    continue;
	    }

	    status = 0;
	    sep_sign = 0;     
	    for (i = 0; i <= sublen && status != 4 && status != 5; i++) {
		    ch = inf_str[i];
		    switch (status) {
			    case 0: {       //去第一个字段的前空格
					    if (ch == SPACE || ch == TAB) {
						    break;  //空格继续
					    } else if (ch == '\r' || ch == '\n') {
						    status = 6; //空行
						    break;
					    } else if (ch == ':') {
						    status = 5; //错误退出
						    break;
					    }
					    j = 0;
					    str_tmp1[j++] = ch;
					    status = 1;
					    break;
				    }
			    case 1: {       //接收name
					    if (ch == SPACE || ch == TAB || ch == ':') { //遇到name后面第一个无效字符
						    if (ch == ':') {
							    sep_sign = 1;
						    }
						    str_tmp1[j] = '\0';
						    status = 2;
						    break;
					    } else if (ch == '\r' || ch == '\n') {  //非法的结束
						    status = 5;
						    break;
					    }
					    str_tmp1[j++] = ch;
					    if(j>=WORD_SIZE){
						    goto end;
					    }
					    break;
				    }
			    case 2: {       //去掉value前面的多余字符
					    if (ch == SPACE || ch == TAB || ch == ':') {
						    if (ch == ':' && sep_sign == 0) {
							    sep_sign = 1;
						    } else if (ch == ':') { //第二次遇到冒号，认为是内容的一部分
							    j = 0;
							    status = 3;
						    }

						    break;
					    } else if (ch == '\r' || ch == '\n') {
						    str_tmp2[0] = '\0';
						    status = 4;
						    break;
					    }
					    j = 0;
					    status = 3;
					    break;
				    }
			    case 3: {
					    j = sublen - 1; //去除尾部无用字段

					    while ( (j>=0) && (inf_str[j] == SPACE || inf_str[j] == TAB || inf_str[j] == '\r'
								    || inf_str[j] == '\n' || inf_str[j] == '\0') ) {
						    inf_str[j--] = '\0';
					    }
					    if (i>=1 && (j >= i - 1) && (j - i + 2 < WORD_SIZE)) {
						    strncpy(str_tmp2, &inf_str[i - 1], WORD_SIZE - 1);
						    str_tmp2[WORD_SIZE - 1] = '\0';
						    status = 4;
						    break;
					    }
					    status = 5;
					    break;
				    }
			    default: {
					     break;
				     }
				
		    }
		    	
	    }
	    if (status == 4 && sep_sign == 1) {

		    snprintf(pd_conf->item[item_num].name, sizeof(pd_conf->item[item_num].name), "%s", str_tmp1);
		    snprintf(pd_conf->item[item_num].value, sizeof(pd_conf->item[item_num].value), "%s", str_tmp2);
		    item_num++;
		    continue;
	    }
	    if (status == 6) {
		    continue;           //空行继续
	    }
	    /* error line */
	    mini_writelog(MINI_LOG_FATAL, "[readconf] line (%d) in conf file (%s) error", line_num, fullpath);
    }
    /* give the real number of conf */
    pd_conf->num = item_num;
    ret = 1;

    /* close conf file */
end:
    fclose(pf);
    return ret;
    
}

int mini_readconf_no_dir(const char *fullpath, mini_confdata_t *pd_conf) {
    return readconf_no_dir(fullpath, pd_conf, 0);
}
int mini_readconf(const char *work_path, const char *fname, mini_confdata_t * pd_conf) {

    if (work_path == NULL || fname == NULL || pd_conf == NULL) {
        return -1;
    }

    char totalname[LINE_SIZE];
    snprintf(totalname, sizeof(totalname), "%s/%s", work_path, fname);
    return mini_readconf_no_dir(totalname, pd_conf);
}
int mini_getconfstr(mini_confdata_t * pd_conf, const char *c_name, char *c_value) {
    if (c_value == NULL || c_name == NULL || pd_conf == NULL) {
        return -1;
    }
    c_value[0] = '\0';

    for (int i = 0; i < pd_conf->num; i++) {
        if (strcmp((pd_conf->item[i]).name, c_name) == 0) {
            strlcpy(c_value, (pd_conf->item[i]).value, strlen((pd_conf->item[i]).value)+1);
            return 0;
        }
    }

    return 1;

}

