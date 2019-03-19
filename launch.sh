#!/bin/sh

if [ "$#" -lt 2 ]; then
	echo "Usage ./launch <path_to_img_file> <memory M|G> [other_options]"
	exit 1
fi

drive_path=$1
memory=$2

if [ -z "$3" ]; then
	options=""
else
	options=$3
fi

echo "Launching Qemu emulator. Login to your VM now.."
../panda/x86_64-softmmu/qemu-system-x86_64 -drive file=$drive_path,format=qcow2 -m $memory -monitor tcp::4443,server,nowait -net user,hostfwd=tcp::2221-:22 -net nic $options

