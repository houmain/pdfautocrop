cmake_minimum_required(VERSION 3.5)

project(pdfautocrop LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SOURCES
  src/input.cpp
  src/optimize.cpp
  src/output.cpp
  src/settings.cpp
  src/main.cpp
)
file(GLOB_RECURSE HEADERS include *.h)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

target_link_libraries(${PROJECT_NAME} poppler-cpp qpdf)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} Threads::Threads)
