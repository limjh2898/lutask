# CMakeList.txt : 최상위 CMake 프로젝트 파일, 전역 구성을 수행하고
# 여기에 하위 프로젝트를 포함합니다.
#
cmake_minimum_required (VERSION 3.8)

# 지원되는 경우 MSVC 컴파일러에 대해 핫 다시 로드 사용하도록 설정합니다.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  #set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project (lutask VERSION 0.0.1)

unset(_default_binfmt)

set(CMAKE_CXX_STANDARD 17)

add_compile_options(-fsanitize=address)
add_link_options(-fsanitize=address)

## ABI

set(_all_archs arm arm64 loongarch64 mips32 mips64 ppc32 ppc64 riscv64 s390x i386 x86_64 combined)

# Try at start to auto determine arch from CMake.
if(CMAKE_SYSTEM_PROCESSOR IN_LIST _all_archs)
  set(_default_arch ${CMAKE_SYSTEM_PROCESSOR})
elseif(_bits EQUAL 32)
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "^[Aa][Rr][Mm]")
    set(_default_arch arm)
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^mips")
    set(_default_arch mips32)
  else()
    set(_default_arch i386)
  endif()
else()
  if(CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64" OR
    CMAKE_SYSTEM_PROCESSOR MATCHES "^[Aa][Rr][Mm]") # armv8
    set(_default_arch arm64)
  elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^mips")
    set(_default_arch mips64)
  else()
    set(_default_arch x86_64)
  endif()
endif()

message("#### ${_default_arch}")

## Binary format

if(WIN32)
  set(_default_binfmt pe)
elseif(APPLE)
  set(_default_binfmt mach-o)
else()
  set(_default_binfmt elf)
endif()

## ABI

math(EXPR _bits "${CMAKE_SIZEOF_VOID_P}*8")

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^[Aa][Rr][Mm]" OR CMAKE_SYSTEM_PROCESSOR STREQUAL "aarch64")
  set(_default_abi aapcs)
elseif(WIN32)
  set(_default_abi ms)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^mips")
  if(_bits EQUAL 32)
    set(_default_abi o32)
  else()
    set(_default_abi n64)
  endif()
else()
  set(_default_abi sysv)
endif()

## Assembler type

if(MSVC)
  if(BOOST_CONTEXT_ARCHITECTURE STREQUAL arm64 OR BOOST_CONTEXT_ARCHITECTURE STREQUAL arm)
    set(_default_asm armasm)
  else()
    set(_default_asm masm)
  endif()
else()
  set(_default_asm gas)
endif()

## Assembler source suffix

if(BOOST_CONTEXT_BINARY_FORMAT STREQUAL pe)
  set(_default_ext .asm)
elseif(BOOST_CONTEXT_ASSEMBLER STREQUAL gas)
  set(_default_ext .S)
else()
  set(_default_ext .asm)
endif()

set(_default_impl fcontext)

message(STATUS "lutask: "
  "architecture ${_default_arch}, "
  "binary format ${_default_binfmt}, "
  "ABI ${_default_abi}, "
  "assembler ${_default_asm}, "
  "suffix ${_default_ext}, "
  "implementation ${_default_impl}")

set(_asm_suffix ${_default_arch}_${_default_abi}_${_default_binfmt}_${_default_asm}${_default_ext})


if(_default_impl STREQUAL "fcontext")
  if(_default_asm STREQUAL gas)
    if(CMAKE_CXX_PLATFORM_ID MATCHES "Cygwin")
      enable_language(ASM-ATT)
    else()
      enable_language(ASM)
    endif()
  elseif(_default_asm STREQUAL armasm)
    enable_language(ASM_ARMASM)
  else()
    enable_language(ASM_MASM)
  endif()
endif()


# Choose .asm sources

set(ASM_SOURCES
  src/asm/make_${_asm_suffix}
  src/asm/jump_${_asm_suffix}
  src/asm/ontop_${_asm_suffix}
)

if(_default_impl STREQUAL "fcontext")
  if(_default_asm STREQUAL masm AND _default_arch STREQUAL i386)
      set_source_files_properties(${ASM_SOURCES} PROPERTIES COMPILE_FLAGS "/safeseh")
  endif()

  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set_property(SOURCE ${ASM_SOURCES} APPEND PROPERTY COMPILE_OPTIONS "-x" "assembler-with-cpp")
  endif()
else()
endif()


message(STATUS "${ASM_SOURCES}")

include_directories(
    "include"
    "C:/Program Files (x86)/Visual Leak Detector/include"
)

link_directories(
    "C:/Program Files (x86)/Visual Leak Detector/lib/Win64"
)

file(GLOB TARGET_SOURCE 
    "src/context/windows/StackTraits.cpp"
    "src/schedule/*.cpp"
    "src/*.cpp")

add_library(lutask STATIC ${ASM_SOURCES} ${TARGET_SOURCE})

add_executable(simple "example/simple.cpp")
target_link_libraries(simple lutask)

add_executable(shared_work "example/shared_work.cpp")
target_link_libraries(shared_work lutask)

if(BUILD_SHARED_LIBS)
  target_compile_definitions(lutask PUBLIC BOOST_CONTEXT_DYN_LINK= BOOST_CONTEXT_EXPORT=EXPORT)
else()
  target_compile_definitions(lutask PUBLIC BOOST_CONTEXT_STATIC_LINK= BOOST_CONTEXT_EXPORT=)
endif()