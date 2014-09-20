# @brief The Makefile builds the DLogg data converter modules 
# @details The phony target all creates the shared library and all it's files. 
# clean deletes every previously generated file by running a simple rm command. 
# This Makefile was primary developed and tested with GNU Make under Linux 3.8. 
# It may not work using other platforms.
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
# @brief The compiler flags
CFLAGS = -std=c99 -pedantic -Wall -I $(INCLUDEDIR)
CFLAGS += -D_XOPEN_SOURCE=500 -D_BSD_SOURCE
CFLAGS += -fPIC 
CFLAGS += -DENDEBUG 
CFLAGS += -fpack-struct
# @brief The linker flags
LDFLAGS += -shared
LDFLAGS += $(LIB:%=-l%)
LDFLAGS += -L../CSVLogger/lib-arm

# @brief The list of source files neccessary to build the MAC library. 
# It has to be updated manually
CFILES_MAC = dlogg-mac.c dlogg-current-data.c
# @brief The list of source files neccessary to build the Temperature access
# library. It has to be updated manually
CFILES_STDVAL = dlogg-stdval.c

# @brief The list of external libraries 
LIB = config

# @brief The name of the MAC library to build
PRGNAME_MAC=dlogg.so
# @brief The name of the access module to build
PRGNAME_STDVAL=dlog-stdval.so


# @brief The name of the binary folder to store some .o and .d files in
BINDIR = bin

# @brief The name of the source folder containing the program's .c and .h files
SRCDIR = src

# @brief The name of the include directory
INCLUDEDIR = ../CSVLogger/includes

# @brief The name of the documentation directory
DOCDIR = doc

# @brief The name of the doxygen configuration file used to generate the 
# documentation
DOXY_CFG = doxygen-def.cfg

# @ brief The librarie's object files 
OBJ_MAC = $(CFILES_MAC:%.c=$(BINDIR)/%.o)
# @ brief The librarie's object files 
OBJ_STDVAL = $(CFILES_STDVAL:%.c=$(BINDIR)/%.o)


DEPFILES_MAC = $(CFILES_MAC:%.c=$(BINDIR)/%.d)
DEPFILES_STDVAL = $(CFILES_STDVAL:%.c=$(BINDIR)/%.d)

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
all: binary docu

# @brief compiles every binary program file and library
binary: $(PRGNAME_MAC) $(PRGNAME_STDVAL)

# @brief Rule to create the MAC library
# @details The LDFLAGS parameter must be passed at the end of arguments list to 
# ensure loading the library after the onject files are read. Otherwise needed 
# functions arn't loaded. (gcc parses the arguments in order.)
$(PRGNAME_MAC): $(OBJ_MAC)
	$(CC) $(OBJ_MAC) -o $@ $(LDFLAGS)

# @brief Rule to create the temperature access library
# @details The LDFLAGS parameter must be passed at the end of arguments list to 
# ensure loading the library after the onject files are read. Otherwise needed 
# functions arn't loaded. (gcc parses the arguments in order.)
$(PRGNAME_STDVAL): $(OBJ_STDVAL)
	$(CC) $(OBJ_STDVAL) -o $@ $(LDFLAGS)

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
docu: $(DOCDIR) $(SRCDIR) $(DOXY_CFG)
	doxygen $(DOXY_CFG)

# @brief Rule to generate the directory containing the documentation files 
$(DOCDIR):
	mkdir -p $(DOCDIR)

# @brief Removes every automatically generated file and directory
clean:
	rm -rf $(BINDIR)
	rm -rf $(DOCDIR)
	rm -f $(PRGNAME_MAC) $(PRGNAME_STDVAL)

.PHONY: all clean docu binary
	