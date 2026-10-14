# Requires: nasm, gcc (with -m32 support), binutils (ld/objcopy), make. Optional: qemu-system-i386
STAGE2_SECTORS := 16
KERNEL_SECTORS := 32

CC      := gcc
LD      := ld
NASM    := nasm
CFLAGS  := -m32 -ffreestanding -fno-pic -fno-pie -fno-stack-protector -fno-builtin \
           -nostdlib -O2 -Wall -Wextra -Iinclude -mno-sse -mno-mmx -Wno-array-bounds
LDFLAGS := -m elf_i386 -nostdlib -z noexecstack --no-warn-rwx-segments

BUILD := build
IMG   := $(BUILD)/disk.img

all: $(IMG)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/stage1.bin: boot/stage1.asm | $(BUILD)
	$(NASM) -f bin -DSTAGE2_SECTORS=$(STAGE2_SECTORS) -DKERNEL_SECTORS=$(KERNEL_SECTORS) $< -o $@

$(BUILD)/stage2_entry.o: boot/stage2_entry.asm | $(BUILD)
	$(NASM) -f elf32 $< -o $@

$(BUILD)/stage2.o: src/stage2.c include/boot_info.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/stage2.bin: $(BUILD)/stage2_entry.o $(BUILD)/stage2.o boot/stage2.ld
	$(LD) $(LDFLAGS) -T boot/stage2.ld $(BUILD)/stage2_entry.o $(BUILD)/stage2.o -o $(BUILD)/stage2.elf
	objcopy -O binary $(BUILD)/stage2.elf $@
	@size=$$(wc -c < $@); max=$$(( $(STAGE2_SECTORS) * 512 )); \
	  if [ $$size -gt $$max ]; then echo "stage2 too big ($$size > $$max)"; exit 1; fi; \
	  echo "stage2: $$size / $$max bytes"
	truncate -s $$(( $(STAGE2_SECTORS) * 512 )) $@

$(BUILD)/kernel.o: kernel/kernel.c include/boot_info.h | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/kernel.bin: $(BUILD)/kernel.o kernel/kernel.ld
	$(LD) $(LDFLAGS) -T kernel/kernel.ld $< -o $(BUILD)/kernel.elf
	objcopy -O binary $(BUILD)/kernel.elf $@
	@size=$$(wc -c < $@); max=$$(( $(KERNEL_SECTORS) * 512 )); \
	  if [ $$size -gt $$max ]; then echo "kernel too big ($$size > $$max)"; exit 1; fi
	truncate -s $$(( $(KERNEL_SECTORS) * 512 )) $@

$(IMG): $(BUILD)/stage1.bin $(BUILD)/stage2.bin $(BUILD)/kernel.bin
	cat $^ > $@
	truncate -s 1440K $@
	@echo "Built $@"

run: $(IMG)
	qemu-system-i386 -drive format=raw,file=$(IMG)

run-nogui: $(IMG)
	qemu-system-i386 -drive format=raw,file=$(IMG) -display none -serial stdio

clean:
	rm -rf $(BUILD)

.PHONY: all run run-nogui clean
