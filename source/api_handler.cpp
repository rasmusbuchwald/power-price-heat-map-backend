#include <vector>
#include <string>
#include <ctime>
#include <curl/curl.h>
#include <iostream>

#include "helper_functions.h"
#include "api_handler.h"

static size_t CurlWriteCallback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    const size_t bytes = size * nmemb;
    std::vector<char> *buffer = static_cast<std::vector<char> *>(userdata);
    buffer->insert(buffer->end(), ptr, ptr + bytes);
    return bytes;
}

std::vector<char> httpGetDayAheadPrices(std::tm timestampStart)
{
    std::vector<char> result;
    std::string start = zeroPadInt(timestampStart.tm_year + 1900) + "-" + zeroPadInt(timestampStart.tm_mon + 1) + "-" + zeroPadInt(timestampStart.tm_mday) + "T" + zeroPadInt(timestampStart.tm_hour) + ":" + zeroPadInt(timestampStart.tm_min);
    const std::string url = timestampStart.tm_year < 125 || (timestampStart.tm_year == 125 && (timestampStart.tm_mon < 8 || (timestampStart.tm_mon == 8 && (timestampStart.tm_mday < 30 || (timestampStart.tm_mday == 30 && timestampStart.tm_hour < 23)))))
                                ? "https://api.energidataservice.dk/dataset/Elspotprices?offset=0&start=" + start + "&sort=HourUTC%20DESC"
                                : "https://api.energidataservice.dk/dataset/DayAheadPrices?offset=0&start=" + start + "&sort=TimeUTC%20DESC";
 

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