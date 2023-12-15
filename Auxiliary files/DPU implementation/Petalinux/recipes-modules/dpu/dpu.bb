<<<<<<< HEAD
version https://git-lfs.github.com/spec/v1
oid sha256:30a1b7bc57f8ac361d63f6622ac86947bd3e9992a020cff5888ed6db50128f69
size 611
=======
SUMMARY = "Recipe for  build an external dpu Linux kernel module"
SECTION = "PETALINUX/modules"
LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://COPYING;md5=12f884d2ae1ff87c09e5b7ccc2c4ca7e"

inherit module

SRC_URI = "file://Makefile \
           file://dpucore.c\
           file://dpucore.h\
           file://dpuext.c\
           file://dpuext.h\
           file://dpudef.h\
	   file://COPYING \
          "

S = "${WORKDIR}"

#EXTRA_OEMAKE += "DPU_TARGET=1.3"

# The inherit of module.bbclass will automatically name module packages with
# "kernel-module-" prefix as required by the oe-core build environment.
>>>>>>> origin/main
