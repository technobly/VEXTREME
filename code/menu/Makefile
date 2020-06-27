# This uses asm6809 from
# http://www.6809.org.uk/asm6809/dl/2.3/
AS = asm6809

default: docker

docker-build: Dockerfile
	docker build . -t asm6809

all: docker

docker:
	docker run --rm -v `pwd`:/build -u `id -u ${USER}`:`id -g ${USER}` asm6809 make menu.bin

menu.bin: menu.asm
	$(AS)  -B -o $@ $^

clean:
	rm -f menu.bin

copy: menu.bin
	sudo mount /dev/sde1 /mnt
	sudo cp menu.bin /mnt
	sudo umount /mnt
