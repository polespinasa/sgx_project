#include "server_key.h"

#include <vector>
#include <fstream>       // Add this for std::ofstream and std::ifstream
#include <iostream>
#include <tuple>
#include <optional>
#include <string>
#include <filesystem>

#include "../Enclave_u.h"
#include "utilities.h"

namespace server_key {
    
    std::optional<std::vector<uint8_t>> generate_sealed_keypair(sgx_enclave_id_t eid) {
        constexpr size_t KEY_PAIR_SIZE = 64; // 32 + 32 bytes
        
        // Calculate required size
        const size_t required_size = utils::sgx_calc_sealed_data_size(0, KEY_PAIR_SIZE);
        
        // Allocate buffer using vector
        std::vector<uint8_t> sealed_blob(required_size);
        size_t actual_size = 0;
        
        // Call enclave function
        sgx_status_t retval;
        sgx_status_t status = enclave_generate_new_keypair(
            eid, &retval, 
            sealed_blob.data(), sealed_blob.size(), 
            &actual_size
        );
        
        // Check both status values
        if (status == SGX_SUCCESS && retval == SGX_SUCCESS) {
            sealed_blob.resize(actual_size);  // Trim to actual size
            std::cout << "Successfully sealed keypair. Size: " << actual_size << " bytes\n";
            return sealed_blob;
        }
        
        // Handle errors
        if (retval == SGX_ERROR_INVALID_PARAMETER) {
            std::cerr << "Buffer too small. Required size: " << actual_size << "\n";
        } else {
            std::cerr << "Failed to generate keypair. ECALL status: " << status 
                    << ", Enclave status: " << retval << "\n";
        }
        
        return std::nullopt;  // Return empty optional on failure
    }

    bool writeVectorToFile(const std::vector<uint8_t>& data, const std::string& filename) {

        if (std::filesystem::exists(filename)) {
            return false; // File already exists, so we don't overwrite it
        }

        std::ofstream file(filename, std::ios::binary);
        
        if (!file.is_open()) {
            return false;
        }
        
        // Write the data
        file.write(reinterpret_cast<const char*>(data.data()), data.size());
        
        // Check if write was successful
        return file.good();
    }

    std::vector<uint8_t> readVectorFromFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::binary | std::ios::ate);
        
        if (!file.is_open()) {
            // Return empty vector if file doesn't exist (for new key generation)
            return std::vector<uint8_t>();
        }
        
        // Get file size (we opened with ate flag, so we're at the end)
        std::streamsize size = file.tellg();
        file.seekg(0, std::ios::beg);
        
        // Allocate vector with the file size
        std::vector<uint8_t> buffer(size);
        
        // Read the entire file
        file.read(reinterpret_cast<char*>(buffer.data()), size);
        
        if (!file.good()) {
            throw std::runtime_error("Failed to read file: " + filename);
        }
        
        return buffer;
    }
}
