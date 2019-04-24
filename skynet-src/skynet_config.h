#ifndef SKYNET_CONFIG_H
#define SKYNET_CONFIG_H

void skynet_config_init(const char * file);
void skynet_config_free();

int skynet_config_int(const char * section, const char * option, int * value);
int skynet_config_string(const char * section, const char * option, char *value, int size);

#endif //SKYNET_CONFIG_H
