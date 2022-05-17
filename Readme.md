# Intel SGX Enclave Tutorial (Ubuntu)

This application is a minimum example for running secured application with Intel SGX enclave.

## Intel SGX Drivers and SDK Installation

Before building this application, please set the required software in Intel SGX platform.

See installation guide for Intel SGX software and then install drivers, runtime, and SDKs.<br>
Here I installed v2.16 on Ubuntu Server 20.04 LTS (Standard DC2s v3) in Microsoft Azure.

```
#
# Setup for platform driver
#

# update system
sudo apt update
sudo apt upgrade
# install Intel SGX driver
# (use out-of-tree platform's driver for Azure VM image)
sudo apt-get install build-essential ocaml automake autoconf libtool wget python libssl-dev dkms
wget https://download.01.org/intel-sgx/latest/linux-latest/distro/ubuntu20.04-server/sgx_linux_x64_driver_2.11.054c9c4c.bin
chmod 777 sgx_linux_x64_driver_2.11.054c9c4c.bin
sudo ./sgx_linux_x64_driver_2.11.054c9c4c.bin

#
# Setup for application users
#

# add repository
echo 'deb [arch=amd64] https://download.01.org/intel-sgx/sgx_repo/ubuntu focal main' | sudo tee /etc/apt/sources.list.d/intel-sgx.list
# add key
wget -qO - https://download.01.org/intel-sgx/sgx_repo/ubuntu/intel-sgx-deb.key | sudo apt-key add
# install packages
sudo apt-get update
sudo apt-get install libsgx-epid libsgx-quote-ex libsgx-dcap-ql
# install DCAP QPL package
sudo apt-get install libsgx-dcap-default-qpl

#
# Setup for application developers
#

# install required packages
sudo apt-get install build-essential python
# download sdk
wget https://download.01.org/intel-sgx/latest/linux-latest/distro/ubuntu20.04-server/sgx_linux_x64_sdk_2.16.100.4.bin
# install sdk
chmod +x sgx_linux_x64_sdk_2.16.100.4.bin
sudo ./sgx_linux_x64_sdk_2.16.100.4.bin --prefix /opt/intel
# set environment
source /opt/intel/sgxsdk/environment
# install other dev packages
sudo apt-get install libsgx-enclave-common-dev libsgx-dcap-ql-dev libsgx-dcap-default-qpl-dev
```

## Build and run this application

Typical workflow

```
mkdir build && cd build
cmake -DSGX_MODE=SIM -DSGX_DEBUG=1 ..
make -j (nproc)                                        # build everything
./app                                                  # run the app+enclave
make clean-all                                         # delete generated artifacts
```

or

```
mkdir build && cd build
cmake -DSGX_MODE=SIM -DSGX_DEBUG=1 ..
cmake --build . --target sgx_build_all -- -j$(nproc)   # build everything (-j (nproc) in fish)
cmake --build . --target run                           # run the app+enclave
cmake --build . --target clean-all                     # delete generated artifacts
```
