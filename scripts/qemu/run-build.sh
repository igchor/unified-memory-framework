#!/bin/bash
set -x

repo=$1
branch=$2

echo password | sudo -Sk apt update
echo password | sudo -Sk apt install -y git cmake gcc g++ numactl hwloc libhwloc-dev libnuma-dev

numactl -H

git clone $repo umf
cd umf
git checkout $branch

mkdir build
cd build

cmake .. \
    -DCMAKE_BUILD_TYPE=Debug \
    -DUMF_BUILD_OS_MEMORY_PROVIDER=ON \
    -DUMF_ENABLE_POOL_TRACKING=ON \
    -DUMF_FORMAT_CODE_STYLE=OFF \
    -DUMF_DEVELOPER_MODE=ON \
    -DUMF_BUILD_LIBUMF_POOL_DISJOINT=ON \
    -DUMF_BUILD_EXAMPLES=ON \
    -DUMF_BUILD_LEVEL_ZERO_PROVIDER=OFF

make -j $(nproc)

ctest --output-on-failure

