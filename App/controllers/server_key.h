#pragma once

#ifndef SERVER_KEY_H
#define SERVER_KEY_H

#include <cstdint>
#include <vector>
#include <optional>
#include <string>

#include "../Enclave_u.h"

namespace server_key {
    // Return type using std::optional for better error handling
    std::optional<std::vector<uint8_t>> generate_sealed_keypair(sgx_enclave_id_t eid);

    // Function to write a vector of bytes to a file
    bool writeVectorToFile(const std::vector<uint8_t>& data, const std::string& filename);

    // Function to read a vector of bytes from a file
    std::vector<uint8_t> readVectorFromFile(const std::string& filename);
}

#endif // ENDPOINT_DEPOSIT_H