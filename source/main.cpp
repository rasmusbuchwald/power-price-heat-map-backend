// main.cpp
#include <curl/curl.h>
#include <iostream>
#include <vector>
#include <cjson/cJSON.h>
#include <postgresql/libpq-fe.h>
#include <cstdlib>

bool ExecOk(PGconn *conn, const char *sql)
{
  PGresult *res = PQexec(conn, sql);
  if (!res)
  {
    std::cerr << "psql Exec failed: " << PQerrorMessage(conn) << std::endl;
    return false;
  }

  if (PQresultStatus(res) != PGRES_COMMAND_OK)
  {
    std::cerr << "psql Exec failed: " << PQerrorMessage(conn) << std::endl;
    PQclear(res);
    return false;
  }

  PQclear(res);
  return true;
}

size_t CurlWriteCallback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
  const size_t bytes = size * nmemb;
  std::vector<char> *buffer = static_cast<std::vector<char> *>(userdata);
  buffer->insert(buffer->end(), ptr, ptr + bytes);
  return bytes;
}

bool HttpGetDayAheadPrices()
{
  bool result;
  const std::string url = "https://api.energidataservice.dk/dataset/DayAheadPrices?offset=0&sort=TimeUTC%20DESC";
  std::string conninfo = "host=" + std::string(std::getenv("DB_HOST")) + " port=" + std::string(std::getenv("DB_PORT")) + " dbname=" + std::string(std::getenv("DB_NAME")) + " user=" + std::string(std::getenv("DB_USER")) + " password=" + std::string(std::getenv("DB_PASSWORD"));

  curl_global_init(CURL_GLOBAL_DEFAULT);
  CURL *curl = curl_easy_init();
  if (!curl)
  {
    std::cerr << "[curl] curl_easy_init failed" << std::endl;
    curl_global_cleanup();
    return false;
  }

  std::vector<char> replybuffer;
  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &CurlWriteCallback);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &replybuffer);

  CURLcode rc = curl_easy_perform(curl);

  if (rc != CURLE_OK)
  {
    std::cerr << "[curl] curl_easy_perform failed: " << curl_easy_strerror(rc) << std::endl;
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return false;
  }

  long httpCode = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

  if (httpCode < 200 || httpCode >= 300)
  {
    std::cerr << "[http] request failed, status code: " << httpCode << std::endl;
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return false;
  }

  curl_easy_cleanup(curl);
  curl_global_cleanup();

  if (replybuffer.empty())
  {
    std::cerr << "replybuffer empty" << std::endl;
  }
  else
  {
  //  std::cout.write(replybuffer.data(), static_cast<std::streamsize>(replybuffer.size()));
  //  std::cout << std::endl;

    std::string json(replybuffer.data(), replybuffer.size());
    cJSON *json_root = cJSON_Parse(json.c_str());
    if (!json_root)
    {
      std::cerr << "cJSON Parse failed" << std::endl;
    }
    else
    {
      cJSON *json_records = cJSON_GetObjectItemCaseSensitive(json_root, "records");
      if (!cJSON_IsArray(json_records))
      {
        std::cerr << "[cJSON] 'records' missing or not an array" << std::endl;
      }
      else
      {

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
                "ON CONFLICT (time_utc, price_area) DO UPDATE SET "
                "  price_dkk = EXCLUDED.price_dkk";

            const char *upsertSql_exchangeRate =
                "INSERT INTO exchange_rate (time_utc, base_currency, quote_currency, exchange_rate) "
                "VALUES ($1::timestamptz, 'EUR', 'DKK', $2::numeric) "
                "ON CONFLICT (base_currency, quote_currency, time_utc) DO UPDATE SET "
                "  exchange_rate = EXCLUDED.exchange_rate";

            PGresult *prep_dayAhead = PQprepare(db_conn, "upsert_day_ahead", upsertSql_dayAheadPrices, 3, nullptr);
            PGresult *prep_exchangeRate = PQprepare(db_conn, "upsert_exchange_rate", upsertSql_exchangeRate, 3, nullptr);
            if (!prep_dayAhead || !prep_exchangeRate)
            {
              std::cerr << "prep in nullptr" << std::endl;
            }
            else
            {
              if (PQresultStatus(prep_dayAhead) != PGRES_COMMAND_OK || PQresultStatus(prep_exchangeRate) != PGRES_COMMAND_OK)
              {
                std::cerr << "psql Prepare failed: " << PQerrorMessage(db_conn) << std::endl;
              }
              else
              {
                if (!ExecOk(db_conn, "BEGIN"))
                {
                  std::cerr << "psql BEGIN failed" << std::endl;
                }
                else
                {
                  const int json_records_count = cJSON_GetArraySize(json_records);
                  std::cout << "Got " << json_records_count << " records." << std::endl;
                  for (int i = 0; i < json_records_count; ++i)
                  {
                    cJSON *json_record = cJSON_GetArrayItem(json_records, i);
                    if (!cJSON_IsObject(json_record))
                    {
                      std::cerr << "Json is not object" << std::endl;
                    }
                    else
                    {
                      cJSON *timeUtc = cJSON_GetObjectItemCaseSensitive(json_record, "TimeUTC");
                      cJSON *area = cJSON_GetObjectItemCaseSensitive(json_record, "PriceArea");
                      cJSON *priceDkk = cJSON_GetObjectItemCaseSensitive(json_record, "DayAheadPriceDKK");
                      cJSON *PriceEur = cJSON_GetObjectItemCaseSensitive(json_record, "DayAheadPriceEUR");
                      if (!cJSON_IsString(timeUtc) || !cJSON_IsString(area) || !cJSON_IsNumber(priceDkk))
                      {
                        std::cerr << "Json value missing" << std::endl;
                      }
                      else
                      {

                        std::string dkkStr = std::to_string(priceDkk->valuedouble);
                        const char *params_dayAhead[3] = {timeUtc->valuestring, area->valuestring, dkkStr.c_str()};
                        PGresult *pgResult = PQexecPrepared(db_conn, "upsert_day_ahead", std::size(params_dayAhead), params_dayAhead, nullptr, nullptr, 0);
                        if (!pgResult)
                        {
                          std::cerr << "pgResult dayAhead is nullptrd" << std::endl;
                        }
                        else
                        {
                          if (PQresultStatus(pgResult) != PGRES_COMMAND_OK)
                          {
                            std::cerr << "psql Upsert dayAhead failed: " << PQerrorMessage(db_conn) << std::endl;
                          }

                          PQclear(pgResult);
                        }

                        std::string exchangeRateStr = std::to_string(priceDkk->valuedouble / PriceEur->valuedouble);
                        const char *params_exchangeRate[2] = {timeUtc->valuestring, exchangeRateStr.c_str()};
                        pgResult = PQexecPrepared(db_conn, "upsert_exchange_rate", std::size(params_exchangeRate), params_exchangeRate, nullptr, nullptr, 0);
                        if (!pgResult)
                        {
                          std::cerr << "pgResult exchangeRate is nullptr" << std::endl;
                        }
                        else
                        {
                          if (PQresultStatus(pgResult) != PGRES_COMMAND_OK)
                          {
                            std::cerr << "psql Upsert exchangeRate failed: " << PQerrorMessage(db_conn) << std::endl;
                          }

                          PQclear(pgResult);
                        }
                      }
                    }
                  }
                  if (!ExecOk(db_conn, "COMMIT"))
                  {
                    std::cerr << "psql COMMIT failed" << std::endl;
                    ExecOk(db_conn, "ROLLBACK");
                    result = false;
                  }
                }
              }
              PQclear(prep_dayAhead);
              PQclear(prep_exchangeRate);
            }
          }
          PQfinish(db_conn);
        }
      }
    }
    cJSON_Delete(json_root);
  }

  return result;
}

int main()
{
  HttpGetDayAheadPrices();
}