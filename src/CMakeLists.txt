set(ABSL_PROPAGATE_CXX_STD ON)

include(FetchContent)
FetchContent_Declare(
        gRPC
        GIT_REPOSITORY https://github.com/grpc/grpc
        GIT_TAG        v1.45.0
)
set(FETCHCONTENT_QUIET OFF)
FetchContent_MakeAvailable(gRPC)

set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:grpc_cpp_plugin>)

add_subdirectory(proto)


add_library(MyLibHeaders INTERFACE)
target_include_directories(MyLibHeaders INTERFACE include)
add_library(MyLib helper.cpp)
target_link_libraries(MyLib PUBLIC MyLibHeaders)

add_library(HashLibHeaders INTERFACE)
target_include_directories(HashLibHeaders INTERFACE include)
add_library(HashLib gthash.cpp)
target_link_libraries(HashLib PUBLIC HashLibHeaders)

add_executable(client client.cpp)
target_link_libraries(client
        PRIVATE
        HashLib
        project_options
        project_warnings
        demo_grpc_proto
        )
target_include_directories(client PUBLIC include)

add_executable(storage storage.cpp)
target_link_libraries(storage
        PRIVATE
        MyLib
        HashLib
        project_options
        project_warnings
        demo_grpc_proto
        )
target_include_directories(storage PUBLIC include)

add_executable(manager manager.cpp)
target_link_libraries(manager
        PRIVATE
        MyLib
        HashLib
        project_options
        project_warnings
        demo_grpc_proto
        )
target_include_directories(manager PUBLIC include)

add_executable(test_app test_app.cpp)
target_link_libraries(test_app
        PRIVATE
        MyLib
        HashLib
        project_options
        project_warnings
        demo_grpc_proto
        )
target_include_directories(test_app PUBLIC include)
