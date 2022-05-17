#pragma once

#ifndef UTILITIES_H
#define UTILITIES_H

#include <cstdint>

namespace utils {
    uint32_t sgx_calc_sealed_data_size(const uint32_t add_mac_txt_size, const uint32_t txt_encrypt_size);
}

#endif // UTILS_H