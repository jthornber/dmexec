PROGRAMS=\
	dmexec

.PHONEY: all
all: $(PROGRAMS)

V=
RM:=rm -f
CC:=gcc
CFLAGS=\
	-g \
	-Wall \
	-D_GNU_SOURCE \
	-std=c99

INCLUDES=\
	-I.

LIBS=\
	-lreadline \
	-lgc

SOURCE=\
	cons.c \
	mm.c \
	string_type.c \
	symbol.c \
	utils.c \
	vm.c \

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

dmexec: $(OBJECTS)
	@echo "    [LD]  $@"
	$(V) $(CC) $(CFLAGS) -o $@ $+ $(LIBS)

# Making this depend on OBJECTS as a quick way of picking up the .h deps.
tags: $(OBJECTS)
	ctags -a --sort=yes *.[hc]

-include $(DEPENDS)
