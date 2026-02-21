#include <cstdlib>
#include <iomanip>
#include <cmath>
#include <iostream>

#include "database_handler.h"

cJSON *getJsonObject(cJSON *json_parrentObject, std::string name)
{
    cJSON *result = cJSON_GetObjectItem(json_parrentObject, name.c_str());
    if (!result)
    {
        if (name == "DayAheadPriceEUR")
        {
            result = getJsonObject(json_parrentObject, "SpotPriceEUR");
        }
        else if (name == "DayAheadPriceDKK")
        {
            result = getJsonObject(json_parrentObject, "SpotPriceDKK");
        }
        else if (name == "TimeUTC")
        {
            result = getJsonObject(json_parrentObject, "HourUTC");
        }
        else
        {
            std::cerr << "\"" << name << "\" missing." << std::endl;
        }
    }
    return result;
}

std::vector<std::string> buildParams_dayAead(cJSON *json_parrentObject)
{
    std::vector<std::string> result;
    cJSON *timeUtc = getJsonObject(json_parrentObject, "TimeUTC");
    cJSON *area = getJsonObject(json_parrentObject, "PriceArea");
    cJSON *priceDkk = getJsonObject(json_parrentObject, "DayAheadPriceDKK");
    if (cJSON_IsString(timeUtc) && cJSON_IsString(area) && cJSON_IsNumber(priceDkk))
    {
        result.push_back(timeUtc->valuestring);
        result.push_back(area->valuestring);
        result.push_back(std::to_string(priceDkk->valuedouble));
    }
    return result;
}

std::vector<std::string> buildParams_exchangeRate(cJSON *json_parrentObject)
{
    std::vector<std::string> result;
    cJSON *timeUtc = getJsonObject(json_parrentObject, "TimeUTC");
    cJSON *priceDkk = getJsonObject(json_parrentObject, "DayAheadPriceDKK");
    cJSON *PriceEur = getJsonObject(json_parrentObject, "DayAheadPriceEUR");
    if (cJSON_IsString(timeUtc) && cJSON_IsNumber(priceDkk) && cJSON_IsNumber(PriceEur))
    {
        double exchangeRate = priceDkk->valuedouble / PriceEur->valuedouble;
        if (std::isfinite(exchangeRate)) // best che
        {
            std::string timeStr = timeUtc->valuestring;
            if (timeStr.size() >= 10)
            {
                timeStr = timeStr.substr(0, 10) + "T00:00:00Z";
            }
            result.push_back(timeStr);
            result.push_back(std::to_string(exchangeRate));
        }
    }
    return result;
}

bool ExecOk(PGconn *conn, const char *sql)
{
    PGresult *res = PQexec(conn, sql);
    bool result = res && (PQresultStatus(res) == PGRES_COMMAND_OK);
    if (!result)
        std::cerr << "psql Exec failed: " << PQerrorMessage(conn) << std::endl;
    if (res)
        PQclear(res);
    return result;
}

void processJsonRecords(cJSON *json_records, PGconn *db_conn, const std::string &preparedStatementName, std::function<std::vector<std::string>(cJSON *)> buildParams, size_t cmp_index)
{
    const int json_records_count = cJSON_GetArraySize(json_records);
    std::cout << "Processing " << json_records_count << " records." << std::endl;

    int index_batch = 0;
    std::vector<std::string> params_prev;
    while (index_batch < json_records_count)
    {
        std::cout << "Persisteting preparedStatem \"" << std::string(preparedStatementName) << "\" form index " << index_batch << std::flush;
        if (ExecOk(db_conn, "BEGIN"))
        {
            int index_record = index_batch;
            for (; index_record < json_records_count && index_record < (index_batch + 2500); ++index_record)
            {
                cJSON *json_record = cJSON_GetArrayItem(json_records, index_record);
                if (!cJSON_IsObject(json_record))
                {
                    std::cerr << "Index " << index_record << " is not an json object" << std::endl;
                }
                else
                {
                    std::vector<std::string> params = buildParams(json_record);
                    if (!params.empty() && (cmp_index < 0 || (cmp_index > params_prev.size()) || (cmp_index > params.size()) || (params[cmp_index] != params_prev[cmp_index])))
                    {
                        params_prev = params;
                        std::vector<const char *> paramPtrs;
                        paramPtrs.reserve(params.size());
                        for (const auto &s : params)
                            paramPtrs.emplace_back(s.c_str());

                        PGresult *pgResult = PQexecPrepared(db_conn, preparedStatementName.c_str(), static_cast<int>(paramPtrs.size()), paramPtrs.data(), nullptr, nullptr, 0);
                        if (!pgResult)
                        {
                            std::cerr << "pgResult for \"" << preparedStatementName << " is nullptr" << std::endl;
                        }
                        else
                        {
                            if (PQresultStatus(pgResult) != PGRES_COMMAND_OK)
                            {
                                std::cerr << "psql Upsert  for \"" << preparedStatementName << " failed: " << PQerrorMessage(db_conn) << std::endl;
                            }
                            PQclear(pgResult);
                        }
                    }
                }
            }
            if (!ExecOk(db_conn, "COMMIT"))
            {
                ExecOk(db_conn, "ROLLBACK");
            }
            else
            {
                double percent = (static_cast<double>(index_record) / json_records_count) * 100.0;
                std::cout << " - " << index_record - index_batch << " records Persisteted. (" << std::fixed << std::setprecision(1) << percent << "%)" << std::endl;
            }
            index_batch = index_record;
        }
    }
}

