ifneq ($(KERNELRELEASE),)
obj-m := snes.o

else
KDIR := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
endif

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
