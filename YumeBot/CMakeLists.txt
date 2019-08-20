set(SOURCE_FILES
    Cryptography.cpp
    Jce.cpp
    Session.cpp
    Tlv.cpp
    Wup.cpp)

set(HEADERS
    Cryptography.h
    Jce.h
    JceStructDef.h
    Misc.h
    Request.h
    Session.h
    Tlv.h
    Utility.h
    Wup.h)

add_library(YumeBot ${SOURCE_FILES} ${HEADERS})

find_package(OpenSSL REQUIRED)

target_compile_features(YumeBot PUBLIC
    cxx_std_20)

target_include_directories(YumeBot INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:.>
    PRIVATE ${OPENSSL_INCLUDE_DIR})

target_link_libraries(YumeBot
    PUBLIC CONAN_PKG::Cafe
    PRIVATE ${OPENSSL_LIBRARIES})

install(TARGETS YumeBot
    EXPORT YumeBot)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    TYPE INCLUDE
    FILES_MATCHING PATTERN "*.h")

install(EXPORT YumeBot
    DESTINATION cmake/YumeBot)
