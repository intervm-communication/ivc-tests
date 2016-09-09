
.PHONY: test-userspace-grants

sample:
	make -C sample-testcase
	./run-test sample-testcase

test-userspace-grants:
	make -C test-userspace-grants
	./run-test test-userspace-grants
