#include <assert.h>
#include <stdexcept>
#include <stdio.h>
#include <string.h>

# include <unistd.h>
# include <pwd.h>
# define ENCLAVE_FILENAME "enclave.signed.so"

#include "sgx_urts.h"
#include "App.h"
#include "Enclave_u.h"

#include "controllers/server_key.h"
#include "controllers/server_management.h"

// for key_to_string - delete later
#include <cstddef>
#include <stdint.h>
#include <string_view>
#include <vector>
#include <iomanip>

sgx_enclave_id_t global_eid = 0;

/* ocall functions (untrusted) */
void ocall_wait_keyinput(const char *str)
{
    printf("%s", str);
    getchar();
}

void ocall_print_string(const char *str)
{
    printf("%s\n", str);
}


std::string key_to_string(const unsigned char* key, size_t keylen) {
    std::stringstream sb;
    sb << "0x";
    for (int i = 0; i < keylen; i++)
        sb << std::hex << std::setw(2) << std::setfill('0') << (int)key[i];
    return sb.str();
}

/* application entry */
int SGX_CDECL main(int argc, char *argv[])
{
    (void)(argc);
    (void)(argv);

    sgx_status_t ret = SGX_ERROR_UNEXPECTED;

    int untrusted_x = 123456789;

    // initialize enclave
    ret = sgx_create_enclave(ENCLAVE_FILENAME, SGX_DEBUG_FLAG, NULL, NULL, &global_eid, NULL);
    if (ret != SGX_SUCCESS) {
        printf("Enclave init error\n");
        getchar();
        return -1;
    }
 
    // invoke trusted_func01();
    int returned_result;
    ret = trusted_func01(global_eid, &returned_result);
    if (ret != SGX_SUCCESS) {
        printf("Enclave call error\n");
        return -1;
    }

    if (auto sealed_data = server_key::generate_sealed_keypair(global_eid)) {
        // Successfully generated
        // save_to_file(sealed_data->data(), sealed_data->size());
        auto sealed_data_hex = key_to_string(sealed_data->data(), sealed_data->size());
        printf("Sealed data 1: %s\n", sealed_data_hex.c_str());

       server_key::writeVectorToFile(sealed_data.value(), "./sealed.key");

        auto sealed_data_from_file = server_key::readVectorFromFile("./sealed.key");

        auto sealed_data_from_file_hex = key_to_string(sealed_data_from_file.data(), sealed_data_from_file.size());
        printf("Sealed data 2: %s\n", sealed_data_from_file_hex.c_str());

        // Unseal

        std::array<uint8_t, 32> privkey;
        std::array<uint8_t, 32> pubkey;

        sgx_status_t retval;
        sgx_status_t status = enclave_unseal_keypair(
            global_eid, 
            &retval,
            const_cast<uint8_t*>(sealed_data_from_file.data()),  // EDL expects non-const
            sealed_data_from_file.size(),
            privkey.data(),
            pubkey.data()
        );

    } else {
        // Handle error case
        throw std::runtime_error("Failed to generate sealed keypair");
    }

    // destroy the enclave
    sgx_destroy_enclave(global_eid);

    printf ("X (untrusted): %d\n", untrusted_x);
    printf ("X (trusted): %d\n", returned_result);

    server_management::start_server();

    return 0;
}

