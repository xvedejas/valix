ARCH="x86"
OUTPUT_TYPE="iso"
GRUB_PREFIX="/usr/local/"
ASM="build/fasm"
CC="gcc"
LD="ld"
ASMARGS=""
CCARGS="-O3 -m32 -nostdlib -nostartfiles -nodefaultlibs"
LDARGS=""
BUILDLOG="buildlog.txt"
GRUB_MKRESCUE=`which grub-mkrescue`
QEMU="qemu-system-i386" # may need to be QEMU="kvm" for your system
