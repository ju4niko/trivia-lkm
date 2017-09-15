obj-m+=trivialkm.o

all:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
	$(CC) trivia.c -o trivia
clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean

