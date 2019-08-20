set(SOURCE_FILES
    CryptographyTest.cpp
    JceTest.cpp
    TlvTest.cpp
    YumeBot.Test.cpp)

add_executable(YumeBot.Test ${SOURCE_FILES})

target_link_libraries(YumeBot.Test PRIVATE
    YumeBot CONAN_PKG::Catch2)
