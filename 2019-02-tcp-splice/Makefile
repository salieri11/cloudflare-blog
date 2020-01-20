all: test-burst echo-naive echo-iosubmit echo-splice echo-splice-proxy echo-sockmap echo-sockmap-proxy echo-naive-proxy echo-iosubmit-proxy
.PHONY: ebpf-prog	
ebpf-prog: venv/.ok
		clang -Wall -Wextra \
		-O2 -emit-llvm \
		-c echo-sockmap-kern.c -S -o - \
	| llc -march=bpf -filetype=obj -o - \
	| ./venv/bin/python3 tbpf-decode-elf.py /dev/stdin prog_parser prog_verdict > ebpf-prog.c
	clang-format -i ebpf-prog.c

.PHONY: ebpf-prog-proxy
ebpf-prog: venv/.ok
		clang -Wall -Wextra \
		-O2 -emit-llvm \
		-c echo-sockmap-kern-proxy.c -S -o - \
	| llc -march=bpf -filetype=obj -o - \
	| ./venv/bin/python3 tbpf-decode-elf.py /dev/stdin prog_parser prog_verdict > ebpf-prog-proxy.c
	clang-format -i ebpf-prog-proxy.c

.PHONY: test-burst
test-burst:
	clang -g -Wall -Wextra -O2 \
		net.c \
		test-burst.c \
		-o test-burst

.PHONY: echo-naive
echo-naive:
	clang -g -Wall -Wextra -O2 \
		net.c \
		echo-naive.c \
		-o echo-naive

.PHONY: echo-iosubmit
echo-iosubmit:
	clang -g -Wall -Wextra -O2 \
		net.c \
		echo-iosubmit.c \
		-o echo-iosubmit

.PHONY: echo-splice
echo-splice:
	clang -g -Wall -Wextra -O2 \
		net.c \
		echo-splice.c \
		-o echo-splice

.PHONY: echo-splice-proxy
echo-splice-proxy:
	clang -g -Wall -Wextra -O2 -pthread \
	net.c \
	echo-splice-proxy.c \
	-o echo-splice-proxy

.PHONY: echo-naive-proxy
echo-naive-proxy:
	clang -g -Wall -Wextra -O2 -pthread \
	net.c \
	echo-naive-proxy.c \
	-o echo-naive-proxy

.PHONY: echo-iosubmit-proxy
echo-iosubmit-proxy:
	clang -g -Wall -Wextra -O2 -pthread \
	net.c \
	echo-iosubmit-proxy.c \
	-o echo-iosubmit-proxy

.PHONY: echo-sockmap
echo-sockmap: ebpf-prog
	gcc -g -Wall -Wextra -O2 \
		tbpf.c \
		net.c \
		ebpf-prog.c \
		echo-sockmap.c \
		-o echo-sockmap

.PHONY: echo-sockmap-proxy
echo-sockmap-proxy: ebpf-prog-proxy
	gcc -g -Wall -Wextra -O2 \
		tbpf.c \
		net.c \
		ebpf-prog-proxy.c \
		echo-sockmap-proxy.c \
		-o echo-sockmap-proxy

venv/.ok:
	virtualenv venv --python=python3
	./venv/bin/pip3 install pyelftools
	pip3 install pyelftools
	touch $@

.PHONY: format
format:
	clang-format -i *.c *.h
	@grep -n "TODO" *.[ch] || true

.PHONY: clean
clean:
	rm -rf *.o
