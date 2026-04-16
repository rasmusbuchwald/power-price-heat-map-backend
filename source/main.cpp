#include <vector>
#include <iostream>
#include <chrono>
#include <optional>
#include <iomanip>
#include <ctime>

#include "api_handler.h"
#include "database_handler.h"

int main()
{
  PGconn *db_conn = connectToDb();
  if (!db_conn)
    return 1;

  std::string latestTimestamp = getLatestTimestamp(db_conn);
  if (latestTimestamp.size() > 0)
  {
    std::cout << "Latest timestamp in DB: " << latestTimestamp << std::endl;
  }
  else
    std::cout << "No data in DB yet." << std::endl;

  std::vector<char> replybuffer = HttpGetDayAheadPrices();

  if (replybuffer.empty())
  {
    std::cerr << "replybuffer is empty" << std::endl;
  }
  else
  {
    std::string json(replybuffer.data(), replybuffer.size());
    cJSON *json_root = cJSON_Parse(json.c_str());
    if (!json_root)
    {
      std::cerr << "cJSON Parse failed" << std::endl;
    }
    else
    {
      persistInDb(db_conn, json_root);
      cJSON_Delete(json_root);
    }
  }

  PQfinish(db_conn);
}
