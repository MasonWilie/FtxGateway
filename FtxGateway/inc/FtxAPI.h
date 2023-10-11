#pragma once

#include <string>

#include <cpr/cpr.h>
#include <rapidjson/document.h>

namespace ftx
{

class FtxAPI
{
public:
    using Response_t = rapidjson::Document;

    explicit FtxAPI(const std::string& key
            , const std::string& secret
            , const std::string& endpoint = "http://ftx.us/api");
    
    virtual ~FtxAPI() = default;

    Response_t GetRequest(const std::string& path) const;
    Response_t PostRequest(const std::string& path, const std::string& body) const;
    Response_t DeleteRequest(const std::string& path) const;

private:

    enum class Method
        : int
    {
        GET,
        POST,
        DELETE
    };

    static std::string MethodToString(const Method method);

    cpr::Header CreateHeader(const std::string& request_path
            , const int64_t time_ms
            , const Method method
            , const std::string& body = "") const;

    std::string Sign(const int64_t timestamp
        , const Method http_method
        , const std::string& request_path
        , const std::string& request_body = "") const;

    const std::string _key;
    const std::string _secret;
    const std::string _endpoint;
};

} // namespace ftx