#ifndef DEFINE__API_HANDLER
#define DEFINE__API_HANDLER


#include <cjson/cJSON.h>


std::vector<char>  HttpGetDayAheadPrices();
size_t CurlWriteCallback(char *ptr, size_t size, size_t nmemb, void *userdata);


#endif // DEFINE__API_HANDLER