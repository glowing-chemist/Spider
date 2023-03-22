cmake_minimum_required(VERSION 3.10)
project(Shader-Reflector)

set(SOURCE
    main.cpp
    ShaderCompiler.cpp
    ${CMAKE_SOURCE_DIR}/ThirdParty/Spirv-Reflect/spirv_reflect.cpp)

add_executable(Shader-Reflector ${SOURCE})
target_link_libraries(Shader-Reflector dxcompiler)

target_include_directories(Shader-Reflector PUBLIC ${CMAKE_SOURCE_DIR}/ThirdParty/Spirv-Reflect/)
target_compile_definitions(Shader-Reflector PUBLIC -DSPIRV_REFLECT_USE_SYSTEM_SPIRV_H)
