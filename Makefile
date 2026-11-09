CC = gcc
CFLAGS = -std=c11 -O2 -Wall -Wextra -Wconversion -pedantic -finput-charset=UTF-8 -fexec-charset=UTF-8
OBJDIR = build
TARGET = aps_ordenacao
OBJECTS = $(OBJDIR)/nucleo.o $(OBJDIR)/menu.o $(OBJDIR)/main.o

.PHONY: all testes

all: $(TARGET)

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)/nucleo.o: aps_ordenacao.c | $(OBJDIR)
	$(CC) $(CFLAGS) -Dmain=aps_core_main -c $< -o $@

$(OBJDIR)/menu.o: src/menu.c include/menu.h include/nucleo.h | $(OBJDIR)
	$(CC) $(CFLAGS) -Iinclude -c $< -o $@

$(OBJDIR)/main.o: src/main.c include/menu.h include/nucleo.h | $(OBJDIR)
	$(CC) $(CFLAGS) -Iinclude -c $< -o $@

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) $(OBJECTS) -o $(TARGET)

testes: $(TARGET)
	./$(TARGET) --selftest
