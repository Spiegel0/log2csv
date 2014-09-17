# @brief The Makefile builds the CSVLogger application for different platforms 
# @details The phony target all creates the program and all it's files. clean 
# deletes every previously generated file by running a simple rm command. This 
# Makefile was primary developed and tested with GNU Make under Linux 3.8. It 
# may not work using other platforms.
#
# @author Michael Spiegel, michael.h.spiegel@gmail.com
#
# Copyright (C) 2014 Michael Spiegel
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; version 2 of the License
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
#

# The compiler command
#CC = gcc
CC=arm-linux-gnueabihf-gcc

# The compiler flags
CFLAGS = -std=c99 -pedantic -Wall -I $(INCLUDEDIR)
CFLAGS += -D_XOPEN_SOURCE=500 -D_BSD_SOURCE
CFLAGS += -DENDEBUG 
CFLAGS += -fpack-struct

LDFLAGS += $(LIB:%=-l%)
LDFLAGS += -Llib-arm
#Used to access main module functions from outside
LDFLAGS += -Wl,-export-dynamic

# @brief The list of source files neccessary to build the program. 
# It has to be updated manually
CFILES = main.c logging-adapter.c pluggable-fieldbus-manager.c

# @brief The list of external libraries 
LIB = config dl

# @brief The name of the program to build
PRGNAME = log2csv

# @brief The name of the binary folder to store some .o and .d files in
BINDIR = bin

# @brief The name of the source folder containing the program's .c and .h files
SRCDIR = src

# @brief The name of the include directory
INCLUDEDIR = includes

# @brief The name of the documentation directory
DOCDIR = doc

# @ brief The program's object files 
OBJ = $(CFILES:%.c=$(BINDIR)/%.o)

DEPFILES = $(CFILES:%.c=$(BINDIR)/%.d)

# @brief The list of goals where the include directive is omitted
NOINCLUDEDEPS = clean docu

vpath %.c $(SRCDIR)
vpath %.h src $(SRCDIR)

vpath %.d $(BINDIR)
vpath %.o $(BINDIR)

# If the goal is listed as no include target the include directive is skipped
# If a sequence of goals is given and this sequence is not exactly listed in 
# NOINCLUDEDEPS the include is performed anyway
ifeq (,findstring($(MAKECMDGOALS),$(NOINCLUDEDEPS)))
	-include $(DEPFILES)
endif

# @brief Rule to create and compile everything
all: $(PRGNAME)

# @brief Rule to create the program
# @details The LDFLAGS parameter must be passed at the end of arguments list to 
# ensure loading the library after the onject files are read. Otherwise needed 
# functions arn't loaded. (gcc parses the arguments in order.)
$(PRGNAME): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS)

# @brief Rule to compile the modules 
# @details The binary directory won't be updated iff the timestamp changes
$(BINDIR)/%.o: %.c $(BINDIR)/%.d | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ -c $<

# @brief Rule to create the dependency files 
# @details The binary directory won't be updated iff the timestamp changes
$(BINDIR)/%.d: %.c | $(BINDIR)
	$(CC) $(CFLAGS) -MM $< > $@

# @brief Rule to create the binary directory
$(BINDIR):
	mkdir -p $(BINDIR)

# @brief Rule to automatically generate the documentation
docu: $(DOCDIR) $(SRCDIR)
	doxygen

# @brief Rule to generate the directory containing the documentation files 
$(DOCDIR):
	mkdir -p $(DOCDIR)

# @brief Removes every automatically generated file and directory
clean:
	rm -rf $(BINDIR)
	rm -rf $(DOCDIR)
	rm -f $(PRGNAME)

.PHONY: all clean docu
	