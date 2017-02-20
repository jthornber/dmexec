PROGRAMS=\
	dmexec \
	vector_t

.PHONEY: all
all: $(PROGRAMS)

V=
RM:=rm -f
CC:=gcc
CFLAGS=\
	-m32 \
	-g \
	-Wall \
	-D_GNU_SOURCE \
	-std=c99

INCLUDES=\
	-I.

LIBS=\
	-lreadline

SOURCE=\
	cons.c \
	env.c \
	eval.c \
	mm.c \
	print.c \
	read.c \
	string_type.c \
	symbol.c \
	utils.c \
	vector.c

OBJECTS:=$(subst .c,.o,$(SOURCE))
DEPENDS:=$(subst .c,.d,$(SOURCE))

.SUFFIXES: .d .c .o

%.o: %.c
	@echo "    [CC]  $<"
	$(V) $(CC) -c $(INCLUDES) $(CFLAGS) -o $@ $<
	@echo "    [DEP] $<"
	$(V) $(CC) -MM -MT $(subst .c,.o,$<) $(INCLUDES) $(CFLAGS) $< > $*.$$$$; \
	sed 's,\([^ :]*\)\.o[ :]*,\1.o : Makefile ,g' < $*.$$$$ > $*.d; \
	$(RM) $*.$$$$

.PHONEY: clean

clean:
	find . -name \*.o -delete
	find . -name \*.d -delete
	$(RM) $(PROGRAMS)

dmexec: $(OBJECTS) main.o
	@echo "    [LD]  $@"
	$(V) $(CC) $(CFLAGS) -o $@ $+ $(LIBS)

vector_t: $(OBJECTS) vector_t.o
	@echo "    [LD]  $@"
	$(V) $(CC) $(CFLAGS) -o $@ $+ $(LIBS)

# Making this depend on OBJECTS as a quick way of picking up the .h deps.
tags:
	ctags -a --sort=yes *.[hc]

-include $(DEPENDS)
