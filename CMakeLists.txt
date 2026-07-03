cmake_minimum_required(VERSION 3.16)
project(MusicPlayer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS
    Core
    Gui
    Multimedia
    Network
    Sql
    Widgets
)

qt_standard_project_setup()

set(PROJECT_HEADERS
    include/core/MusicTrack.h
    include/core/PlaybackController.h
    include/data/MusicLibrary.h
    include/providers/LocalProvider.h
    include/providers/MusicProvider.h
    include/providers/ProviderRegistry.h
    include/ui/MainWindow.h
)

set(PROJECT_SOURCES
    src/core/PlaybackController.cpp
    src/data/MusicLibrary.cpp
    src/main.cpp
    src/providers/LocalProvider.cpp
    src/providers/ProviderRegistry.cpp
    src/ui/MainWindow.cpp
)

set(PROJECT_RESOURCES
    resources/MusicPlayer.qrc
)

set(PROJECT_UI_REFERENCES
    ui/MainWindow.ui
)

set(PROJECT_FILES
    ${PROJECT_HEADERS}
    ${PROJECT_SOURCES}
    ${PROJECT_RESOURCES}
    ${PROJECT_UI_REFERENCES}
)

set_source_files_properties(${PROJECT_UI_REFERENCES}
    PROPERTIES
        HEADER_FILE_ONLY true
)

qt_add_executable(${PROJECT_NAME} ${PROJECT_FILES})

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(${PROJECT_NAME}
    PROPERTIES
        WIN32_EXECUTABLE false
        # 是否使用控制台窗口，false 是使用，true 是不使用
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Multimedia
        Qt6::Network
        Qt6::Sql
        Qt6::Widgets
)

source_group("Header Files\\Core" FILES include/core/MusicTrack.h include/core/PlaybackController.h)
source_group("Header Files\\Data" FILES include/data/MusicLibrary.h)
source_group("Header Files\\Providers" FILES include/providers/LocalProvider.h include/providers/MusicProvider.h include/providers/ProviderRegistry.h)
source_group("Header Files\\UI" FILES include/ui/MainWindow.h)
source_group("Source Files\\Core" FILES src/core/PlaybackController.cpp)
source_group("Source Files\\Data" FILES src/data/MusicLibrary.cpp)
source_group("Source Files\\Providers" FILES src/providers/LocalProvider.cpp src/providers/ProviderRegistry.cpp)
source_group("Source Files\\UI" FILES src/ui/MainWindow.cpp src/main.cpp)
source_group("Form Files" FILES ${PROJECT_UI_REFERENCES})
source_group("Resource Files" FILES ${PROJECT_RESOURCES})