void persistInDb(cJSON *json_root)
{

    cJSON *json_records = getJsonObject(json_root, "records");
    if (!cJSON_IsArray(json_records))
    {
        std::cerr << "[cJSON] 'records' missing or not an array" << std::endl;
    }
    else
    {
        std::string conninfo = "host=" + std::string(std::getenv("DB_HOST")) + " port=" + std::string(std::getenv("DB_PORT")) + " dbname=" + std::string(std::getenv("DB_NAME")) + " user=" + std::string(std::getenv("DB_USER")) + " password=" + std::string(std::getenv("DB_PASSWORD"));
        PGconn *db_conn = PQconnectdb(conninfo.c_str());
        if (!db_conn)
        {
            std::cerr << "db_conn in nullptr." << std::endl;
        }
        else
        {
            if (PQstatus(db_conn) != CONNECTION_OK)
            {
                std::cerr << "psql Connection failed: " << PQerrorMessage(db_conn) << std::endl;
            }
            else
            {
                const char *upsertSql_dayAheadPrices =
                    "INSERT INTO day_ahead_prices (time_utc, price_area, price_dkk) "
                    "VALUES ($1::timestamptz, $2::text, $3::float8) "
                    "ON CONFLICT (time_utc, price_area) DO UPDATE "
                    "SET price_dkk = EXCLUDED.price_dkk "
                    "WHERE day_ahead_prices.price_dkk IS DISTINCT FROM EXCLUDED.price_dkk";

                const char *upsertSql_exchangeRate =
                    "INSERT INTO exchange_rate (time_utc, base_currency, quote_currency, exchange_rate) "
                    "VALUES ($1::timestamptz, 'EUR', 'DKK', $2::numeric) "
                    "ON CONFLICT (base_currency, quote_currency, time_utc) DO UPDATE SET "
                    "  exchange_rate = EXCLUDED.exchange_rate";

                std::string preparedStatementName_dayAhead = "upsert_day_ahead";
                std::string preparedStatementName_exchangeRate = "upsert_exchange_rate";
                PGresult *prep_dayAhead = PQprepare(db_conn, preparedStatementName_dayAhead.c_str(), upsertSql_dayAheadPrices, 3, nullptr);
                PGresult *prep_exchangeRate = PQprepare(db_conn, preparedStatementName_exchangeRate.c_str(), upsertSql_exchangeRate, 3, nullptr);
                if (!prep_dayAhead || !prep_exchangeRate)
                {
                    std::cerr << "prep is nullptr" << std::endl;
                }
                else
                {
                    if (PQresultStatus(prep_dayAhead) != PGRES_COMMAND_OK || PQresultStatus(prep_exchangeRate) != PGRES_COMMAND_OK)
                    {
                        std::cerr << "psql Prepare failed: " << PQerrorMessage(db_conn) << std::endl;
                    }
                    else
                    {
                        processJsonRecords(json_records, db_conn, preparedStatementName_dayAhead, buildParams_dayAead, -1);
                        processJsonRecords(json_records, db_conn, preparedStatementName_exchangeRate, buildParams_exchangeRate, 1);
                    }
                }
                PQclear(prep_dayAhead);
                PQclear(prep_exchangeRate);
            }
        }
        PQfinish(db_conn);
    }
}
