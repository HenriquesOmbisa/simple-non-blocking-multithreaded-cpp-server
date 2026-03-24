if (NOT DEFINED STATIC_DIR)
    message(FATAL_ERROR "STATIC_DIR is required")
endif()

if (NOT DEFINED GZIP_EXECUTABLE)
    message(FATAL_ERROR "GZIP_EXECUTABLE is required")
endif()

if (NOT EXISTS "${STATIC_DIR}")
    message(FATAL_ERROR "Static directory not found: ${STATIC_DIR}")
endif()

# Compress only textual assets. Binary files (png/jpg/webp/...) are skipped.
set(COMPRESSIBLE_EXTENSIONS
    ".html"
    ".css"
    ".js"
    ".json"
    ".svg"
    ".txt"
    ".xml"
    ".csv"
)

file(GLOB_RECURSE STATIC_FILES RELATIVE "${STATIC_DIR}" "${STATIC_DIR}/*")

set(COMPRESSED_COUNT 0)

foreach(REL_PATH IN LISTS STATIC_FILES)
    set(SRC_FILE "${STATIC_DIR}/${REL_PATH}")

    if (IS_DIRECTORY "${SRC_FILE}")
        continue()
    endif()

    get_filename_component(EXT "${SRC_FILE}" EXT)
    string(TOLOWER "${EXT}" EXT_LOWER)

    list(FIND COMPRESSIBLE_EXTENSIONS "${EXT_LOWER}" EXT_INDEX)
    if (EXT_INDEX EQUAL -1)
        continue()
    endif()

    set(GZ_FILE "${SRC_FILE}.gz")
    set(SHOULD_COMPRESS FALSE)

    if (NOT EXISTS "${GZ_FILE}")
        set(SHOULD_COMPRESS TRUE)
    else()
        file(TIMESTAMP "${SRC_FILE}" SRC_TS UTC)
        file(TIMESTAMP "${GZ_FILE}" GZ_TS UTC)
        if (SRC_TS STRGREATER GZ_TS)
            set(SHOULD_COMPRESS TRUE)
        endif()
    endif()

    if (SHOULD_COMPRESS)
        execute_process(
            COMMAND "${GZIP_EXECUTABLE}" -n -k -f -9 "${SRC_FILE}"
            RESULT_VARIABLE GZIP_RESULT
            ERROR_VARIABLE GZIP_ERROR
            OUTPUT_QUIET
        )

        if (NOT GZIP_RESULT EQUAL 0)
            message(WARNING "Failed to compress ${SRC_FILE}: ${GZIP_ERROR}")
        else()
            math(EXPR COMPRESSED_COUNT "${COMPRESSED_COUNT} + 1")
            message(STATUS "Compressed ${REL_PATH} -> ${REL_PATH}.gz")
        endif()
    endif()
endforeach()

message(STATUS "Static precompression done. Updated files: ${COMPRESSED_COUNT}")
