#
# Make sure all needed macro's are defined
#

#
# Get the 'head' of the build environment if necessary.  This includes default
# targets and paths to tools
#

ifndef EnvironmentDefined
include $(AP_WORK)\NWGNUhead.inc
endif

#
# These directories will be at the beginning of the include list, followed by
# INCDIRS
#
XINCDIRS	+= \
			$(AP) \
			$(SRC)\include \
			$(NWOS) \
			$(EOLIST)

#
# These flags will come after CFLAGS
#
XCFLAGS		+= \
			$(EOLIST)

#
# These defines will come after DEFINES
#
XDEFINES	+= \
			$(EOLIST)

#
# These flags will be added to the link.opt file
#
XLFLAGS		+= \
			$(EOLIST)

#
# These values will be appended to the correct variables based on the value of
# RELEASE
#
ifeq "$(RELEASE)" "debug"
XINCDIRS	+= \
			$(EOLIST)

XCFLAGS		+= \
			$(EOLIST)

XDEFINES	+= \
			$(EOLIST)

XLFLAGS		+= \
			$(EOLIST)
endif

ifeq "$(RELEASE)" "noopt"
XINCDIRS	+= \
			$(EOLIST)

XCFLAGS		+= \
			$(EOLIST)

XDEFINES	+= \
			$(EOLIST)

XLFLAGS		+= \
			$(EOLIST)
endif

ifeq "$(RELEASE)" "release"
XINCDIRS	+= \
			$(EOLIST)

XCFLAGS		+= \
			$(EOLIST)

XDEFINES	+= \
			$(EOLIST)

XLFLAGS		+= \
			$(EOLIST)
endif

#
# These are used by the link target if an NLM is being generated
# This is used by the link 'name' directive to name the nlm.  If left blank
# TARGET_nlm (see below) will be used.
#
NLM_NAME		= htpasswd

#
# This is used by the link '-desc ' directive. 
# If left blank, NLM_NAME will be used.
#
NLM_DESCRIPTION	= Password generator for Apache

#
# This is used by the '-threadname' directive.  If left blank,
# NLM_NAME Thread will be used.
#
NLM_THREAD_NAME	= htpasswd

#
# If this is specified, it will override VERSION value in 
# $(AP_WORK)\NWGNUenvironment.inc
#
NLM_VERSION		= 1,0,0

#
# If this is specified, it will override the default of 64K
#
NLM_STACK_SIZE	= 8192

#
# If this is specified it will be used by the link '-entry' directive
#
NLM_ENTRY_SYM	=

#
# If this is specified it will be used by the link '-exit' directive
#
NLM_EXIT_SYM	=

#
# If this is specified it will be used by the link '-flags' directive
#
NLM_FLAGS		=

#
# Declare all target files (you must add your files here)
#

#
# If there is an NLM target, put it here
#
TARGET_nlm = \
	$(OBJDIR)/htpasswd.nlm \
	$(EOLIST)

#
# If there is an LIB target, put it here
#
TARGET_lib = \
	$(EOLIST)

#
# These are the OBJ files needed to create the NLM target above.
# Paths must all use the '/' character
#
FILES_nlm_objs = \
	$(OBJDIR)/htpasswd.o \
	$(OBJDIR)/ap_snprintf.o \
	$(OBJDIR)/ap_checkpass.o \
	$(OBJDIR)/ap_cpystrn.o \
	$(OBJDIR)/ap_getpass.o \
	$(OBJDIR)/ap_md5c.o \
	$(OBJDIR)/ap_sha1.o \
	$(OBJDIR)/ap_base64.o \
	$(EOLIST)

#
# These are the LIB files needed to create the NLM target above.
# These will be added as a library command in the link.opt file.
#
FILES_nlm_libs = \
	clibpre.o \
	$(EOLIST)

#
# These are the modules that the above NLM target depends on to load.
# These will be added as a module command in the link.opt file.
#
FILES_nlm_modules = \
	$(EOLIST)

#
# If the nlm has a msg file, put it's path here
#
FILE_nlm_msg =
 
#
# If the nlm has a hlp file put it's path here
#
FILE_nlm_hlp =

#
# If this is specified, it will override $(NWOS)\copyright.txt.
#
FILE_nlm_copyright =

#
# Any additional imports go here
#
FILES_nlm_Ximports = \
	@nlmlib.imp \
	@clib.imp \
	@threads.imp \
	@lib0.imp \
	@ws2nlm.imp \
	$(EOLIST)
 
#   
# Any symbols exported to here
#
FILES_nlm_exports = \
	$(EOLIST)
	
#   
# These are the OBJ files needed to create the LIB target above.
# Paths must all use the '/' character
#
FILES_lib_objs = \
	$(EOLIST)

#
# implement targets and dependancies (leave this section alone)
#

libs :: $(OBJDIR) $(TARGET_lib)

nlms :: libs $(TARGET_nlm)

#
# Updated this target to create necessary directories and copy files to the 
# correct place.  (See $(AP_WORK)\NWGNUhead.inc for examples)
#
install :: nlms FORCE

#
# Any specialized rules here
#

$(OBJDIR)/%.o: $(AP)\%.c $(OBJDIR)\cc.opt
	@echo compiling $<
	$(CC) $< -o=$(OBJDIR)\$(@F) @$(OBJDIR)\cc.opt

%.d: $(AP)\%.c $(OBJDIR)\cc.opt
	@echo Creating dependancy list for $(notdir $<)
	$(CC) $< -o $*.tmp -M @$(OBJDIR)\cc.opt
	$(GNUTOOLS)/sed 's/$*.o[ :]*/$(OBJDIR)\/$*.o : $@ /g' $*.tmp > $@ 
	-$(DEL) $*.tmp

#
# Include the 'tail' makefile that has targets that depend on variables defined
# in this makefile
#

include $(AP_WORK)\NWGNUtail.inc

