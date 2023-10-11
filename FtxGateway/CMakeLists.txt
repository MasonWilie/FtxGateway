INCLUDE_DIRECTORIES(libs/rapidjson)
INCLUDE_DIRECTORIES(inc)

SET(SRC
        src/FtxAPI.cpp
        src/FtxWebSocket.cpp
        src/Gateway.cpp)

SET(INC
        inc/FtxAPI.h
        inc/FtxWebSocket.h
        inc/Gateway.h
        inc/HmacSha256.hpp
        inc/FtxWebSocketMessages.h)

ADD_LIBRARY(FtxGateway ${SRC} ${INC})

include(FetchContent)
FetchContent_Declare(cpr GIT_REPOSITORY https://github.com/libcpr/cpr.git)
FetchContent_MakeAvailable(cpr)

FIND_PACKAGE(RapidJSON REQUIRED)
FIND_PACKAGE(OpenSSL REQUIRED)
find_package(websocketpp REQUIRED)

SET_PROPERTY(TARGET FtxGateway PROPERTY CXX_STANDARD 17)

TARGET_LINK_LIBRARIES(FtxGateway
        OpenSSL::SSL
        cpr::cpr)