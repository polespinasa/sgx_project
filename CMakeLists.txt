cmake_minimum_required(VERSION 3.16)
project(SGXProject)

# ---------------- VCPKG (host-only) ----------------
# Enable by configuring with:
#   -DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
option(ENABLE_VCPKG_HOST "Enable vcpkg packages for the untrusted host app" ON)

# ---------------- SGX SDK Settings ----------------
if(NOT DEFINED ENV{SGX_SDK})
    set(SGX_SDK "/opt/intel/sgxsdk")
else()
    set(SGX_SDK $ENV{SGX_SDK})
endif()

# SGX Mode (HW or SIM)
if(NOT DEFINED SGX_MODE)
    if(DEFINED ENV{SGX_MODE})
        set(SGX_MODE $ENV{SGX_MODE})
    else()
        set(SGX_MODE "HW")
    endif()
endif()

# SGX Debug flag
if(NOT DEFINED SGX_DEBUG)
    if(DEFINED ENV{SGX_DEBUG})
        set(SGX_DEBUG $ENV{SGX_DEBUG})
    else()
        set(SGX_DEBUG "1")
    endif()
endif()

# SGX Prerelease flag
if(NOT DEFINED SGX_PRERELEASE)
    if(DEFINED ENV{SGX_PRERELEASE})
        set(SGX_PRERELEASE $ENV{SGX_PRERELEASE})
    else()
        set(SGX_PRERELEASE "0")
    endif()
endif()

# Sanity: debug vs prerelease
if(SGX_DEBUG EQUAL 1 AND SGX_PRERELEASE EQUAL 1)
    message(FATAL_ERROR "Cannot set SGX_DEBUG and SGX_PRERELEASE at the same time!")
endif()

# Detect architecture
execute_process(COMMAND getconf LONG_BIT OUTPUT_VARIABLE LONG_BIT OUTPUT_STRIP_TRAILING_WHITESPACE)
if(LONG_BIT EQUAL 32 OR CMAKE_CXX_FLAGS MATCHES "-m32")
    set(SGX_ARCH "x86")
else()
    set(SGX_ARCH "x64")
endif()

# Arch-specific tools and lib paths
if(SGX_ARCH STREQUAL "x86")
    set(SGX_ARCH_FLAGS "-m32")
    set(SGX_LIBRARY_PATH "${SGX_SDK}/lib")
    set(SGX_ENCLAVE_SIGNER "${SGX_SDK}/bin/x86/sgx_sign")
    set(SGX_EDGER8R       "${SGX_SDK}/bin/x86/sgx_edger8r")
else()
    set(SGX_ARCH_FLAGS "-m64")
    set(SGX_LIBRARY_PATH "${SGX_SDK}/lib64")
    set(SGX_ENCLAVE_SIGNER "${SGX_SDK}/bin/x64/sgx_sign")
    set(SGX_EDGER8R       "${SGX_SDK}/bin/x64/sgx_edger8r")
endif()

# Common compile flags (as lists)
set(SGX_COMMON_FLAGS_LIST
    ${SGX_ARCH_FLAGS}
    -Wall -Wextra -Winit-self -Wpointer-arith -Wreturn-type -Waddress
    -Wsequence-point -Wformat-security -Wmissing-include-dirs
    -Wfloat-equal -Wundef -Wshadow -Wcast-align -Wcast-qual
    -Wconversion -Wredundant-decls
)
if(SGX_DEBUG EQUAL 1)
    list(APPEND SGX_COMMON_FLAGS_LIST -O0 -g)
else()
    list(APPEND SGX_COMMON_FLAGS_LIST -O2)
endif()

set(SGX_COMMON_CFLAGS_LIST ${SGX_COMMON_FLAGS_LIST}
    -Wjump-misses-init -Wstrict-prototypes -Wunsuffixed-float-constants
)
# IMPORTANT: no -std here; set per-target (host: C++20, enclave: C++11)
set(SGX_COMMON_CXXFLAGS_LIST ${SGX_COMMON_FLAGS_LIST}
    -Wnon-virtual-dtor
)

# Build mode banner
if(SGX_MODE STREQUAL "HW")
    if(SGX_DEBUG EQUAL 1)
        set(BUILD_MODE "HW_DEBUG")
    elseif(SGX_PRERELEASE EQUAL 1)
        set(BUILD_MODE "HW_PRERELEASE")
    else()
        set(BUILD_MODE "HW_RELEASE")
    endif()
else()
    if(SGX_DEBUG EQUAL 1)
        set(BUILD_MODE "SIM_DEBUG")
    elseif(SGX_PRERELEASE EQUAL 1)
        set(BUILD_MODE "SIM_PRERELEASE")
    else()
        set(BUILD_MODE "SIM_RELEASE")
    endif()
endif()
message(STATUS "Build mode: ${BUILD_MODE}")

