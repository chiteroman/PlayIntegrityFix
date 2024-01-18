add_executable(socket_example
  main.cc
  socket_example.cc
  )

target_link_libraries(socket_example
  dobby
  )


add_library(socket_example_x SHARED
  socket_example.cc
  )

target_link_libraries(socket_example_x
  dobby
  )