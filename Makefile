ls-sim: main.o manager.o router.o simulator.o
	clang main.o manager.o router.o simulator.o -o ls-sim

main.o: main.c simulator.h
	clang -c main.c

manager.o: manager.c simulator.h
	clang -c manager.c

router.o: router.c simulator.h
	clang -c router.c

simulator.o: simulator.c simulator.h
	clang -c simulator.c

clean:
	rm -f *.o ls-sim