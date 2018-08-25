# ezmake.mk
# Allen Wild
# Simple C/C++ Makefile framework with automake-like syntax
#
# Copyright (c) 2017 Allen Wild <allenwild93@gmail.com>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

# First target is to build, unless user specified something else
ez-build: $(CC_PROGRAMS) $(CXX_PROGRAMS) $(CC_ALIBS) $(CXX_ALIBS) $(CC_SOLIBS) $(CXX_SOLIBS)
.PHONY: ez-build

# Macro to get per-target XX_YYFLAGS (also applies for LDADD)
#  $1 = the target name
#  $2 = variable name, e.g. CFLAGS, CXXFLAGS
define target_flags
ifeq ($$($(1)_$(2)),)
$(1)_$(2) = $$(AM_$(2))
endif
endef

# Macro to convert a target patch to a sanitized makefile variable name
# Replace everything that's not a letter or a number with an underscore
define san_target_name
$$(shell echo '$(1)' | sed 's/[^A-Za-z0-9_]/_/g')
endef

# Common object/compilation rules
#  $(1) = sanitized name of target (replace invalid characters with underscores)
#  $(2) = source file extension
#  $(3) = FLAGS prefix: C or CXX
#  $(4) = compiler variable name: CC or CXX
#  $(5) = real name of target
define target_common
$(1)_OBJECTS = $$(patsubst %.$(2),%.o,$$($(1)_SOURCES))
$(1)_DEPS    = $$(patsubst %.o,%.d,$$($(1)_OBJECTS))
CLEANFILES  += $(5) $$($(1)_OBJECTS) $$($(1)_DEPS)

# Define target-specific flags variables
$$(foreach flag,CFLAGS CPPFLAGS CXXFLAGS LDFLAGS LDADD,$$(eval $$(call target_flags,$(1),$$(flag))))

# file compilation rule
$$($(1)_OBJECTS) : %.o : %.$(2)
	$$($(4)) -MD -MP $$(strip $$($(1)_CPPFLAGS) $$(CPPFLAGS) $$($(1)_$(3)FLAGS) $$($(3)FLAGS)) -c -o $$@ $$<

# include dependency files
-include $$($(1)_DEPS)
endef # target_common

# C program
#  $(1) = sanitized target name
#  $(2) = real target name
define cc_program
$$(eval $$(call target_common,$(1),c,C,CC,$(2)))
$(2) : $$($(1)_OBJECTS)
	$$(CC) $$(strip $$($(1)_CFLAGS) $$(CFLAGS) $$($(1)_LDFLAGS) $$(LDFLAGS)) -o $$@ $$(filter %.o,$$^) $$($(1)_LDADD) $$(LIBS)
endef # cc_program

# C++ program
define cxx_program
$$(eval $$(call target_common,$(1),cpp,CXX,CXX,$(2)))
$(2) : $$($(1)_OBJECTS)
	$$(CXX) $$(strip $$($(1)_CXXFLAGS) $$(CXXFLAGS) $$($(1)_LDFLAGS) $$(LDFLAGS)) -o $$@ $$(filter %.o,$$^) $$($(1)_LDADD) $$(LIBS)
endef # cxx_program

# C static library
define cc_alib
$$(eval $$(call target_common,$(1),c,C,CC,$(2)))
$(2) : $$($(1)_OBJECTS)
	$$(AR) rcs $$@ $$(filter %.o,$$^)
endef

# C++ static library
define cxx_alib
$$(eval $$(call target_common,$(1),cpp,CXX,CXX,$(2)))
$(2) : $$($(1)_OBJECTS)
	$$(AR) rcs $$@ $$(filter %.o,$$^)
endef

# C shared library
define cc_solib
$$(eval $$(call target_common,$(1),c,C,CC,$(2)))

# Shared libraries need to have -fPIC somewhere in CFLAGS
ifeq ($$(findstring -fPIC,$$($(1)_CFLAGS)),)
ifeq ($$(findstring -fPIC,$$(CFLAGS)),)
$(1)_CFLAGS += -fPIC
endif
else
ifeq ($$(findstring -fPIC,$$($(1)_CFLAGS)),)
ifeq ($$(findstring -fPIC,$$(CFLAGS)),)
$(1)_CFLAGS += -fPIC
endif
endif
endif
$(2) : $$($(1)_OBJECTS)
	$$(CC) $$(strip $$($(1)_CFLAGS) $$(CFLAGS) $$($(1)_LDFLAGS) $$(LDFLAGS)) -shared -o $$@ $$(filter %.o,$$^) $$($(1)_LDADD) $$(LIBS)
endef # cc_solib

# C++ shared library
define cxx_solib
$$(eval $$(call target_common,$(1),cpp,CXX,CXX,$(2)))

# Shared libraries need to have -fPIC somewhere in CXXFLAGS
ifeq ($$(findstring -fPIC,$$($(1)_CXXFLAGS)),)
ifeq ($$(findstring -fPIC,$$(CXXFLAGS)),)
$(1)_CXXFLAGS += -fPIC
endif
else
ifeq ($$(findstring -fPIC,$$($(1)_CXXFLAGS)),)
ifeq ($$(findstring -fPIC,$$(CXXFLAGS)),)
$(1)_CXXFLAGS += -fPIC
endif
endif
endif
$(2) : $$($(1)_OBJECTS)
	$$(CXX) $$(strip $$($(1)_CXXFLAGS) $$(CXXFLAGS) $$($(1)_LDFLAGS) $$(LDFLAGS)) -shared -o $$@ $$(filter %.o,$$^) $$($(1)_LDADD) $$(LIBS)
endef # cxx_solib

# Use the above macros to generate the appropriate rules
$(foreach prog,$(CC_PROGRAMS),$(eval $(call cc_program,$(call san_target_name,$(prog)),$(prog))))
$(foreach prog,$(CXX_PROGRAMS),$(eval $(call cxx_program,$(call san_target_name,$(prog)),$(prog))))
$(foreach prog,$(CC_ALIBS),$(eval $(call cc_alib,$(call san_target_name,$(prog)),$(prog))))
$(foreach prog,$(CXX_ALIBS),$(eval $(call cxx_alib,$(call san_target_name,$(prog)),$(prog))))
$(foreach prog,$(CC_SOLIBS),$(eval $(call cc_solib,$(call san_target_name,$(prog)),$(prog))))
$(foreach prog,$(CXX_SOLIBS),$(eval $(call cxx_solib,$(call san_target_name,$(prog)),$(prog))))

# clean and other targets
clean:
	rm -f $(CLEANFILES)
.PHONY: clean
