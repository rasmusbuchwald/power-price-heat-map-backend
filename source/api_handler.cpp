#include <vector>
#include <string>
#include <curl/curl.h>
#include <iostream>

#include "api_handler.h"

size_t CurlWriteCallback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    const size_t bytes = size * nmemb;
    std::vector<char> *buffer = static_cast<std::vector<char> *>(userdata);
    buffer->insert(buffer->end(), ptr, ptr + bytes);
    return bytes;
}

std::vector<char> HttpGetDayAheadPrices()
{

    std::vector<char> result;
    const std::string url = "https://api.energidataservice.dk/dataset/DayAheadPrices?offset=0&start=2020-02-21T00:00&sort=TimeUTC%20DESC";

    curl_global_init(CURL_GLOBAL_DEFAULT);
    CURL *curl = curl_easy_init();
    if (!curl)
    {
        std::cerr << "[curl] curl_easy_init failed" << std::endl;
    }
    else
    {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &CurlWriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);

        CURLcode rc = curl_easy_perform(curl);
        if (rc != CURLE_OK)
        {
            std::cerr << "[curl] curl_easy_perform failed: " << curl_easy_strerror(rc) << std::endl;
        }
        else
        {
            long httpCode = 0;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

            if (httpCode < 200 || httpCode >= 300)
            {
                std::cerr << "[http] request failed, status code: " << httpCode << std::endl;
            }

            curl_easy_cleanup(curl);
        }
    }
    curl_global_cleanup();
    return result;
}