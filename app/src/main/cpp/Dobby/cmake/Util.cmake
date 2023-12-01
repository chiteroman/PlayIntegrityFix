# Check files list exist
function(check_files_exist CHECK_FILES)
    foreach(file ${CHECK_FILES})
        if(NOT EXISTS "${file}")
            message(FATAL_ERROR "${file} NOT EXISTS!")
        endif()
    endforeach()
endfunction(check_files_exist CHECK_FILES)

# Search suffix files
function(search_suffix_files suffix INPUT_VARIABLE OUTPUT_VARIABLE)
    set(ResultFiles )
    foreach(filePath ${${INPUT_VARIABLE}})
        # message(STATUS "[*] searching *.${suffix} from ${filePath}")
        file(GLOB files ${filePath}/*.${suffix})
        set(ResultFiles ${ResultFiles} ${files})
    endforeach()
    set(${OUTPUT_VARIABLE} ${ResultFiles} PARENT_SCOPE)
endfunction()