# Lib names by mode (trusted + untrusted)
if(SGX_MODE STREQUAL "HW")
    set(URTS_LIB_NAME "sgx_urts")                 # untrusted runtime
    set(TRTS_LIB_NAME "sgx_trts")                 # trusted runtime
    set(SERVICE_LIB_NAME "sgx_tservice")          # trusted service
    set(UAESERVICE_LIB_NAME "sgx_uae_service")    # untrusted service
else()
    set(URTS_LIB_NAME "sgx_urts_sim")
    set(TRTS_LIB_NAME "sgx_trts_sim")
    set(SERVICE_LIB_NAME "sgx_tservice_sim")
    set(UAESERVICE_LIB_NAME "sgx_uae_service_sim")
endif()
set(UKEYEX_LIB_NAME "sgx_ukey_exchange")          # untrusted key-exchange (no _sim variant)
set(CRYPTO_LIB_NAME "sgx_tcrypto")                # trusted crypto

# Output names
set(APP_NAME "app")
set(ENCLAVE_NAME "enclave.so")
set(SIGNED_ENCLAVE_NAME "enclave.signed.so")
set(ENCLAVE_CONFIG_FILE "${CMAKE_CURRENT_SOURCE_DIR}/Enclave/Enclave.config.xml")

# ---------------- Host App (untrusted) ----------------
set(APP_C_FLAGS_LIST -fPIC -Wno-attributes)
if(SGX_DEBUG EQUAL 1)
    list(APPEND APP_C_FLAGS_LIST -DDEBUG -UNDEBUG -UEDEBUG)
elseif(SGX_PRERELEASE EQUAL 1)
    list(APPEND APP_C_FLAGS_LIST -DNDEBUG -DEDEBUG -UDEBUG)
else()
    list(APPEND APP_C_FLAGS_LIST -DNDEBUG -UEDEBUG -UDEBUG)
endif()

# Untrusted EDL codegen → Enclave_u.c/h
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/App/Enclave_u.h
           ${CMAKE_CURRENT_SOURCE_DIR}/App/Enclave_u.c
    COMMAND ${SGX_EDGER8R} --untrusted ${CMAKE_CURRENT_SOURCE_DIR}/Enclave/Enclave.edl
            --search-path ${CMAKE_CURRENT_SOURCE_DIR}/Enclave
            --search-path ${SGX_SDK}/include
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/App
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Enclave/Enclave.edl
    BYPRODUCTS ${CMAKE_CURRENT_SOURCE_DIR}/App/Enclave_u.h
               ${CMAKE_CURRENT_SOURCE_DIR}/App/Enclave_u.c
    COMMENT "Generating untrusted proxy files (Enclave_u.*)"
    VERBATIM
)

# OBJECT lib for untrusted stubs (lets us target-scope includes/flags)
add_library(app_generated_objs OBJECT
    ${CMAKE_CURRENT_SOURCE_DIR}/App/Enclave_u.c
)
target_include_directories(app_generated_objs PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/App
    ${SGX_SDK}/include
)
target_compile_options(app_generated_objs PRIVATE
    ${SGX_COMMON_CFLAGS_LIST} ${APP_C_FLAGS_LIST}
)

