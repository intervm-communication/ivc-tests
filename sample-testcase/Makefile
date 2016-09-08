testcase.cpio.gz: Makefile initrd/init initrd/runtest initrd
	cd initrd && find . | cpio --create --format='newc' | gzip > ../testcase.cpio.gz

runtest: runtest.c
	g++ -static runtest.c -o runtest

initrd: runtest init
	mkdir -p initrd initrd/bin
	cp runtest init initrd/
	cp /bin/busybox initrd/bin/
	touch initrd/init
	
