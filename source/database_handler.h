#ifndef DEFINE__DB_HANDLER
#define DEFINE__DB_HANDLER

#include <vector>
#include <string>
#include <functional>
#include <cjson/cJSON.h>
#include <postgresql/libpq-fe.h>

cJSON *getJsonObject(cJSON *json_parentObject, std::string name);
std::vector<std::string> buildParams_dayAead(cJSON *json_parentObject);
std::vector<std::string> buildParams_exchangeRate(cJSON *json_parentObject);
bool ExecOk(PGconn *conn, const char *sql);
void processJsonRecords(cJSON *json_records, PGconn *db_conn, const std::string &preparedStatementName, std::function<std::vector<std::string>(cJSON *)> buildParams, size_t cmp_index);
void persistInDb(cJSON *json_root);

#endif // DEFINE__DB_HANDLER