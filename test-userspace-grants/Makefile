testcase.cpio.gz: Makefile initrd/init initrd/runtest initrd
	cd initrd && find . | cpio --create --format='newc' | gzip > ../testcase.cpio.gz

runtest: runtest.cpp otherside
	g++ -lxengnttab -lxentoollog -lxenstore $< -o $@

otherside: otherside.cpp
	g++ -lxengnttab -lxentoollog -lxenstore $< -o $@

initrd/init: runtest init
	# Copy the core executables...
	mkdir -p initrd initrd/bin
	cp runtest init initrd/
	cp /bin/busybox initrd/bin/
	
	# Copy our dependencies...
	mkdir -p initrd/usr/lib
	../utils/cpldd initrd/runtest initrd/usr/lib
	
	# ... and copy in the dynamic linker.
	mkdir -p initrd/lib64
	cp /lib64/ld-linux-x86-64.so.2 initrd/lib64/
	
	# Copy in any modules we need.
	find /usr/lib/modules/`uname -r`/kernel/drivers/xen/ -name *.ko* -exec cp {} initrd/ \;
	find initrd/ -name *.ko.gz -exec gunzip -f {} \;
	
	# Update the timestamp on the initrd.
	touch initrd/init
