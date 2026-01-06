#!/bin/bash

OS_NAME="VST_OS"
BUILD_DIR="build"
ISO_DIR="iso_root"
OUTPUT_ISO="${OS_NAME}.iso"


GREEN='\033[0;32m'
BLUE='\033[0;34m'
RED='\033[0;31m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[OK]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; exit 1; }


clear
log_info "Сборка $OS_NAME..."


for cmd in nasm g++ ld grub-mkrescue; do
    command -v $cmd >/dev/null 2>&1 || log_error "Утилита $cmd не найдена. Установите её."
done


rm -rf $BUILD_DIR $ISO_DIR
mkdir -p $BUILD_DIR


log_info "Компиляция загрузчика (ASM)..."
nasm -f elf32 boot.asm -o $BUILD_DIR/boot.o || log_error "Ошибка NASM"


log_info "Компиляция ядра (C++)..."
g++ -m32 -c kernel.cpp -o $BUILD_DIR/kernel.o \
    -ffreestanding -O2 -Wall -Wextra \
    -fno-exceptions -fno-rtti -fno-stack-protector || log_error "Ошибка C++"

log_info "Связывание компонентов (Linker)..."
ld -m elf_i386 -T linker.ld -o $BUILD_DIR/kernel.bin $BUILD_DIR/boot.o $BUILD_DIR/kernel.o || log_error "Ошибка линковки"

log_info "Формирование структуры ISO..."
mkdir -p $ISO_DIR/boot/grub
cp $BUILD_DIR/kernel.bin $ISO_DIR/boot/kernel.bin
cp grub.cfg $ISO_DIR/boot/grub/grub.cfg

log_info "Генерация $OUTPUT_ISO..."
grub-mkrescue -o $OUTPUT_ISO $ISO_DIR >/dev/null 2>&1 || log_error "Ошибка grub-mkrescue"

log_info "Уборка временных файлов..."
rm -rf $BUILD_DIR $ISO_DIR

echo -e "--------------------------------------"
log_success "Сборка завершена успешно!"
log_info "Файл образа: ${GREEN}$OUTPUT_ISO${NC}"
echo -e "--------------------------------------"