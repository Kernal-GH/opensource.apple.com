/* loader-dlopen.c --  dynamic linking with dlopen/dlsym
   Copyright (C) 1998, 1999, 2000, 2004, 2006 Free Software Foundation, Inc.
   Originally by Thomas Tanner <tanner@ffii.org>

   NOTE: The canonical source of this file is maintained with the
   GNU Libtool package.  Report bugs to bug-libtool@gnu.org.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

As a special exception to the GNU Lesser General Public License,
if you distribute this file as part of a program or library that
is built using GNU libtool, you may include it under the same
distribution terms that you use for the rest of that program.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
02110-1301  USA

*/

#include "lt_dlloader.h"
#include "lt__private.h"

/* Use the preprocessor to rename non-static symbols to avoid namespace
   collisions when the loader code is statically linked into libltdl.
   Use the "<module_name>_LTX_" prefix so that the symbol addresses can
   be fetched from the preloaded symbol list by lt_dlsym():  */
#define get_vtable	dlopen_LTX_get_vtable

LT_BEGIN_C_DECLS
LT_SCOPE lt_dlvtable *get_vtable (lt_user_data loader_data);
LT_END_C_DECLS


/* Boilerplate code to set up the vtable for hooking this loader into
   libltdl's loader list:  */
static lt_module vm_open  (lt_user_data loader_data, const char *filename);
static int	 vm_close (lt_user_data loader_data, lt_module module);
static void *	 vm_sym   (lt_user_data loader_data, lt_module module,
			  const char *symbolname);

/* Return the vtable for this loader, only the name and sym_prefix
   attributes (plus the virtual function implementations, obviously)
   change between loaders.  */
lt_dlvtable *
get_vtable (lt_user_data loader_data)
{
  static lt_dlvtable *vtable = 0;

  if (!vtable)
    {
      vtable = (lt_dlvtable *) lt__zalloc (sizeof *vtable);
    }

  if (vtable && !vtable->name)
    {
      vtable->name		= "lt_dlopen";
#if defined(DLSYM_USCORE)
      vtable->sym_prefix	= "_";
#endif
      vtable->module_open	= vm_open;
      vtable->module_close	= vm_close;
      vtable->find_sym		= vm_sym;
      vtable->dlloader_data	= loader_data;
      vtable->priority		= LT_DLLOADER_PREPEND;
    }

  if (vtable && (vtable->dlloader_data != loader_data))
    {
      LT__SETERROR (INIT_LOADER);
      return 0;
    }

  return vtable;
}



/* --- IMPLEMENTATION --- */


#if defined(HAVE_DLFCN_H)
#  include <dlfcn.h>
#endif

#if defined(HAVE_SYS_DL_H)
#  include <sys/dl.h>
#endif


/* We may have to define LT_LAZY_OR_NOW in the command line if we
   find out it does not work in some platform. */
#if !defined(LT_LAZY_OR_NOW)
#  if defined(RTLD_LAZY)
#    define LT_LAZY_OR_NOW	RTLD_LAZY
#  else
#    if defined(DL_LAZY)
#      define LT_LAZY_OR_NOW	DL_LAZY
#    endif
#  endif /* !RTLD_LAZY */
#endif
#if !defined(LT_LAZY_OR_NOW)
#  if defined(RTLD_NOW)
#    define LT_LAZY_OR_NOW	RTLD_NOW
#  else
#    if defined(DL_NOW)
#      define LT_LAZY_OR_NOW	DL_NOW
#    endif
#  endif /* !RTLD_NOW */
#endif
#if !defined(LT_LAZY_OR_NOW)
#  define LT_LAZY_OR_NOW	0
#endif /* !LT_LAZY_OR_NOW */

/* Open MPI */
#if !defined(LT_GLOBAL)
#  if defined(RTLD_GLOBAL)
#    define LT_GLOBAL	RTLD_GLOBAL
#  else
#    if defined(DL_GLOBAL)
#      define LT_GLOBAL	DL_GLOBAL
#    endif
#  endif /* !RTLD_GLOBAL */
#endif

#if defined(HAVE_DLERROR)
#  define DLERROR(arg)	dlerror ()
#else
#  define DLERROR(arg)	LT__STRERROR (arg)
#endif

#define DL__SETERROR(errorcode) \
	LT__SETERRORSTR (DLERROR (errorcode))

/* A function called through the vtable to open a module with this
   loader.  Returns an opaque representation of the newly opened
   module for processing with this loader's other vtable functions.  */
static lt_module
vm_open (lt_user_data LT__UNUSED loader_data, const char *filename)
{
  lt_module module = dlopen (filename, LT_GLOBAL | LT_LAZY_OR_NOW);

  if (!module)
    {
      DL__SETERROR (CANNOT_OPEN);
    }

  return module;
}


/* A function called through the vtable when a particular module
   should be unloaded.  */
static int
vm_close (lt_user_data LT__UNUSED loader_data, lt_module module)
{
  int errors = 0;

  if (dlclose (module) != 0)
    {
      DL__SETERROR (CANNOT_CLOSE);
      ++errors;
    }

  return errors;
}


/* A function called through the vtable to get the address of
   a symbol loaded from a particular module.  */
static void *
vm_sym (lt_user_data LT__UNUSED loader_data, lt_module module, const char *name)
{
  void *address = dlsym (module, name);

  if (!address)
    {
      DL__SETERROR (SYMBOL_NOT_FOUND);
    }

  return address;
}
