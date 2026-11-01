cmake_minimum_required(VERSION 3.5)
project(hi-tech LANGUAGES CXX)

# Включаем автоматический MOC, UIC и RCC
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Настройки компилятора
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Ищем библиотеки Qt5 (Widgets, SerialPort, SQL, PrintSupport)
find_package(Qt5 COMPONENTS Widgets SerialPort Sql PrintSupport REQUIRED)

# Собираем исполняемый файл (Windows/Linux)
if(ANDROID)
  add_library(hi-tech SHARED
    main.cpp mainwindow.cpp mainwindow.h
    database.cpp database.h
    linkmanager.cpp linkmanager.h
  )
else()
  add_executable(hi-tech
    main.cpp mainwindow.cpp mainwindow.h
    database.cpp database.h
    linkmanager.cpp linkmanager.h
  )
endif()

# Линкуем библиотеки
target_link_libraries(hi-tech PRIVATE Qt5::Widgets Qt5::SerialPort Qt5::Sql Qt5::PrintSupport)

# Копируем инструкции рядом с .exe
add_custom_command(TARGET hi-tech POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:hi-tech>/instructions"
)
add_custom_command(TARGET hi-tech POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/instructions" "$<TARGET_FILE_DIR:hi-tech>/instructions"
)

# Создаем папку для DataMatrix
add_custom_command(TARGET hi-tech POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:hi-tech>/datamatrix"
)

# Копируем Python-скрипт рядом с .exe
add_custom_command(TARGET hi-tech POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/generateDataMatrix.py" "$<TARGET_FILE_DIR:hi-tech>/generateDataMatrix.py"
)