dnl **************************************************************************
dnl * Initialize
dnl **************************************************************************
AC_INIT([[[loom]]],[[[0.10]]],[wujingyue@gmail.com])

dnl Tell autoconf that this is an LLVM project being configured
dnl This provides the --with-llvmsrc and --with-llvmobj options
LLVM_SRC_ROOT=`llvm-config --src-root`
LLVM_OBJ_ROOT=`llvm-config --obj-root`
LLVM_CONFIG_PROJECT($LLVM_SRC_ROOT,$LLVM_OBJ_ROOT)

dnl Tell autoconf that this project uses the RCS common project
dnl This provides the --with-rcssrc and --with-rcsobj options
RCS_SRC_ROOT="."
RCS_OBJ_ROOT="."
RCS_CONFIG_PROJECT($RCS_SRC_ROOT,$RCS_OBJ_ROOT)

dnl Tell autoconf that the auxilliary files are actually located in
dnl the LLVM autoconf directory, not here.
AC_CONFIG_AUX_DIR($LLVM_SRC/autoconf)

dnl Verify that the source directory is valid
AC_CONFIG_SRCDIR(["Makefile.common.in"])

dnl Configure a common Makefile
AC_CONFIG_FILES(Makefile.common)

dnl Configure the header file
AC_CONFIG_HEADERS([include/loom/config.h])
AH_TOP([#ifndef __LOOM_CONFIG_H
#define __LOOM_CONFIG_H])
AH_BOTTOM([#endif])

AC_ARG_WITH(ctrl-ip, AS_HELP_STRING([--with-ctrl-ip], [Controller's IP (default = localhost)]), , [with_ctrl_ip=localhost])
AC_MSG_CHECKING([controler's ip])
AC_MSG_RESULT([$with_ctrl_ip])
AC_DEFINE_UNQUOTED(CONTROLLER_IP, "$with_ctrl_ip", [Controller's IP])

AC_ARG_WITH(ctrl-port, AS_HELP_STRING([--with-ctrl-port], [Controller's port (default = 1229)]), , [with_ctrl_port=1229])
AC_MSG_CHECKING([controler's port])
AC_MSG_RESULT([$with_ctrl_port])
AC_DEFINE_UNQUOTED(CONTROLLER_PORT, $with_ctrl_port, [Controller's port])

dnl Configure project makefiles
dnl List every Makefile that exists within your source tree
AC_CONFIG_MAKEFILE(Makefile)

dnl **************************************************************************
dnl * Determine which system we are building on
dnl **************************************************************************

dnl **************************************************************************
dnl * Check for programs.
dnl **************************************************************************

dnl **************************************************************************
dnl * Check for libraries.
dnl **************************************************************************

dnl **************************************************************************
dnl * Checks for header files.
dnl **************************************************************************

dnl **************************************************************************
dnl * Checks for typedefs, structures, and compiler characteristics.
dnl **************************************************************************

dnl **************************************************************************
dnl * Checks for library functions.
dnl **************************************************************************

dnl **************************************************************************
dnl * Enable various compile-time options
dnl **************************************************************************

dnl **************************************************************************
dnl * Set the location of various third-party software packages
dnl **************************************************************************

dnl **************************************************************************
dnl * Create the output files
dnl **************************************************************************

dnl This must be last
AC_OUTPUT
