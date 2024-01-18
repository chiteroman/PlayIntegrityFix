add_library(objc_runtime_replace
  objc_runtime_replace.mm
  )

target_link_libraries(objc_runtime_replace
  "-framework Foundation"
  )