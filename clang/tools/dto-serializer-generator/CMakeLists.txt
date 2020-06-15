set(LLVM_LINK_COMPONENTS support)

add_clang_tool(dto-serializer-generator
        main.cpp
        )

set(DTO_SERIALIZER_GENERATOR_LIB_DEPS
        clangBasic
        clangFormat
        clangRewrite
        clangTooling
        clangToolingCore
        )

set_target_properties(dto-serializer-generator PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO)

target_compile_definitions(dto-serializer-generator PRIVATE _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS)

clang_target_link_libraries(dto-serializer-generator
        PRIVATE
        ${DTO_SERIALIZER_GENERATOR_LIB_DEPS}
        ${CONAN_LIBS}
        )
