add_executable(socket_example
  main.cc
  socket_example.cc
  )

target_link_libraries(socket_example
  dobby
  logging
  )


add_library(socket_example_lib SHARED
  socket_example.cc
  )

target_link_libraries(socket_example_lib
  dobby
  )