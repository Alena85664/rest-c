cmake_minimum_required(VERSION 3.10)
project(rest_c_server VERSION 1.0)

# ----------------------------------------------------------------------------
# Стандарт C++17
# ----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ----------------------------------------------------------------------------
# Собираем все .cpp и .hpp файлы проекта
# Используем CMAKE_CURRENT_SOURCE_DIR, чтобы пути были абсолютными
# ----------------------------------------------------------------------------
file(GLOB_RECURSE SOURCES 
    "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp" 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/**/*.hpp" 
    "${CMAKE_CURRENT_SOURCE_DIR}/src/**/*.cpp"
)

add_executable(rest_server ${SOURCES})

# Указываем компилятору, где искать заголовочные файлы (корень проекта и src)
target_include_directories(rest_server PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# ----------------------------------------------------------------------------
# Многопоточность
# ----------------------------------------------------------------------------
find_package(Threads REQUIRED)
target_link_libraries(rest_server PRIVATE Threads::Threads)

# ----------------------------------------------------------------------------
# Платформенные зависимости
# ----------------------------------------------------------------------------
if(WIN32)
    target_link_libraries(rest_server PRIVATE ws2_32)
else()
    find_package(SQLite3 REQUIRED)
    target_link_libraries(rest_server PRIVATE SQLite3::SQLite3)
endif()