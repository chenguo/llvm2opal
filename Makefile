all: llvm2opal

llvm2opal: src/bufops.o src/l2o.o
	gcc -o llvm2opal src/bufops.o src/l2o.o

bufops.o: src/bufops.c
	gcc -O -c src/bufops.c

l2o.o: src/l2o.c
	gcc -O -c src/l2o.c

check:
	@make check -sC tests/

clean:
	@rm -f llvm2opal src/*.o src/*~ src/*#
