cmake_minimum_required(VERSION 3.5)

project(hi-tech LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt5 COMPONENTS Widgets SerialPort Sql REQUIRED)

if(ANDROID)
  add_library(hi-tech SHARED
    main.cpp mainwindow.cpp mainwindow.h database.cpp database.h linkmanager.cpp linkmanager.h
  )
else()
  add_executable(hi-tech
    main.cpp mainwindow.cpp mainwindow.h database.cpp database.h linkmanager.cpp linkmanager.h
  )
endif()

target_link_libraries(hi-tech PRIVATE Qt5::Widgets Qt5::SerialPort Qt5::Sql)

# Копирование инструкций
add_custom_command(TARGET hi-tech POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:hi-tech>/instructions"
)
add_custom_command(TARGET hi-tech POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/instructions" "$<TARGET_FILE_DIR:hi-tech>/instructions"
)