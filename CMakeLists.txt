cmake_minimum_required(VERSION 3.21)

project(AstroCapture VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.2 REQUIRED COMPONENTS Sql Widgets)

add_executable(astrocapture
    src/app/main.cpp
    src/domain/capture_entities.cpp
    src/domain/capture_entities.h
    src/domain/logging/log_event.h
    src/domain/logging/logging_service.h
    src/domain/repositories/capture_run_repository.h
    src/domain/repositories/repository_errors.h
    src/domain/repositories/session_repository.h
    src/infrastructure/persistence/sqlite_database.cpp
    src/infrastructure/persistence/sqlite_database.h
    src/infrastructure/persistence/sqlite_repositories.cpp
    src/infrastructure/persistence/sqlite_repositories.h
    src/infrastructure/logging/json_lines_logging_service.cpp
    src/infrastructure/logging/json_lines_logging_service.h
    src/infrastructure/storage/storage_layout.cpp
    src/infrastructure/storage/storage_layout.h
    src/presentation/main_window.cpp
    src/presentation/main_window.h
)

target_include_directories(astrocapture PRIVATE src)
target_link_libraries(astrocapture PRIVATE Qt6::Sql Qt6::Widgets)
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

    add_executable(astrocapture_domain_tests
        src/domain/capture_entities.cpp
        src/domain/capture_entities.h
        tests/unit/test_capture_entities.cpp
    )

    target_include_directories(astrocapture_domain_tests PRIVATE src)
    target_link_libraries(astrocapture_domain_tests PRIVATE Qt6::Test)

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(astrocapture_domain_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME astrocapture_domain_tests COMMAND astrocapture_domain_tests)

    add_executable(astrocapture_persistence_tests
        src/infrastructure/persistence/sqlite_database.cpp
        src/infrastructure/persistence/sqlite_database.h
        tests/unit/test_sqlite_database.cpp
    )

    target_include_directories(astrocapture_persistence_tests PRIVATE src)
    target_link_libraries(astrocapture_persistence_tests PRIVATE Qt6::Sql Qt6::Test)

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(astrocapture_persistence_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME astrocapture_persistence_tests COMMAND astrocapture_persistence_tests)

    add_executable(astrocapture_repository_tests
        src/domain/capture_entities.cpp
        src/domain/capture_entities.h
        src/domain/repositories/capture_run_repository.h
        src/domain/repositories/repository_errors.h
        src/domain/repositories/session_repository.h
        src/infrastructure/persistence/sqlite_database.cpp
        src/infrastructure/persistence/sqlite_database.h
        src/infrastructure/persistence/sqlite_repositories.cpp
        src/infrastructure/persistence/sqlite_repositories.h
        tests/unit/test_sqlite_repositories.cpp
    )

    target_include_directories(astrocapture_repository_tests PRIVATE src)
    target_link_libraries(astrocapture_repository_tests PRIVATE Qt6::Sql Qt6::Test)

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(astrocapture_repository_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME astrocapture_repository_tests COMMAND astrocapture_repository_tests)

    add_executable(astrocapture_logging_tests
        src/domain/capture_entities.cpp
        src/domain/capture_entities.h
        src/domain/logging/log_event.h
        src/domain/logging/logging_service.h
        src/infrastructure/logging/json_lines_logging_service.cpp
        src/infrastructure/logging/json_lines_logging_service.h
        tests/unit/test_logging_service.cpp
    )

    target_include_directories(astrocapture_logging_tests PRIVATE src)
    target_link_libraries(astrocapture_logging_tests PRIVATE Qt6::Test)

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(astrocapture_logging_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME astrocapture_logging_tests COMMAND astrocapture_logging_tests)

    add_executable(astrocapture_storage_tests
        src/domain/capture_entities.cpp
        src/domain/capture_entities.h
        src/infrastructure/storage/storage_layout.cpp
        src/infrastructure/storage/storage_layout.h
        tests/unit/test_storage_layout.cpp
    )

    target_include_directories(astrocapture_storage_tests PRIVATE src)
    target_link_libraries(astrocapture_storage_tests PRIVATE Qt6::Test)

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(astrocapture_storage_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME astrocapture_storage_tests COMMAND astrocapture_storage_tests)
endif()
