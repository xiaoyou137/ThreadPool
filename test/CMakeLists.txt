#设置编译源文件
aux_source_directory(. TEST_LIST)
aux_source_directory(../src SRC_LIST)

#生成可执行文件
add_executable(test ${TEST_LIST} ${SRC_LIST})

#链接时依赖的库文件
target_link_libraries(test pthread)