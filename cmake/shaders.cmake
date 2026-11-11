find_program(GLSLC_EXECUTABLE NAMES glslc
    HINTS "$ENV{VULKAN_SDK}/Bin"
          "D:/sdk/VulkanSDK/1.4.363.1/Bin")

function(vulkao_add_shaders target)
    if(NOT GLSLC_EXECUTABLE)
        message(FATAL_ERROR "glslc not found. Install Vulkan SDK or set VULKAN_SDK.")
    endif()

    if(NOT TARGET ${target})
        message(FATAL_ERROR "vulkao_add_shaders: target '${target}' does not exist.")
    endif()

    set(out_dir "${CMAKE_CURRENT_BINARY_DIR}/shaders")
    file(MAKE_DIRECTORY "${out_dir}")

    set(spv_files "")
    foreach(src IN LISTS ARGN)
        if(NOT EXISTS "${src}")
            message(FATAL_ERROR "Shader source not found: ${src}")
        endif()

        get_filename_component(fname "${src}" NAME)
        set(out "${out_dir}/${fname}.spv")

        add_custom_command(
            OUTPUT  "${out}"
            COMMAND ${GLSLC_EXECUTABLE} "${src}" -o "${out}"
            DEPENDS "${src}"
            COMMENT "glslc: ${src} -> ${out}"
            VERBATIM
        )
        list(APPEND spv_files "${out}")
    endforeach()

    add_custom_target(${target}_shaders DEPENDS ${spv_files})
    add_dependencies(${target} ${target}_shaders)

    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${spv_files}
                "$<TARGET_FILE_DIR:${target}>"
        COMMENT "Copy shaders -> $<TARGET_FILE_DIR:${target}>"
        VERBATIM
    )
endfunction()