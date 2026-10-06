cmake_minimum_required(VERSION 3.5)

set(CMAKE_SYSTEM_NAME "Generic")
set(CMAKE_C_COMPILER "arm-vita-eabi-gcc")

project(iTLSKernel C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)
set(CMAKE_C_FLAGS "-Wl,-q -Wall -Wextra -O3")

include_directories(
)

link_directories(
	${CMAKE_CURRENT_BINARY_DIR}
)

add_executable(itls.elf
	main.c
	path_rewrite.c
)

target_link_libraries(itls.elf
	SceThreadmgrForDriver_stub
	SceSysclibForDriver_stub
	SceCpuForDriver_stub
	taihenModuleUtils_stub
	taihenForKernel_stub
	SceSysmemForDriver_stub
	SceIofilemgrForDriver_stub
	SceDebugForDriver_stub
	SceModulemgrForDriver_stub
	SceSysrootForDriver_stub
)

set_target_properties(itls.elf
	PROPERTIES LINK_FLAGS "-nostdlib"
	COMPILE_FLAGS "-D__VITA_KERNEL__"
)

add_custom_target(itls.skprx ALL
	COMMAND vita-elf-create -e ${CMAKE_SOURCE_DIR}/itls.yml itls.elf itls.velf
	COMMAND vita-make-fself -c itls.velf itls.skprx
)
add_dependencies(itls.skprx itls.elf)
