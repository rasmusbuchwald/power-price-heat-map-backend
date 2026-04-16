#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <functional>
#include <unordered_map>
#include <chrono>
#include <optional>
#include <sstream>

#include "database_handler.h"

static bool requireEnv(const char *name, std::string &out)
{
    const char *value = std::getenv(name);
    if (!value)
    {
        std::cerr << "ERROR: Missing required environment variable: " << name << std::endl;
        return false;
    }
    out = value;
    return true;
}

static cJSON *getJsonObject(cJSON *json_parentObject, const std::string &name)
{
    static const std::unordered_map<std::string, std::string> aliasMap = {
        {"DayAheadPriceEUR", "SpotPriceEUR"},
        {"DayAheadPriceDKK", "SpotPriceDKK"},
        {"TimeUTC", "HourUTC"},
    };

    cJSON *result = cJSON_GetObjectItem(json_parentObject, name.c_str());
    if (!result)
    {
        std::unordered_map<std::string, std::string>::const_iterator it = aliasMap.find(name);
        if (it != aliasMap.end())
            result = cJSON_GetObjectItem(json_parentObject, it->second.c_str());
    }

    if (!result)
        std::cerr << "\"" << name << "\" missing." << std::endl;

    return result;
}

static std::vector<std::string> buildParams_dayAhead(cJSON *json_parentObject)
{
    std::vector<std::string> result;
    cJSON *timeUtc = getJsonObject(json_parentObject, "TimeUTC");
    cJSON *area = getJsonObject(json_parentObject, "PriceArea");
    cJSON *priceDkk = getJsonObject(json_parentObject, "DayAheadPriceDKK");
    if (cJSON_IsString(timeUtc) && cJSON_IsString(area) && cJSON_IsNumber(priceDkk))
    {
        result.push_back(timeUtc->valuestring);
        result.push_back(area->valuestring);
        result.push_back(std::to_string(priceDkk->valuedouble));
    }
    return result;
}

static bool ExecOk(PGconn *conn, const char *sql)
{
    PGresult *res = PQexec(conn, sql);
    bool result = res && (PQresultStatus(res) == PGRES_COMMAND_OK);
    if (!result)
        std::cerr << "psql Exec failed: " << PQerrorMessage(conn) << std::endl;
    if (res)
        PQclear(res);
    return result;
}

static void processJsonRecords(cJSON *json_records, PGconn *db_conn, const std::string &preparedStatementName, std::function<std::vector<std::string>(cJSON *)> buildParams)
{
    const int json_records_count = cJSON_GetArraySize(json_records);
    std::cout << "Processing " << json_records_count << " records." << std::endl;

    int index_batch = 0;
    while (index_batch < json_records_count)
    {
        std::cout << "Persisting \"" << preparedStatementName << "\" from index " << index_batch << std::flush;

        if (!ExecOk(db_conn, "BEGIN"))
        {
            std::cerr << "BEGIN failed, skipping batch at index " << index_batch << std::endl;
            break;
        }

        int index_record = index_batch;
        for (; index_record < json_records_count && index_record < (index_batch + 2500); ++index_record)
        {
            cJSON *json_record = cJSON_GetArrayItem(json_records, index_record);
            if (!cJSON_IsObject(json_record))
            {
                std::cerr << "Index " << index_record << " is not a json object" << std::endl;
                continue;
            }

            std::vector<std::string> params = buildParams(json_record);
            if (params.empty())
                continue;

            std::vector<const char *> paramPtrs;
            paramPtrs.reserve(params.size());
            for (const auto &s : params)
                paramPtrs.emplace_back(s.c_str());

            PGresult *pgResult = PQexecPrepared(db_conn, preparedStatementName.c_str(), static_cast<int>(paramPtrs.size()), paramPtrs.data(), nullptr, nullptr, 0);
            if (!pgResult)
            {
                std::cerr << "pgResult for \"" << preparedStatementName << "\" is nullptr" << std::endl;
            }
            else
            {
                if (PQresultStatus(pgResult) != PGRES_COMMAND_OK)
                    std::cerr << "Upsert for \"" << preparedStatementName << "\" failed: " << PQerrorMessage(db_conn) << std::endl;
                PQclear(pgResult);
            }
        }

        if (!ExecOk(db_conn, "COMMIT"))
        {
            ExecOk(db_conn, "ROLLBACK");
        }
        else
        {
            double percent = (static_cast<double>(index_record) / json_records_count) * 100.0;
            std::cout << " - " << index_record - index_batch << " records persisted. (" << std::fixed << std::setprecision(1) << percent << "%)" << std::endl;
        }
        index_batch = index_record;
    }
}

