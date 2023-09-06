add_subdirectory(Moira)

add_library(m68k STATIC m68k.cpp)

target_link_libraries(m68k Moira)
