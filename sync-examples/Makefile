LDFLAGS += -pthread

BIN=counter1 counter2 counter3 counter4 election1 election2 election3 election4 procon1 procon2 procon3 procon4

all: ${BIN}
.PHONY: all

clean:
	rm *.o ${BIN}
.PHONY: clean

counter2: lock.o
election2: lock.o
procon2: lock.o
procon4: lock.o
