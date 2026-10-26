# This uses asm6809 from
# http://www.6809.org.uk/asm6809/dl/2.3/
AS = asm6809

default: docker

docker-build: Dockerfile
	docker build . -t asm6809

all: docker

docker:
	docker run --rm -v "$(CURDIR)":/build -u `id -u ${USER}`:`id -g ${USER}` asm6809 make led-test2.bin

led-test2.bin: led-test2.asm
	$(AS)  -B -o $@ $^

clean:
	rm -f led-test2.bin

copy: led-test.bin
	sudo mount /dev/sde1 /mnt
	sudo cp led-test2.bin /mnt
	sudo umount /mnt
