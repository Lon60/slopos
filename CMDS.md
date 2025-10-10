grub-mkimage \
  -O x86_64-efi \
  -o BOOTX64.EFI \
  efi_gop efi_uga multiboot2 normal configfile

grub-mkstandalone \
  -O x86_64-efi \
  -o BOOTX64.EFI \
  "iso/boot/grub/grub.cfg=grub.cfg"


