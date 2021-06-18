cmake_minimum_required(VERSION 2.8)

set(CMAKE_SYSTEM_NAME "Generic")
set(CMAKE_C_COMPILER "arm-vita-eabi-gcc")
set(CMAKE_CXX_COMPILER "arm-vita-eabi-g++")

project(HENkaku)

set(CMAKE_C_FLAGS "-Wl,-q -Wall -O3 -std=gnu99")
set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++11 -fno-rtti -fno-exceptions")

include_directories(
)

link_directories(
	${CMAKE_CURRENT_BINARY_DIR}
)

add_executable(itls.elf
	main.c
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
