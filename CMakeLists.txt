cmake_minimum_required(VERSION 3.0)

project(wsclient)
add_executable(wsclient src/source.cpp src/websocketwrapper.cpp)
include_directories(src)

#OpenSSl
find_package(OpenSSL REQUIRED)
include_directories(${OPENSSL_INCLUDE_DIR})
target_link_libraries(wsclient ${OPENSSL_LIBRARIES})


#Boost
find_package(Boost REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})
target_link_libraries(wsclient ${Boost_LIBRARIES})


