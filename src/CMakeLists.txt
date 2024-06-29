#设置编译源文件
aux_source_directory(. SRC_LIST)

#生成可执行文件
add_library(threadpool STATIC ${SRC_LIST})

#链接时依赖的库文件
target_link_libraries(threadpool pthread)