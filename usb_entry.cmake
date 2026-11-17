# Include this file in the PS5 title project and bind the final executable.
function(dragoon_bind_usb_entry target)
    target_sources("${target}" PRIVATE
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/usb_entry.c"
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/usb_storage.c")
    target_compile_definitions("${target}" PRIVATE __BSD_VISIBLE=1)
    target_link_options("${target}" PRIVATE "LINKER:--wrap=main")
endfunction()
