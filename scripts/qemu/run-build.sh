repo=$1
branch=$2

echo password | sudo -Sk apt update
echo password | sudo -Sk apt install git cmake gcc numactl hwloc libnuma-dev

git clone $repo umf
cd umf
git checkout $branch

cmake -B build \
    -DCMAKE_BUILD_TYPE=Debug \
    -DUMF_BUILD_OS_MEMORY_PROVIDER=ON \
    -DUMF_ENABLE_POOL_TRACKING=ON \
    -DUMF_FORMAT_CODE_STYLE=OFF \
    -DUMF_DEVELOPER_MODE=ON \
    -DUMF_BUILD_LIBUMF_POOL_DISJOINT=ON \
    -DUMF_BUILD_EXAMPLES=ON \
    -DUMF_BUILD_LEVEL_ZERO_PROVIDER=OFF

cmake --build build -j $(nproc)

cd build
ctest --output-on-failure --test-dir test
