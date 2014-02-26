CONTIKI = ../..

CONTIKI_PROJECT = bus

# 1 = Success Only
# 2 = Print Q
# 3 = Verbose Mode

#CONTIKI_PROJECT = dtn
all: $(CONTIKI_PROJECT)

CONTIKI_SOURCEFILES += dtn.c

include $(CONTIKI)/Makefile.include

