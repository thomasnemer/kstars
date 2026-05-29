# kstars/ekos/mcp/CMakeLists.txt
target_sources(KStarsLib PRIVATE
    mcptransport.cpp
    mcptoolregistry.cpp
    mcpserver.cpp
    mcplogbridge.cpp
    tools/ekostools.cpp
    tools/mounttools.cpp
    tools/capturetools.cpp
    tools/guidetools.cpp
    tools/focustools.cpp
    tools/aligntools.cpp
    tools/schedulertools.cpp
    tools/inditools.cpp
)

target_include_directories(KStarsLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
