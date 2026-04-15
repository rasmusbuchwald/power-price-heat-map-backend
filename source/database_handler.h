#ifndef DEFINE__DB_HANDLER
#define DEFINE__DB_HANDLER

#include <vector>
#include <string>
#include <chrono>
#include <optional>
#include <cjson/cJSON.h>
#include <postgresql/libpq-fe.h>

PGconn *connectToDb();
void persistInDb(PGconn *db_conn, cJSON *json_root);
std::optional<std::chrono::sys_seconds> getLatestTimestamp(PGconn *db_conn);

#endif // DEFINE__DB_HANDLER