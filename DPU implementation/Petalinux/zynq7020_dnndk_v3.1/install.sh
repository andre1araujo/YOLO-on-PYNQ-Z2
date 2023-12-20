#!/bin/bash

echo "Begin to install Xilinx DNNDK ..."

my_board_type=""
mount_boot_dev="/dev/mmcblk0p1"

install_fail () {
    echo "Installation failed!"
    exit 1
}

uname=$(uname -n)
if [ "$my_board_type" != "" ] && [ "$my_board_type" != "$uname" ]; then
    echo "Board type mismatch."
    install_fail
fi

arch=$(uname -m)
if [ "$arch" != "armv7l" -a "$arch" != "aarch64" ]; then
    echo "Xilinx DNNDK package could only be installed on ARM targets."
    echo "Terminate installation ..."
    install_fail
fi

###########################################################
echo "Install DPU Driver ..."

mkdir -p /lib/modules/$(uname -r)/extra/ || install_fail
touch /lib/modules/$(uname -r)/modules.order
touch /lib/modules/$(uname -r)/modules.builtin
cp pkgs/driver/dpu.ko /lib/modules/$(uname -r)/extra/ || install_fail

depmod -a
rst="$(lsmod | grep dpu 2>&1)"
if [ -n "$rst" ] ; then
    rmmod dpu
    [ $? != 0 ] && install_fail
fi
modprobe dpu
[ $? != 0 ] && install_fail

if ! grep -Fxq "dpu" /etc/modules ; then
    sh -c 'echo "dpu" >> /etc/modules' ;
    [ $? != 0 ] && install_fail
fi

chmod og+rw /sys/module/dpu/parameters/cache || install_fail
chmod og+rw /sys/module/dpu/parameters/coremask || install_fail
chmod og+rw /sys/module/dpu/parameters/debuglevel || install_fail
chmod og+rw /sys/module/dpu/parameters/extension || install_fail
chmod og+rw /sys/module/dpu/parameters/mode || install_fail
chmod og+rw /sys/module/dpu/parameters/profiler || install_fail
chmod og+rw /sys/module/dpu/parameters/timeout || install_fail
chmod og+rw /sys/module/dpu/parameters/accipmask || install_fail

###########################################################
echo "Install DNNDK tools, runtime & libraries ..."
[ ! -d /usr/local/bin ] && mkdir -p /usr/local/bin
[ ! -d /usr/local/lib ] && mkdir -p /usr/local/lib
pfile="/usr/local/lib/libhineon.a"
[ -f $pfile ] && rm $pfile
cp pkgs/bin/*  /usr/local/bin/ || install_fail
cp pkgs/lib/*  /usr/local/lib/ || install_fail

opencvver="$(pkg-config --modversion opencv)"
[ $? != 0 ] && install_fail
opencvver="$(echo $opencvver | awk -F'.' '{print $1"."$2}')"
lfile="/usr/local/lib/libdputils.so"
if [ -f $lfile ] ; then
    rm $lfile
fi
if [ "$opencvver" =  "2.4" ] ; then
    if [ ! -f /usr/local/lib/libdputils.so.2.4 ]; then
        echo "Library /usr/loca/lib/libdputils.so.2.4 isn't installed as expected." 1>&2
        install_fail
    fi
    ln -s /usr/local/lib/libdputils.so.2.4 $lfile
    [ $? != 0 ] && install_fail
elif [ "$opencvver" = "3.1" ] ; then
    if [ ! -f /usr/local/lib/libdputils.so.3.1 ]; then
        echo "Library /usr/loca/lib/libdputils.so.3.1 isn't installed as expected." 1>&2
        install_fail
    fi
    ln -s /usr/local/lib/libdputils.so.3.1 $lfile
    [ $? != 0 ] && install_fail
elif [ "$opencvver" = "3.3" ] ; then
    if [ ! -f /usr/local/lib/libdputils.so.3.3 ]; then
        echo "Library /usr/loca/lib/libdputils.so.3.3 isn't installed as expected." 1>&2
        install_fail
    fi
    ln -s /usr/local/lib/libdputils.so.3.3 $lfile
    [ $? != 0 ] && install_fail
elif [ "$opencvver" = "3.4" ] ; then
    if [ ! -f /usr/local/lib/libdputils.so.3.4 ]; then
        echo "Library /usr/loca/lib/libdputils.so.3.4 isn't installed as expected." 1>&2
        install_fail
    fi
    ln -s /usr/local/lib/libdputils.so.3.4 $lfile
    [ $? != 0 ] && install_fail
else
    echo "opencv version" $opencvver "not support!!"
    install_fail
fi
mkdir -p /usr/local/include/dnndk/ || install_fail
cp pkgs/include/*.h  /usr/local/include/dnndk/ || install_fail
[ ! -d /etc/ld.so.conf.d ] && mkdir /etc/ld.so.conf.d
echo "/usr/local/lib" > /etc/ld.so.conf.d/dnndk.conf
ldconfig
[ $? != 0 ] && install_fail

reboot_flag=false
if [ "$my_board_type" != "" ]; then
    if [ -f ./pkgs/system/BOOT.BIN ]; then
        msg=$(mount "$mount_boot_dev" /boot 2>&1)
        md5sum_boot=$(md5sum ./pkgs/system/BOOT.BIN) || install_fail
        md5sum_boot=${md5sum_boot:0:31}
        md5sum_boot_old=$(md5sum /boot/BOOT.BIN) || install_fail
        md5sum_boot_old=${md5sum_boot_old:0:31}
        if [ "$md5sum_boot" != "$md5sum_boot_old" ]; then
            cp /boot/BOOT.BIN /boot/Backup_BOOT.BIN || install_fail
            cp ./pkgs/system/BOOT.BIN /boot/ || install_fail
            sync
            reboot_flag=true
        fi
    fi
    if [ -f ./pkgs/system/system.dtb ] && [ "$reboot_flag" == "true" ]; then
        cp /boot/system.dtb /boot/Backup_system.dtb || install_fail
        cp ./pkgs/system/system.dtb /boot/ || install_fail
        sync
    fi
fi

# install python support
echo "Install python support ..."
tmp=$(pip --version 2>&1 | grep "command not found")
if [ "$tmp" = "" ]; then
    pip install pkgs/python/*.whl || install_fail
else
    echo "Warning: pip command not found, skip install python support"
fi

echo "Complete installation successfully."

if [ "$my_board_type" != "" ]; then
    if [ "$reboot_flag" == "true" ]; then
        echo "The BOOT.BIN and system.dtb have been replaced."
        echo "The system will reboot after 5 seconds ..."
        for (( i=5; i>0; i-- )); do
            sleep 1
            echo $i
        done
        reboot
    fi
fi

exit 0
