// Include image with some alignment

.section ".rodata"
.p2align 12
.global image_vmlinux_start
image_vmlinux_start:
.incbin VMLINUX_IMAGE
.global image_vmlinux_end
image_vmlinux_end:
.p2align 12
.previous
