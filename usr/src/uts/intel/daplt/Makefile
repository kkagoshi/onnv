#
# CDDL HEADER START
#
# The contents of this file are subject to the terms of the
# Common Development and Distribution License (the "License").
# You may not use this file except in compliance with the License.
#
# You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
# or http://www.opensolaris.org/os/licensing.
# See the License for the specific language governing permissions
# and limitations under the License.
#
# When distributing Covered Code, include this CDDL HEADER in each
# file and include the License file at usr/src/OPENSOLARIS.LICENSE.
# If applicable, add the following below this CDDL HEADER, with the
# fields enclosed by brackets "[]" replaced with your own identifying
# information: Portions Copyright [yyyy] [name of copyright owner]
#
# CDDL HEADER END
#

#
# Copyright 2007 Sun Microsystems, Inc.  All rights reserved.
# Use is subject to license terms.
#

#
#	Paths to the base of the uts directory trees
#
UTSBASE = ../..

#
#	Define the module and object file sets.
#
MODULE		= daplt
OBJECTS		= $(DAPLT_OBJS:%=$(OBJS_DIR)/%)
LINTS		= $(DAPLT_OBJS:%.o=$(LINTS_DIR)/%.ln)
ROOTMODULE	= $(ROOT_DRV_DIR)/$(MODULE)
CONF_SRCDIR	= $(UTSBASE)/common/io/ib/clients/daplt
LDFLAGS		+= -dy -Nmisc/ibtl -Nmisc/ibcm
WARLOCK_OUT     = $(DAPLT_OBJS:%.o=%.ll)
WARLOCK_OK	= $(MODULE).ok
WLCMD_DIR	= $(UTSBASE)/common/io/warlock
#
#	Include common rules.
#
include $(UTSBASE)/intel/Makefile.intel

#
#	Define targets
#
ALL_TARGET	= $(BINARY) $(SRC_CONFILE)
LINT_TARGET	= $(MODULE).lint
INSTALL_TARGET	= $(BINARY) $(ROOTMODULE) $(ROOT_CONFFILE)

#
# path for header files
#
INCLUDE_PATH += -I$(UTSBASE)/common/sys/ib/clients/daplt

#
#       Overrides.
#

#
# For now, disable these lint checks; maintainers should endeavor
# to investigate and remove these for maximum lint coverage.
# Please do not carry these forward to new Makefiles.
#
LINTTAGS	+= -erroff=E_BAD_PTR_CAST_ALIGN
LINTTAGS	+= -erroff=E_ASSIGN_NARROW_CONV
LINTTAGS	+= -erroff=E_STATIC_UNUSED

#
#	Default build targets.
#
.KEEP_STATE:

def:		$(DEF_DEPS)

all:		$(ALL_DEPS)

clean:		$(CLEAN_DEPS)
		$(RM) $(WARLOCK_OUT) $(WARLOCK_OK)

clobber:	$(CLOBBER_DEPS)
		$(RM) $(WARLOCK_OUT) $(WARLOCK_OK)

lint:		$(LINT_DEPS)

modlintlib:	$(MODLINTLIB_DEPS)

clean.lint:	$(CLEAN_LINT_DEPS)

install:	$(INSTALL_DEPS)

#
#	Include common targets.
#
include $(UTSBASE)/intel/Makefile.targ

#
#       Defines for local commands.
#
WARLOCK         = warlock
WLCC            = wlcc
TOUCH           = touch
TEST            = test

warlock: $(WARLOCK_OK)

$(WARLOCK_OK): $(WARLOCK_OUT) $(WLCMD_DIR)/daplt.wlcmd warlock_ddi.files
	$(WARLOCK) -c $(WLCMD_DIR)/daplt.wlcmd $(WARLOCK_OUT) \
		-l $(UTSBASE)/intel/warlock/ddi_dki_impl.ll
	$(TOUCH) $@

%.ll: $(UTSBASE)/common/io/ib/clients/daplt/%.c \
	$(UTSBASE)/common/sys/ib/clients/daplt/daplt.h \
	$(UTSBASE)/common/sys/ib/clients/daplt/daplt_if.h
	$(WLCC)  $(CPPFLAGS) -DDEBUG -o $@ $<
        
warlock_ddi.files:
	@cd $(UTSBASE)/intel/warlock; pwd; $(MAKE) warlock
