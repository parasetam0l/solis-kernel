
cd /opt/swap/sdk

insmod swap_master.ko
echo 1 > /sys/kernel/debug/swap/enable
insmod swap_kprobe.ko

insmod swap_kp_tests.ko 2>/dev/null

rmmod swap_kprobe
rmmod swap_master
