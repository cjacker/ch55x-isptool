DESTDIR := /usr/local
CC := g++
TARGET := ch55xisptool

$(TARGET):main.o KT_BinIO.o KT_ProgressBar.o
	$(CC) main.o KT_BinIO.o KT_ProgressBar.o -o $(TARGET) -lusb-1.0 

%.o: %.c
	$(CC) -c $<

clean:
	rm -f $(TARGET) *.o

install: $(TARGET)
	install -m0755 $(TARGET) $(DESTDIR)/bin

