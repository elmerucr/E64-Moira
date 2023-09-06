add_library(machine STATIC machine.cpp)

target_link_libraries(machine blitter cia lua m68k mmu sound timer)
