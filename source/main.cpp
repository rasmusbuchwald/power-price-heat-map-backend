#include <vector>
#include <iostream>
#include <chrono>
#include <optional>
#include <iomanip>
#include <ctime>

#include "helper_functions.h"
#include "api_handler.h"
#include "database_handler.h"

int main()
{
  PGconn *db_conn = connectToDb();
  if (!db_conn)
    return 1;

  std::tm startTime = getLatestCphTimestamp(db_conn);

  if (startTime.tm_year == 0)
  {
    startTime = {.tm_sec = 0, .tm_min = 0, .tm_hour = 22, .tm_mday = 30, .tm_mon = 5, .tm_year = 99};
  }
  std::cout << "starttime : " + zeroPadInt(startTime.tm_year + 1900) << "-" << zeroPadInt(startTime.tm_mon + 1) << "-" << zeroPadInt(startTime.tm_mday) << " " << zeroPadInt(startTime.tm_hour) << ":" << zeroPadInt(startTime.tm_min) << std::endl;
  std::vector<char> replybuffer = httpGetDayAheadPrices(startTime);

  // std::cout.write(replybuffer.data(), static_cast<std::streamsize>(replybuffer.size()));
  //  std::cout << std::endl;

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
