U2FS_VERSION="0.1"

EXTRA_CFLAGS += -DU2FS_VERSION=\"$(U2FS_VERSION)\"

obj-$(CONFIG_WRAP_FS) += wrapfs.o

wrapfs-y :=sioq.o rename.o  copyup.o whiteout.o dentry.o file.o inode.o main.o super.o lookup.o mmap.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

default:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean: 
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
