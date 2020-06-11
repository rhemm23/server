BINDIR = ./bin
SRCDIR = ./src
OBJDIR = ./obj

CC = gcc
OUT = server
LINK = -pthread
CFLAGS = -std=c99 -Wall

SOURCE_FILES := $(shell find $(SRCDIR)/ -type f -name '*.c')
OBJECT_FILES := $(patsubst $(SRCDIR)/%.c, $(OBJDIR)/%.o,$(SOURCE_FILES))

$(BINDIR)/$(OUT): $(OBJECT_FILES) | $(BINDIR)
	$(CC) $(LINK) $(OBJECT_FILES) -o $(BINDIR)/$(OUT)

$(OBJDIR)/%.o: $(SRCDIR)/%.c | $(OBJDIR)
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(BINDIR):
	mkdir -p $(BINDIR)

clean:
	rm -rf $(BINDIR)
	rm -rf $(OBJDIR)