# Host executable (includes controller)
add_executable(${APP_NAME}
    ${CMAKE_CURRENT_SOURCE_DIR}/App/App.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/App/controllers/server_key.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/App/controllers/utilities.cpp
    $<TARGET_OBJECTS:app_generated_objs>
)
target_include_directories(${APP_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/App
    ${CMAKE_CURRENT_SOURCE_DIR}/App/controllers
    ${SGX_SDK}/include
)
target_compile_options(${APP_NAME} PRIVATE
    ${SGX_COMMON_CXXFLAGS_LIST} ${APP_C_FLAGS_LIST}
)
# Host C++ standard = C++20 (no GNU extensions)
set_target_properties(${APP_NAME} PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED YES
    CXX_EXTENSIONS NO
)

# ---- Link SGX untrusted libs (fixes undefined reference to sgx_calc_sealed_data_size)
find_library(SGX_URTS         ${URTS_LIB_NAME}         HINTS ${SGX_LIBRARY_PATH} REQUIRED)
find_library(SGX_UAE_SERVICE  ${UAESERVICE_LIB_NAME}   HINTS ${SGX_LIBRARY_PATH} REQUIRED)
find_library(SGX_UKEY_EXCHANGE ${UKEYEX_LIB_NAME}      HINTS ${SGX_LIBRARY_PATH})
# Note: sgx_ukey_exchange may be optional; if not found we just don't link it.

target_link_libraries(${APP_NAME} PRIVATE
    ${SGX_URTS}
    ${SGX_UAE_SERVICE}
    pthread
)
if(SGX_UKEY_EXCHANGE)
    target_link_libraries(${APP_NAME} PRIVATE ${SGX_UKEY_EXCHANGE})
endif()

# (Optional) vcpkg packages for host app only
if(ENABLE_VCPKG_HOST)
    # Example (commented): fmt + mbedtls
    # find_package(fmt CONFIG QUIET)
    # if(fmt_FOUND)
    #     target_link_libraries(${APP_NAME} PRIVATE fmt::fmt)
    #     target_compile_definitions(${APP_NAME} PRIVATE HAVE_FMT=1)
    # endif()
    # find_package(MbedTLS CONFIG QUIET)
    # if(MbedTLS_FOUND)
    #     target_link_libraries(${APP_NAME} PRIVATE MbedTLS::mbedtls MbedTLS::mbedx509 MbedTLS::mbedcrypto)
    #     target_compile_definitions(${APP_NAME} PRIVATE HAVE_MBEDTLS=1)
    # endif()
endif()

# ---------------- Enclave (trusted) ----------------
# Enclave compile flags
set(ENCLAVE_C_FLAGS_LIST
    -nostdinc -fvisibility=hidden -fpie -ffunction-sections -fdata-sections
)
execute_process(COMMAND ${CMAKE_C_COMPILER} -dumpversion
                OUTPUT_VARIABLE GCC_VERSION OUTPUT_STRIP_TRAILING_WHITESPACE)
if(GCC_VERSION VERSION_LESS "4.9")
    list(APPEND ENCLAVE_C_FLAGS_LIST -fstack-protector)
else()
    list(APPEND ENCLAVE_C_FLAGS_LIST -fstack-protector-strong)
endif()
set(ENCLAVE_CXX_FLAGS_LIST ${ENCLAVE_C_FLAGS_LIST} -nostdinc++)

# Link flags for enclave final link
set(ENCLAVE_LINK_FLAGS_LIST
    -Wl,-z,relro,-z,now,-z,noexecstack
    -Wl,--no-undefined
    -nostdlib -nodefaultlibs -nostartfiles
    -L${SGX_LIBRARY_PATH}
    -Wl,--whole-archive -l${TRTS_LIB_NAME} -Wl,--no-whole-archive
    -Wl,--start-group -lsgx_tstdc -lsgx_tcxx -l${CRYPTO_LIB_NAME} -l${SERVICE_LIB_NAME} -Wl,--end-group
    -Wl,-Bstatic -Wl,-Bsymbolic -Wl,--no-undefined
    -Wl,-pie,-eenclave_entry -Wl,--export-dynamic -Wl,--defsym,__ImageBase=0
    -Wl,--gc-sections
    -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/Enclave/Enclave.lds
)

# Trusted EDL codegen → Enclave_t.c/h
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/Enclave/Enclave_t.h
           ${CMAKE_CURRENT_SOURCE_DIR}/Enclave/Enclave_t.c
    COMMAND ${SGX_EDGER8R} --trusted ${CMAKE_CURRENT_SOURCE_DIR}/Enclave/Enclave.edl
            --search-path ${CMAKE_CURRENT_SOURCE_DIR}/Enclave
            --search-path ${SGX_SDK}/include
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/Enclave
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/Enclave/Enclave.edl
    BYPRODUCTS ${CMAKE_CURRENT_SOURCE_DIR}/Enclave/Enclave_t.h
               ${CMAKE_CURRENT_SOURCE_DIR}/Enclave/Enclave_t.c
    COMMENT "Generating trusted proxy files (Enclave_t.*)"
    VERBATIM
)

# OBJECT libraries (trusted stubs, core, third-party)
add_library(enclave_trusted_objs OBJECT
    ${CMAKE_CURRENT_SOURCE_DIR}/Enclave/Enclave_t.c
)
target_include_directories(enclave_trusted_objs PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Enclave
    ${CMAKE_CURRENT_SOURCE_DIR}/Enclave/libraries
    ${SGX_SDK}/include
    ${SGX_SDK}/include/tlibc
)
target_compile_options(enclave_trusted_objs PRIVATE
    ${SGX_COMMON_CFLAGS_LIST} ${ENCLAVE_C_FLAGS_LIST}
)

add_library(enclave_core_objs OBJECT
    ${CMAKE_CURRENT_SOURCE_DIR}/Enclave/Enclave.cpp
)
target_include_directories(enclave_core_objs PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Enclave
    ${CMAKE_CURRENT_SOURCE_DIR}/Enclave/libraries
    ${SGX_SDK}/include
    ${SGX_SDK}/include/tlibc
    ${SGX_SDK}/include/libcxx
)
# Enclave C++ standard = C++11 (explicit, matches sgx_tcxx expectations)
target_compile_options(enclave_core_objs PRIVATE
    ${SGX_COMMON_CXXFLAGS_LIST} ${ENCLAVE_CXX_FLAGS_LIST} -std=c++11
)

