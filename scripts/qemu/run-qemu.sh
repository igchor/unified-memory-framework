#!/bin/bash

echo pass | sudo -Sk qemu-system-x86_64 \
-drive file=/home/user/lunar-server-cloudimg-amd64.img,format=qcow2,index=0,media=disk,id=hd \
-cdrom /var/ubuntu-cloud-init.iso \
-machine pc,accel=kvm,usb=off,hmat=on \
-enable-kvm -net nic -net user,hostfwd=tcp::2222-:22 \
-m 1G \
-smp 3 \
-object memory-backend-ram,size=100M,id=ram0 \
-object memory-backend-ram,size=100M,id=ram1 \
-object memory-backend-ram,size=100M,id=ram2 \
-numa node,nodeid=0,memdev=ram0,cpus=0-1 \
-numa node,nodeid=1,memdev=ram1,cpus=2-3 \
-numa node,nodeid=2,memdev=ram2 \
-numa dist,src=0,dst=0,val=10 \
-numa dist,src=0,dst=1,val=20 \
-numa dist,src=0,dst=2,val=17 \
-numa dist,src=1,dst=0,val=20 \
-numa dist,src=1,dst=1,val=10 \
-numa dist,src=1,dst=2,val=28 \
-numa dist,src=2,dst=0,val=17 \
-numa dist,src=2,dst=1,val=28 \
-numa dist,src=2,dst=2,val=10 \
-numa hmat-lb,initiator=0,target=0,hierarchy=memory,data-type=access-latency,latency=10 \
-numa hmat-lb,initiator=0,target=0,hierarchy=memory,data-type=access-bandwidth,bandwidth=10485760 \
-numa hmat-lb,initiator=0,target=1,hierarchy=memory,data-type=access-latency,latency=20 \
-numa hmat-lb,initiator=0,target=1,hierarchy=memory,data-type=access-bandwidth,bandwidth=5242880 \
-numa hmat-lb,initiator=0,target=2,hierarchy=memory,data-type=access-latency,latency=16 \
-numa hmat-lb,initiator=0,target=2,hierarchy=memory,data-type=access-bandwidth,bandwidth=1048576 \
-numa hmat-lb,initiator=1,target=0,hierarchy=memory,data-type=access-latency,latency=20 \
-numa hmat-lb,initiator=1,target=0,hierarchy=memory,data-type=access-bandwidth,bandwidth=5242880 \
-numa hmat-lb,initiator=1,target=1,hierarchy=memory,data-type=access-latency,latency=10 \
-numa hmat-lb,initiator=1,target=1,hierarchy=memory,data-type=access-bandwidth,bandwidth=10485760 \
-numa hmat-lb,initiator=1,target=2,hierarchy=memory,data-type=access-latency,latency=27 \
-numa hmat-lb,initiator=1,target=2,hierarchy=memory,data-type=access-bandwidth,bandwidth=1048576 \
-daemonize

# wait for qemu to boot
ssh-keyscan -p 2222 -H 172.17.0.2 >> /home/user/.ssh/known_hosts
while [ $? -ne 0 ]
do
echo "Trying to connect..."
ps -aux | grep qemu
sleep 5
ssh-keyscan -p 2222 -H 172.17.0.2 >> /home/user/.ssh/known_hosts
done

scp -P 2222 /opt/shared/scripts/qemu/run-build.sh cxltest@172.17.0.2:/home/cxltest

ssh cxltest@172.17.0.2 -p 2222 -t 'sudo apt update && sudo apt install git libnuma-dev libjemalloc-dev libtbb-dev libhwloc-dev cmake gcc'
ssh cxltest@172.17.0.2 -p 2222 -t 'bash /home/cxltest/run-build.sh ${CI_REPO_SLUG} ${CI_BRANCH}'
