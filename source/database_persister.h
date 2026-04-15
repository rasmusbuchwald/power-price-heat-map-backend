#ifndef DEFINE__DB_HANDLER
#define DEFINE__DB_HANDLER

#include <vector>
#include <string>
#include <cjson/cJSON.h>
#include <postgresql/libpq-fe.h>

void persistInDb(cJSON *json_root);

#endif // DEFINE__DB_HANDLER