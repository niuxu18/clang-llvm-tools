
add_clang_executable(find-func-decls FindFunctionDecls.cpp)
target_link_libraries(find-func-decls clangTooling)