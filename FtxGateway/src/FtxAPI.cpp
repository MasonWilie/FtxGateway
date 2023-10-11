#include "FtxAPI.h"

#include <chrono>

#include "HmacSha256.hpp"

namespace
{

static int64_t GetTimestampMs()
{
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count() * 1000;
}

}

namespace ftx
{

FtxAPI::FtxAPI(const std::string& key
            , const std::string& secret
            , const std::string& endpoint)
    : _key(key)
    , _secret(secret)
    , _endpoint(endpoint)
{}

FtxAPI::Response_t FtxAPI::GetRequest(const std::string& path) const
{
    const int64_t time_ms = GetTimestampMs();

    cpr::Response r = cpr::Get(
        cpr::Url{_endpoint + path},
        CreateHeader(path, time_ms, Method::GET)
    );

    rapidjson::Document json_response;
    json_response.Parse(r.text.c_str());

    return json_response;
}

FtxAPI::Response_t FtxAPI::PostRequest(const std::string& path, const std::string& body) const
{
    const int64_t time_ms = GetTimestampMs();

    cpr::Response r = cpr::Post(
        cpr::Url{_endpoint + path},
        CreateHeader(path, time_ms, Method::POST, body),
        cpr::Body(body)
    );

    rapidjson::Document json_response;
    json_response.Parse(r.text.c_str());

    return json_response;
}

FtxAPI::Response_t FtxAPI::DeleteRequest(const std::string& path) const
{
    const int64_t time_ms = GetTimestampMs();

    cpr::Response r = cpr::Delete(
        cpr::Url{_endpoint + path},
        CreateHeader(path, time_ms, Method::DELETE)
    );

    rapidjson::Document json_response;
    json_response.Parse(r.text.c_str());

    return json_response;
}

std::string FtxAPI::MethodToString(const Method method)
{
    switch (method)
    {
    case Method::GET:
        return "GET";
    case Method::POST:
        return "POST";
    case Method::DELETE:
        return "DELETE";
    default:
        throw std::runtime_error("Invalid method");
    }
}

cpr::Header FtxAPI::CreateHeader(const std::string& request_path
            , const int64_t time_ms
            , const Method method
            , const std::string& body) const
{
    static constexpr const char* PATH_PREFIX = "/api";

    return cpr::Header
    {
        {"FTXUS-KEY", _key},
        {"FTXUS-SIGN", Sign(time_ms, method, PATH_PREFIX + request_path, body)},
        {"FTXUS-TS", std::to_string(time_ms)},
        {"Content-Type", "application/json"},
        {"Accepts", "application/json"}
    };
}

std::string FtxAPI::Sign(const int64_t timestamp
        , const Method http_method
        , const std::string& request_path
        , const std::string& request_body) const
{
    const std::string data = std::to_string(timestamp)
        + MethodToString(http_method)
        + request_path
        + request_body;
    
    return HmacSha256(data, _secret);
}
} // namespace ftx