#Makefile for pid monitor
obj-m:=PidMonitor.o
KDIR:=/lib/modules/$(shell uname -r)/build
default:
	make -C $(KDIR) M=$(shell pwd) modules
	$(RM) *.mod.c *.mod.o *.o *.order *.symvers *.ko.unsigned
clean:
	$(RM) *.mod.c *.mod.o *.o *.order *.symvers *.ko.unsigned
