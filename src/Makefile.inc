#
# File: Makefile.inc
#
# Includes Makefile.src and contains dependency info
#

include Makefile.src

BASEOBJS := $(ANGFILES) $(ZFILES)

depgen:
	head -n 19 Makefile.inc > Makefile.new
	@for i in $(BASEOBJS:.o=.c); do \
		DIR=`echo ./$$i | sed 's|/[^/]*$$||'`; \
			gcc -MM -I. $$i | \
			sed "s|^\\([^ ]\\)|$$DIR/\\1|" >> Makefile.new; \
	done;

# Dependencies
./variable.o: variable.c angband.h h-basic.h h-config.h h-system.h h-type.h \
  h-define.h z-util.h z-virt.h z-form.h z-rand.h z-term.h config.h \
  defines.h types.h externs.h
./tables.o: tables.c angband.h h-basic.h h-config.h h-system.h h-type.h \
  h-define.h z-util.h z-virt.h z-form.h z-rand.h z-term.h config.h \
  defines.h types.h externs.h
./util.o: util.c angband.h h-basic.h h-config.h h-system.h h-type.h \
  h-define.h z-util.h z-virt.h z-form.h z-rand.h z-term.h config.h \
  defines.h types.h externs.h
./cave.o: cave.c angband.h h-basic.h h-config.h h-system.h h-type.h \
  h-define.h z-util.h z-virt.h z-form.h z-rand.h z-term.h config.h \
  defines.h types.h externs.h
./object1.o: object1.c angband.h h-basic.h h-config.h h-system.h h-type.h \
  h-define.h z-util.h z-virt.h z-form.h z-rand.h z-term.h config.h \
  defines.h types.h externs.h
./object2.o: object2.c angband.h h-basic.h h-config.h h-system.h h-type.h \
  h-define.h z-util.h z-virt.h z-form.h z-rand.h z-term.h config.h \
  defines.h types.h externs.h
./monster1.o: monster1.c angband.h h-basic.h h-config.h h-system.h h-type.h \
  h-define.h z-util.h z-virt.h z-form.h z-rand.h z-term.h config.h \
  defines.h types.h externs.h
./monster2.o: monster2.c angband.h h-basic.h h-config.h h-system.h h-type.h \
  h-define.h z-util.h z-virt.h z-form.h z-rand.h z-term.h config.h \
  defines.h types.h externs.h
./xtra1.o: xtra1.c angband.h h-basic.h h-config.h h-system.h h-type.h \
  h-define.h z-util.h z-virt.h z-form.h z-rand.h z-term.h config.h \
  defines.h types.h externs.h
./xtra2.o: xtra2.c angband.h h-basic.h h-config.h h-system.h h-type.h \
  h-define.h z-util.h z-virt.h z-form.h z-rand.h z-term.h config.h \
  defines.h types.h externs.h
./spells1.o: spells1.c angband.h h-basic.h h-config.h h-system.h h-type.h \
  h-define.h z-util.h z-virt.h z-form.h z-rand.h z-term.h config.h \
  defines.h types.h externs.h
./spells2.o: spells2.c angband.h h-basic.h h-config.h h-system.h h-type.h \
  h-define.h z-util.h z-virt.h z-form.h z-rand.h z-term.h config.h \
  defines.h types.h externs.h
./melee1.o: melee1.c angband.h h-basic.h h-config.h h-system.h h-type.h \
  h-define.h z-util.h z-virt.h z-form.h z-rand.h z-term.h config.h \
  defines.h types.h externs.h
./melee2.o: melee2.c angband.h h-basic.h h-config.h h-system.h h-type.h \
  h-define.h z-util.h z-virt.h z-form.h z-rand.h z-term.h config.h \
  defines.h types.h externs.h
./save.o: save.c angband.h h-basic.h h-config.h h-system.h h-type.h \
  h-define.h z-util.h z-virt.h z-form.h z-rand.h z-term.h config.h \
  defines.h types.h externs.h
./files.o: files.c angband.h h-basic.h h-config.h h-system.h h-type.h \
  h-define.h z-util.h z-virt.h z-form.h z-rand.h z-term.h config.h \
  defines.h types.h externs.h
./cmd1.o: cmd1.c angband.h h-basic.h h-config.h h-system.h h-type.h \
  h-define.h z-util.h z-virt.h z-form.h z-rand.h z-term.h config.h \
  defines.h types.h externs.h