add_library(enclave_thirdparty_objs OBJECT
    ${CMAKE_CURRENT_SOURCE_DIR}/Enclave/libraries/monocypher.c
)
target_include_directories(enclave_thirdparty_objs PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/Enclave
    ${CMAKE_CURRENT_SOURCE_DIR}/Enclave/libraries
    ${SGX_SDK}/include
    ${SGX_SDK}/include/tlibc
)
target_compile_options(enclave_thirdparty_objs PRIVATE
    ${SGX_COMMON_CFLAGS_LIST} ${ENCLAVE_C_FLAGS_LIST}
)

# Enclave link (custom command consuming the OBJECT libs)
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${ENCLAVE_NAME}
    COMMAND ${CMAKE_CXX_COMPILER}
            $<TARGET_OBJECTS:enclave_trusted_objs>
            $<TARGET_OBJECTS:enclave_core_objs>
            $<TARGET_OBJECTS:enclave_thirdparty_objs>
            -o ${CMAKE_CURRENT_BINARY_DIR}/${ENCLAVE_NAME}
            ${ENCLAVE_LINK_FLAGS_LIST}
    DEPENDS enclave_trusted_objs enclave_core_objs enclave_thirdparty_objs
    COMMENT "Linking enclave (${ENCLAVE_NAME})"
    VERBATIM
)

# Convenience target for the unsigned enclave .so
add_custom_target(enclave_unsigned ALL
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${ENCLAVE_NAME}
)

# Enclave signing
if(BUILD_MODE STREQUAL "HW_RELEASE")
    # No auto-signing in HW_RELEASE
    add_custom_target(enclave ALL
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${ENCLAVE_NAME}
        COMMENT "Enclave built but not signed. Sign manually with: ${SGX_ENCLAVE_SIGNER} sign -key <key> -enclave ${ENCLAVE_NAME} -out ${SIGNED_ENCLAVE_NAME} -config ${ENCLAVE_CONFIG_FILE}"
    )
else()
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${SIGNED_ENCLAVE_NAME}
        COMMAND ${SGX_ENCLAVE_SIGNER} sign
                -key ${CMAKE_CURRENT_SOURCE_DIR}/Enclave/Enclave_private_test.pem
                -enclave ${CMAKE_CURRENT_BINARY_DIR}/${ENCLAVE_NAME}
                -out ${CMAKE_CURRENT_BINARY_DIR}/${SIGNED_ENCLAVE_NAME}
                -config ${ENCLAVE_CONFIG_FILE}
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${ENCLAVE_NAME}
        COMMENT "Signing enclave with test key"
        VERBATIM
    )
    add_custom_target(enclave ALL
        DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/${SIGNED_ENCLAVE_NAME}
    )
endif()

# ---------------- Aggregate / Utility targets ----------------
# Single command to build both host app and (signed) enclave
add_custom_target(sgx_build_all
    DEPENDS ${APP_NAME} enclave
)

# Clean (does NOT trigger a build)
add_custom_target(clean-all
    # Host app (single-config + multi-config path)
    COMMAND ${CMAKE_COMMAND} -E remove
            ${CMAKE_CURRENT_BINARY_DIR}/${APP_NAME}
            ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/${APP_NAME}
    # Enclave artifacts and generated sources:
    COMMAND ${CMAKE_COMMAND} -E remove
            ${CMAKE_CURRENT_BINARY_DIR}/${ENCLAVE_NAME}
            ${CMAKE_CURRENT_BINARY_DIR}/${SIGNED_ENCLAVE_NAME}
            ${CMAKE_CURRENT_SOURCE_DIR}/App/Enclave_u.h
            ${CMAKE_CURRENT_SOURCE_DIR}/App/Enclave_u.c
            ${CMAKE_CURRENT_SOURCE_DIR}/Enclave/Enclave_t.h
            ${CMAKE_CURRENT_SOURCE_DIR}/Enclave/Enclave_t.c
    COMMENT "Cleaning all generated files"
    VERBATIM
)

# Run target (portable path to the host app)
if(NOT BUILD_MODE STREQUAL "HW_RELEASE")
    add_custom_target(run
        COMMAND $<TARGET_FILE:${APP_NAME}>
        DEPENDS ${APP_NAME} enclave
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Running ${APP_NAME} [${SGX_MODE}|${SGX_ARCH}]"
    )
endif()

# ---------------- Print configuration ----------------
message(STATUS "SGX_SDK: ${SGX_SDK}")
message(STATUS "SGX_MODE: ${SGX_MODE}")
message(STATUS "SGX_ARCH: ${SGX_ARCH}")
message(STATUS "SGX_DEBUG: ${SGX_DEBUG}")
message(STATUS "BUILD_MODE: ${BUILD_MODE}")
