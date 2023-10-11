#pragma once

#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include <openssl/hmac.h>
#include <openssl/sha.h>

namespace ftx
{
namespace crypto
{

static std::string HmacSha256(const std::string& data, const std::string& secret)
{
    const unsigned char* data_char = reinterpret_cast<const unsigned char*>(data.c_str());
    const unsigned char* secret_char = reinterpret_cast<const unsigned char*>(secret.c_str());

    std::vector<unsigned char> result(EVP_MAX_MD_SIZE);
    unsigned int result_len = 0;

    HMAC_CTX* ctx = HMAC_CTX_new();
    HMAC_Init_ex(ctx, secret_char, secret.length(), EVP_sha256(), NULL);
    HMAC_Update(ctx, data_char, data.length());
    HMAC_Final(ctx, result.data(), &result_len);
    HMAC_CTX_free(ctx);

    result.resize(result_len);

    std::stringstream ss;
    for (const unsigned char c : result)
    {
        ss << std::setfill('0') << std::setw(2) << std::hex << static_cast<unsigned int>(c);
    }

    return ss.str();
}

}  // namespace crypto
}  // namespace ftx

