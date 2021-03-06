#include <stdlib.h>
#include <string.h>
#include <map>
#include <string>

#include "logging.h"
#include "loadconfig.h"

#ifdef __cplusplus
extern "C" {
#endif
#ifdef __linux__
#include "safe_lib.h"
#endif
#ifdef __cplusplus
}
#endif

int load_config(const char * configFile, std::map<std::string, std::string> & config_map)
{
	FILE *fp = fopen(configFile, "r");
	char *line;
	int line_size = 512;
	char *key = NULL;
	char *value = NULL;
	long unsigned int cur_line_size = 0;
	LOG_TRACE("Loading vRTM config file %s", configFile);
	if (fp == NULL)
	{
		LOG_ERROR("Failed to load vRTM config file");
		return -1;
	}
	while (true)
	{
		line = (char *)calloc(1, sizeof(char) * 512);
		if (line != NULL) {
			fgets(line, line_size, fp);
			if (feof(fp)) {
				free(line);
				break;
			}
			LOG_TRACE("Line read from config file: %s", line);
			if (line[0] == '#') {
				LOG_DEBUG("Comment in configuration file : %s", &line[1]);
				free(line);
				continue;
			}
			size_t line_max = 512;
			char *next_token;
#ifdef __linux__
			key = strtok_s(line, &line_max, "=", &next_token);
			value = strtok_s(NULL, &line_max, "=", &next_token);
#elif _WIN32
			key = strtok_s(line, "=", &next_token);
			value = strtok_s(NULL, "=", &next_token);
#endif
			if (key != NULL && value != NULL) {
				std::string map_key(key);
				std::string map_value(value);
				LOG_TRACE("Parsed Key=%s and Value=%s", map_key.c_str(), map_value.c_str());
				std::pair<std::string, std::string> config_pair(trim_copy(map_key, " \t\n"), trim_copy(map_value, " \t\n"));
				config_map.insert(config_pair);
			}
			free(line);
		}
		else {
			LOG_ERROR("Can't allocate memory to read a line");
			config_map.clear();
			fclose(fp);
			return -1;
		}
	}
	fclose(fp);
	return config_map.size();
}

void clear_config(std::map<std::string, std::string> & config_map)
{
	config_map.clear();
}