PGconn *connectToDb()
{
    std::string db_host, db_port, db_name, db_user, db_password;
    if (!requireEnv("DB_HOST", db_host) || !requireEnv("DB_PORT", db_port) || !requireEnv("DB_NAME", db_name) || !requireEnv("DB_USER", db_user) || !requireEnv("DB_PASSWORD", db_password))
        return nullptr;

    std::string conninfo = "host=" + db_host + " port=" + db_port + " dbname=" + db_name + " user=" + db_user + " password=" + db_password;
    PGconn *db_conn = PQconnectdb(conninfo.c_str());
    if (!db_conn)
    {
        std::cerr << "db_conn is nullptr." << std::endl;
        return nullptr;
    }

    if (PQstatus(db_conn) != CONNECTION_OK)
    {
        std::cerr << "psql Connection failed: " << PQerrorMessage(db_conn) << std::endl;
        PQfinish(db_conn);
        return nullptr;
    }

    return db_conn;
}

std::string getLatestTimestamp(PGconn *db_conn)
{
    const char *sql = "SELECT MAX(time_utc) FROM day_ahead_prices";
    PGresult *res = PQexec(db_conn, sql);
    std::string result;
    if (!res || PQresultStatus(res) != PGRES_TUPLES_OK)
    {
        std::cerr << "getLatestTimestamp failed: " << PQerrorMessage(db_conn) << std::endl;
    }
    else if (PQntuples(res) > 0 && !PQgetisnull(res, 0, 0))
    {
        result = PQgetvalue(res, 0, 0);
    }
    if (res)
        PQclear(res);
    return result;
}

void persistInDb(PGconn *db_conn, cJSON *json_root)
{
    cJSON *json_records = getJsonObject(json_root, "records");
    if (!cJSON_IsArray(json_records))
    {
        std::cerr << "[cJSON] 'records' missing or not an array" << std::endl;
        return;
    }

    const char *upsertSql_dayAheadPrices =
        "INSERT INTO day_ahead_prices (time_utc, price_area, price_dkk) "
        "VALUES ($1::timestamptz, $2::text, $3::float8) "
        "ON CONFLICT (time_utc, price_area) DO UPDATE "
        "SET price_dkk = EXCLUDED.price_dkk "
        "WHERE day_ahead_prices.price_dkk IS DISTINCT FROM EXCLUDED.price_dkk";

    std::string preparedStatementName_dayAhead = "upsert_day_ahead";
    PGresult *prep_dayAhead = PQprepare(db_conn, preparedStatementName_dayAhead.c_str(), upsertSql_dayAheadPrices, 3, nullptr);

    if (!prep_dayAhead)
    {
        std::cerr << "prep is nullptr" << std::endl;
        return;
    }

    if (PQresultStatus(prep_dayAhead) != PGRES_COMMAND_OK)
    {
        std::cerr << "psql Prepare failed: " << PQerrorMessage(db_conn) << std::endl;
        PQclear(prep_dayAhead);
        return;
    }

    processJsonRecords(json_records, db_conn, preparedStatementName_dayAhead, buildParams_dayAhead);

    PQclear(prep_dayAhead);
}