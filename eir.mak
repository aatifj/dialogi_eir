#
#                       Copyright (C) Dialogic Corporation 2001-2010.  All Rights Reserved.
#
#   File:               mtr.mak
#
#   Makefile to build:  MTR example
#   for use with:       C compiler.
#   with libraries:     from the Development package
#   Output file:        mtr (executable)
#
#   -------+---------+------+------------------------------------------
#   Issue     Date      By    Description
#   -------+---------+------+------------------------------------------
#     1     03-Apr-01   MH   - Initial makefile.
#     2     29-Mar-10   JLP  - Updated comments

# Include common definitions:
include ../makdefs.mak

TARGET = $(BINPATH)/eir

all :   $(TARGET)

clean:
	rm -rf *.o
	rm -rf $(TARGET)

OBJS = eir.o eir_main.o

$(TARGET): $(OBJS)
	$(LINKER) $(LFLAGS) -o $@ $(OBJS) $(LIBS) $(SYSLIBS)


