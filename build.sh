#!/bin/bash
trap exit ERR # Exit on error
source settings.sh

###################################
# Functions for output formatting #
###################################
function error
{   echo -e "\033[31;1m[Error]\033[0m ${1}"
    exit 1
}

function warning
{   
    echo -e "\033[33;1m[Warning]\033[0m ${1}"
}

function notice
{   
    echo -e "\033[32;1m[${1}]\033[0m ${2}"
}

function notice_build
{   
    echo -e "\033[34;1m[Build]\033[0m ${1}"
}

#################
# Cleanup       #
#################

if [[ $1 == "clean" ]]; then
    rm output/* -rf
    exit 0
fi

compiletoasm=false
if [[ $1 == "asm" ]]; then
    compiletoasm=true
fi

#################
# Build Process #
#################

# Verify that all compilers have been found
notice "Setup" "Finding compilers"
if [ ! ${CC} ]; then error "C Compiler not found"; fi
if [ ! ${ASM} ]; then error "Assembler not found"; fi

# Find source files
notice "Setup" "Finding source files"

ASMFILES=""
CFILES=""

for file in $(find src/${ARCH}/ -iname *.c 2> /dev/null); do
    CFILES+="${file} "
done
for file in $(find src/${ARCH}/ -iname *.asm 2> /dev/null); do
    ASMFILES+="${file} "
done
for file in $(find src/all/ -iname *.c 2> /dev/null); do
    CFILES+="${file} "
done
for file in $(find src/all/ -iname *.asm 2> /dev/null); do
    ASMFILES+="${file} "
done

# Compiler arguments
notice "Setup" "Setting compiler arguments"

INCLUDE="-Iinc/${ARCH} -Iinc/all -Iinc/${ARCH}/drivers"

case $ARCH in
    x86 ) ARCHARGS="-m32" ;;
    x86_64 ) ARCHARGS="" ;;
    * ) error "Specified Architecture not found" ;;
esac


# Build time!
mkdir -p output/image_root/
notice_build "Building Valix"

OFILES=""
notice_build "Assembling"
for FILE in ${ASMFILES}; do
    OUTPUT=${FILE}.o
    OUTPUT=output/${OUTPUT##*/}
    OFILES="${OFILES} ${OUTPUT}"
    # Compile only if modified since last build
    if [[ ${FILE} -nt ${OUTPUT} ]]; then
        notice "ASM" "${FILE}"
        ${ASM} ${ASMARGS} ${FILE} ${OUTPUT} || error "Assembly failed"
    fi
done

notice_build "Compiling"
for FILE in ${CFILES}; do
    ASMOUTPUT=${FILE}.s
    ASMOUTPUT=output/${ASMOUTPUT##*/}
    OUTPUT=${FILE}.o
    OUTPUT=output/${OUTPUT##*/}
    OFILES="${OFILES} ${OUTPUT}"
    # Compile only if modified since last build
    if [[ ${FILE} -nt ${OUTPUT} ]]; then
        notice "CC" "${FILE}"
        ${CC} ${AUTODEFINES} -nostdlib -nodefaultlibs -fno-stack-protector \
            -ffreestanding -fno-stack-limit -fno-stack-check -fno-builtin \
            -O3 -nostdinc -Werror -g ${CCARGS} ${ARCHARGS} -Wall ${INCLUDE} -c \
            -o $OUTPUT $FILE || error "C compilation failed"
    fi
    if $compiletoasm; then
        ${CC} ${AUTODEFINES} -S -nostdlib -nodefaultlibs -fno-stack-protector \
            -ffreestanding -fno-stack-limit -fno-stack-check \
            -O3 -nostdinc -Werror -g ${CCARGS} ${ARCHARGS} -Wall ${INCLUDE} -c \
            -o $ASMOUTPUT $FILE || error "C compilation failed"
    fi
done

notice_build "Linking"
ld -n -T build/link.ld -Map ./kernel.map -b elf32-i386 -o output/kernel.elf ${OFILES} \
    -melf_i386 || (touch src/*/* && error "Linking failed")

# Build output
cp -R build/image_root/ output/
cp output/kernel.elf output/image_root/boot/

IMAGE_NAME="valix.${OUTPUT_TYPE}"
$GRUB_MKRESCUE --version
case $OUTPUT_TYPE in
  "iso" )
    notice_build "Building ISO"
    $GRUB_MKRESCUE --modules="iso9660,terminal,gfxterm,vbe,tga" \
        --output=${IMAGE_NAME} output/image_root || error "ISO failed to build"
    notice_build "Build complete"
    echo -e "Use \033[1mqemu -cdrom ${IMAGE_NAME} -serial stdio\033[0m to run , or use this script with the run-qemu argument"
  ;;
  "img" )
    notice_build "Building Image"
    $GRUB_MKRESCUE --modules="fat,terminal,gfxterm,vbe,tga" \
        --output=${IMAGE_NAME} output/image_root || error "Img failed to build"
    notice_build "Build complete"
    echo -e "Use \033[1mqemu -hda ${IMAGE_NAME} -serial stdio\033[0m to run, or use this script with the run-qemu argument"
  ;;
  * ) error "Unknown output type";;
esac


if [[ $1 == "run-qemu" ]]; then
    #if [[ ! -d valix.img ]]; then
    #    notice_build "Creating QEMU Hard Disk Image"
    #    qemu-img create -f qcow2 valix.img 4G
    #fi
    qemu -cdrom valix.iso -m 256 -serial stdio # -hda valix.img
    exit
fi

if [[ $1 == "run-vbox" ]]; then
    VBoxManage startvm "valix"
fi

${POST_BUILD} &
