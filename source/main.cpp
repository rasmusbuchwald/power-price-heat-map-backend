#include <vector>
#include <iostream>

#include "api_handler.h"
#include "database_handler.h"

int main()
{

  std::vector<char> replybuffer = HttpGetDayAheadPrices();

  if (replybuffer.empty())
  {
    std::cerr << "replybuffer is empty" << std::endl;
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
      persistInDb(json_root);
      cJSON_Delete(json_root);
    }

  }
}


