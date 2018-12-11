#include "skynet.h"
#include "skynet_config.h"

#include <stdio.h>
#include <string.h>

static char * config_file = NULL;

void skynet_config_init(const char * file) {
	size_t len = strlen(file) + 1;
	config_file = skynet_malloc(len);
	memcpy(config_file, file, len);
}

void skynet_config_free() {
	skynet_free(config_file);
}

int config_parse(FILE * fp, const char * section, const char * option, char * value, int size) {
	char * split;
    int flag = 0, i = 0, len = 0;
    char line[1024], title[1024], buff[1024];

	while (NULL != fgets(line, sizeof(line), fp)) {
		i = 0, len = 0;
		while (line[i] != '\0' && line[i] != '#') {
			if (line[i] != ' ' && line[i] != '\n' && line[i] != '\r' && line[i] != '"') {
				buff[len++] = line[i];
			}
			++i;
		}
		buff[len] = '\0';

		if (!flag) {
			sprintf(title, "[%s]", section);
			if (!strncmp(buff, title, strlen(section))) {
				flag = 1;
			}
			continue;
		} else {
			if (buff[0] == '[' && buff[len-1] == ']') {
				skynet_logger_error(NULL, "fail to find %s:%s in config file: %s\n", section, option, config_file);
				printf("fail to find %s:%s in config file: %s\n", section, option, config_file);
				break;
			}

			split = strchr(buff, '=');
			if (split != NULL && len-(split-buff) < size) {
				if (strncmp(buff, option, split-buff)) {
					continue;
				}
				memset(value, 0, size);
				memcpy(value, split+1, len-(split-buff));
				return 0;
			} else {
				skynet_logger_error(NULL, "fail to parse config file: %s\n", config_file);
				printf("fail to parse config file: %s\n", config_file);
				break;
			}
		}
    }
	return 1;
}

int skynet_config_string(const char * section, const char * option, char * value, int size) {
    int ret;
	FILE * fp;

	fp = fopen(config_file, "r+");
	if (fp == NULL) {
		skynet_logger_error(NULL, "fail to open config file: %s\n", config_file);
		printf("fail to open config file: %s\n", config_file);
        return 1;
	}

	ret = config_parse(fp, section, option, value, size);

    fclose(fp);
	return ret;
}

int skynet_config_int(const char * section, const char * option, int * value) {
	int ret;
	char s[1024];
	ret = skynet_config_string(section, option, s, sizeof(s));
	if (ret == 0) 
		*value = atoi(s);
	return ret;
}