./cmd2.o: cmd2.c angband.h h-basic.h h-config.h h-system.h h-type.h \
  h-define.h z-util.h z-virt.h z-form.h z-rand.h z-term.h config.h \
  defines.h types.h externs.h
./cmd3.o: cmd3.c angband.h h-basic.h h-config.h h-system.h h-type.h \
  h-define.h z-util.h z-virt.h z-form.h z-rand.h z-term.h config.h \
  defines.h types.h externs.h
./cmd4.o: cmd4.c angband.h h-basic.h h-config.h h-system.h h-type.h \
  h-define.h z-util.h z-virt.h z-form.h z-rand.h z-term.h config.h \
  defines.h types.h externs.h
./cmd5.o: cmd5.c angband.h h-basic.h h-config.h h-system.h h-type.h \
  h-define.h z-util.h z-virt.h z-form.h z-rand.h z-term.h config.h \
  defines.h types.h externs.h
./cmd6.o: cmd6.c angband.h h-basic.h h-config.h h-system.h h-type.h \
  h-define.h z-util.h z-virt.h z-form.h z-rand.h z-term.h config.h \
  defines.h types.h externs.h
./birth.o: birth.c angband.h h-basic.h h-config.h h-system.h h-type.h \
  h-define.h z-util.h z-virt.h z-form.h z-rand.h z-term.h config.h \
  defines.h types.h externs.h
./load.o: load.c angband.h h-basic.h h-config.h h-system.h h-type.h \
  h-define.h z-util.h z-virt.h z-form.h z-rand.h z-term.h config.h \
  defines.h types.h externs.h init.h
./squelch.o: squelch.c angband.h h-basic.h h-config.h h-system.h h-type.h \
  h-define.h z-util.h z-virt.h z-form.h z-rand.h z-term.h config.h \
  defines.h types.h externs.h
./wizard1.o: wizard1.c angband.h h-basic.h h-config.h h-system.h h-type.h \
  h-define.h z-util.h z-virt.h z-form.h z-rand.h z-term.h config.h \
  defines.h types.h externs.h
./wizard2.o: wizard2.c angband.h h-basic.h h-config.h h-system.h h-type.h \
  h-define.h z-util.h z-virt.h z-form.h z-rand.h z-term.h config.h \
  defines.h types.h externs.h
./obj-info.o: obj-info.c angband.h h-basic.h h-config.h h-system.h h-type.h \
  h-define.h z-util.h z-virt.h z-form.h z-rand.h z-term.h config.h \
  defines.h types.h externs.h
./generate.o: generate.c angband.h h-basic.h h-config.h h-system.h h-type.h \
  h-define.h z-util.h z-virt.h z-form.h z-rand.h z-term.h config.h \
  defines.h types.h externs.h
./dungeon.o: dungeon.c angband.h h-basic.h h-config.h h-system.h h-type.h \
  h-define.h z-util.h z-virt.h z-form.h z-rand.h z-term.h config.h \
  defines.h types.h externs.h
./init1.o: init1.c angband.h h-basic.h h-config.h h-system.h h-type.h \
  h-define.h z-util.h z-virt.h z-form.h z-rand.h z-term.h config.h \
  defines.h types.h externs.h init.h
./init2.o: init2.c angband.h h-basic.h h-config.h h-system.h h-type.h \
  h-define.h z-util.h z-virt.h z-form.h z-rand.h z-term.h config.h \
  defines.h types.h externs.h init.h
./randart.o: randart.c angband.h h-basic.h h-config.h h-system.h h-type.h \
  h-define.h z-util.h z-virt.h z-form.h z-rand.h z-term.h config.h \
  defines.h types.h externs.h init.h
./use-obj.o: use-obj.c angband.h h-basic.h h-config.h h-system.h h-type.h \
  h-define.h z-util.h z-virt.h z-form.h z-rand.h z-term.h config.h \
  defines.h types.h externs.h
./z-util.o: z-util.c z-util.h h-basic.h h-config.h h-system.h h-type.h \
  h-define.h
./z-virt.o: z-virt.c z-virt.h h-basic.h h-config.h h-system.h h-type.h \
  h-define.h z-util.h
./z-form.o: z-form.c z-form.h h-basic.h h-config.h h-system.h h-type.h \
  h-define.h z-util.h z-virt.h
./z-rand.o: z-rand.c z-rand.h h-basic.h h-config.h h-system.h h-type.h \
  h-define.h
./z-term.o: z-term.c z-term.h h-basic.h h-config.h h-system.h h-type.h \
  h-define.h z-virt.h
