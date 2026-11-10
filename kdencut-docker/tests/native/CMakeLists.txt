cmake_minimum_required(VERSION 3.16)
project(KdenCutMobileChecks LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
find_package(Qt6 REQUIRED COMPONENTS Widgets Test)
add_executable(check_mobile_workspace
    check_mobile_workspace.cpp
    ../../../src/kdencut/mobileworkspace.cpp
    ../../../src/kdencut/mobileworkspace.h)
target_link_libraries(check_mobile_workspace PRIVATE Qt6::Widgets Qt6::Test)
find_package(KDDockWidgets-qt6 2.4.0 QUIET)
if(TARGET KDAB::kddockwidgets)
    target_link_libraries(check_mobile_workspace PRIVATE KDAB::kddockwidgets)
    target_compile_definitions(check_mobile_workspace PRIVATE CHECK_KDDOCK)
endif()
enable_testing()
add_test(NAME mobile_workspace COMMAND check_mobile_workspace)
set_tests_properties(mobile_workspace PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
