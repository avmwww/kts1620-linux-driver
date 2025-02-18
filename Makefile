#
# Example:
#  KERN_SRC=~/projects/linux
#  ARCH=arm64

obj-m := gpio-kts1620.o
KERN_SRC ?= /lib/modules/`uname -r`/build

all:
	$(MAKE) -C $(KERN_SRC) M=$(PWD) modules
clean:
	$(MAKE) -C $(KERN_SRC) M=$(PWD) clean

install:
	$(MAKE) -C $(KERN_SRC) M=$(PWD) modules_install

