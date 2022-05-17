#include "Enclave_t.h"
#include <stdio.h>
#include <string.h>

#include "libraries/monocypher.h"

#include "sgx_trts.h"
#include "sgx_tseal.h"

int trusted_func01()
{
    int trusted_x = 987654321;
    ocall_wait_keyinput("Please enter keyboard to show variables in memory ...");
    return trusted_x;
}

char* data_to_hex(uint8_t* in, size_t insz)
{
  char* out = (char*) malloc(insz * 2 + 1);
  uint8_t* pin = in;
  const char * hex = "0123456789abcdef";
  char* pout = out;
  for(; pin < in + insz; pout += 2, pin++){
    pout[0] = hex[(*pin>>4) & 0xF];
    pout[1] = hex[ *pin     & 0xF];
  }
  pout[0] = 0;
  return out;
}

sgx_status_t enclave_seal_data(uint8_t *privkey, size_t privkey_len,
                               uint8_t *pubkey, size_t pubkey_len,
                               uint8_t *sealed_data, size_t sealed_buf_size,
                               size_t *actual_sealed_size)
{
    sgx_status_t status = SGX_ERROR_UNEXPECTED;
    
    // Calculate total data size to seal
    size_t total_data_size = privkey_len + pubkey_len;
    
    // Calculate required sealed data size
    uint32_t required_sealed_size = sgx_calc_sealed_data_size(0, (uint32_t)total_data_size);
    if (required_sealed_size == UINT32_MAX) {
        return SGX_ERROR_INVALID_PARAMETER;
    }
    
    // Check if provided buffer is large enough
    if (sealed_buf_size < required_sealed_size) {
        *actual_sealed_size = required_sealed_size;  // Tell caller the required size
        return SGX_ERROR_INVALID_PARAMETER;
    }
    
    // Combine private and public keys into one buffer
    uint8_t *combined_data = (uint8_t*)malloc(total_data_size);
    if (combined_data == NULL) {
        return SGX_ERROR_OUT_OF_MEMORY;
    }
    
    // Copy private key and public key to combined buffer
    memcpy(combined_data, privkey, privkey_len);
    memcpy(combined_data + privkey_len, pubkey, pubkey_len);
    
    // Seal the data into the provided buffer
    status = sgx_seal_data(0, NULL,                     // No additional authenticated data
                          (uint32_t)total_data_size,    // Data to encrypt length
                          combined_data,                 // Data to encrypt
                          required_sealed_size,          // Sealed data buffer size
                          (sgx_sealed_data_t*)sealed_data); // Sealed data buffer
    
    // Clean up
    memset(combined_data, 0, total_data_size);  // Clear sensitive data
    free(combined_data);
    
    if (status == SGX_SUCCESS) {
        *actual_sealed_size = required_sealed_size;
    } else {
        *actual_sealed_size = 0;
    }
    
    return status;
}

sgx_status_t enclave_generate_new_keypair(uint8_t *sealed_keypair, size_t sealed_buf_size, 
                                          size_t *actual_sealed_size)
{
    sgx_status_t status = SGX_ERROR_UNEXPECTED;
    uint8_t x25519_privkey[32];
    uint8_t x25519_public_key[32];
    
    // Validate input parameters
    if (sealed_keypair == NULL || actual_sealed_size == NULL) {
        return SGX_ERROR_INVALID_PARAMETER;
    }
    
    // Calculate required sealed data size first
    uint32_t required_size = sgx_calc_sealed_data_size(0, 64); // 32 + 32 bytes for keys
    if (required_size == UINT32_MAX) {
        return SGX_ERROR_UNEXPECTED;
    }
    
    // Check if buffer is large enough
    if (sealed_buf_size < required_size) {
        *actual_sealed_size = required_size;  // Inform caller of required size
        return SGX_ERROR_INVALID_PARAMETER;
    }
    
    // Generate random private key
    status = sgx_read_rand(x25519_privkey, 32);
    if (status != SGX_SUCCESS) {
        return status;
    }
    
    // Generate public key from private key
    crypto_x25519_public_key(x25519_public_key, x25519_privkey);
    
    // Optional: Generate hex representation for logging/debugging
    char* x25519_privkey_hex = data_to_hex(x25519_privkey, sizeof(x25519_privkey));
    ocall_print_string("1. private key:");
    ocall_print_string(x25519_privkey_hex);

    char* x25519_public_key_hex = data_to_hex(x25519_public_key, sizeof(x25519_public_key));
    ocall_print_string("2. public key:");
    ocall_print_string(x25519_public_key_hex);
    
    // Seal the keypair into the provided buffer
    status = enclave_seal_data(x25519_privkey, sizeof(x25519_privkey),
                               x25519_public_key, sizeof(x25519_public_key),
                               sealed_keypair, sealed_buf_size,
                               actual_sealed_size);
    
    // Clear sensitive data from memory
    memset(x25519_privkey, 0, sizeof(x25519_privkey));
    
    // Free hex string if it was allocated
    if (x25519_public_key_hex != NULL) {
        free(x25519_public_key_hex);
    }
    
    return status;
}

sgx_status_t enclave_unseal_keypair(uint8_t *sealed_data, size_t sealed_size,
                                    uint8_t *privkey, uint8_t *pubkey)
{
    sgx_status_t status = SGX_ERROR_UNEXPECTED;
    uint32_t decrypted_data_len = 64;  // 32 bytes privkey + 32 bytes pubkey
    uint8_t decrypted_data[64];
    
    // Unseal the data
    status = sgx_unseal_data((sgx_sealed_data_t*)sealed_data,
                            NULL, NULL,  // No additional authenticated data
                            decrypted_data, &decrypted_data_len);
    
    if (status == SGX_SUCCESS) {
        // Extract private and public keys
        memcpy(privkey, decrypted_data, 32);
        memcpy(pubkey, decrypted_data + 32, 32);

        // Optional: Generate hex representation for logging/debugging
        char* privkey_hex = data_to_hex(privkey, 32);
        ocall_print_string("3. private key:");
        ocall_print_string(privkey_hex);

        char* pubkey_hex = data_to_hex(pubkey, 32);
        ocall_print_string("4. public key:");
        ocall_print_string(pubkey_hex);
        
        // Clear sensitive data
        memset(decrypted_data, 0, sizeof(decrypted_data));
    }
    
    return status;
}
