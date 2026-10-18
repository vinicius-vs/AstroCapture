cmake_minimum_required(VERSION 3.21)

project(AstroCapture VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets)

add_executable(astrocapture
    src/app/main.cpp
    src/presentation/main_window.cpp
    src/presentation/main_window.h
)

target_include_directories(astrocapture PRIVATE src)
target_link_libraries(astrocapture PRIVATE Qt6::Widgets)
target_compile_definitions(astrocapture PRIVATE
    ASTROCAPTURE_VERSION="${PROJECT_VERSION}"
)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(astrocapture PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)

if(BUILD_TESTING)
    find_package(Qt6 6.2 REQUIRED COMPONENTS Test)

    add_executable(astrocapture_tests
        src/presentation/main_window.cpp
        src/presentation/main_window.h
        tests/unit/test_application_metadata.cpp
    )

    target_include_directories(astrocapture_tests PRIVATE src)
    target_link_libraries(astrocapture_tests PRIVATE Qt6::Test Qt6::Widgets)
    target_compile_definitions(astrocapture_tests PRIVATE
        ASTROCAPTURE_VERSION="${PROJECT_VERSION}"
    )

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(astrocapture_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME astrocapture_tests COMMAND astrocapture_tests)
    set_tests_properties(astrocapture_tests PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen"
    )
endif()
