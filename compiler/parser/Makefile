# Copyright 2004-2020 Hewlett Packard Enterprise Development LP
# Other additional copyright holders may be indicated within.
#
# The entirety of this work is licensed under the Apache License,
# Version 2.0 (the "License"); you may not use this file except
# in compliance with the License.
#
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

ifndef CHPL_MAKE_HOME
export CHPL_MAKE_HOME=$(shell pwd)/../..
endif

COMPILER_ROOT = ..
COMPILER_SUBDIR = parser

#
# standard header
#
include $(COMPILER_ROOT)/make/Makefile.compiler.head

PARSER_OBJDIR = $(OBJ_SUBDIR)
include Makefile.share

TARGETS = $(PARSER_OBJS)

include $(COMPILER_ROOT)/make/Makefile.compiler.subdirrules

ifdef CHPL_DEVELOPER
CLEAN_TARGS += \
               bison-chapel.output

endif

# Don't create rules for bison-chapel.cpp or flex-chapel.cpp since we don't
# want them to be rebuilt automatically. (This is because of the bison version
# 2.5 requirement)
parser: FORCE flex_version_check
	@rm -f bison-chapel.output bison-chapel.cpp flex-chapel.cpp
	@rm -f ../include/bison-chapel.h ../include/flex-chapel.h
	bison chapel.ypp
	@if grep -q "conflicts:" bison-chapel.output; \
		then echo "PROBLEM: chapel.ypp contains conflicts"; \
		rm -f bison-chapel.cpp; \
		exit 1; \
	fi;
	flex chapel.lex

# Require that the flex version is at least 2.6.0
flex_version_check:
	@if python -c "import sys;                                        \
                       from distutils.version import LooseVersion as LV;  \
                       sys.exit(LV(sys.argv[1]) >= LV(sys.argv[2]))"      \
                   `flex --version | sed -e 's/flex //' -e 's/ .*//'`     \
                   "2.6.0";                                               \
		then echo "PROBLEM: flex version must be at least 2.6.0"; \
		exit 1;                                                   \
	fi

#
# standard footer
#
include $(COMPILER_ROOT)/make/Makefile.compiler.foot


# Use bison-chapel.h as prerequisite, to ensure bison only invoked once, even
# with parallel execution.
#
$(OBJ_SUBDIR)/bison-chapel.o: ../include/antlr.h ../include/bison-chapel.h bison-chapel.cpp $(OBJ_SUBDIR_MADE)
	$(CXX) -c $(COMP_CXXFLAGS) $(COMP_CXXFLAGS_NONCHPL) -o $@ bison-chapel.cpp

$(OBJ_SUBDIR)/flex-chapel.o: ../include/bison-chapel.h ../include/flex-chapel.h flex-chapel.cpp $(OBJ_SUBDIR_MADE)
	$(CXX) -c $(COMP_CXXFLAGS) $(COMP_CXXFLAGS_NONCHPL) -o $@ flex-chapel.cpp

$(OBJ_SUBDIR)/parser.o:  antlr_headers ../include/antlr.h ../include/bison-chapel.h ../include/flex-chapel.h parser.cpp $(OBJ_SUBDIR_MADE)
	$(CXX) -c $(COMP_CXXFLAGS) -o $@ parser.cpp

$(OBJ_SUBDIR)/antlr.o: antlr_headers antlr.cpp $(OBJ_SUBDIR_MADE)
	$(CXX) -c $(COMP_CXXFLAGS) -o $@ antlr.cpp

$(OBJ_SUBDIR)/ChapelBaseVisitor.o: antlr_headers ChapelBaseVisitor.cpp $(OBJ_SUBDIR_MADE)
	$(CXX) -c $(COMP_CXXFLAGS) -o $@ ChapelBaseVisitor.cpp

$(OBJ_SUBDIR)/ChapelVisitor.o: antlr_headers ChapelVisitor.cpp $(OBJ_SUBDIR_MADE)
	$(CXX) -c $(COMP_CXXFLAGS) -o $@ ChapelVisitor.cpp

$(OBJ_SUBDIR)/ChapelLexer.o: antlr_headers ChapelLexer.cpp $(OBJ_SUBDIR_MADE)
	$(CXX) -c $(COMP_CXXFLAGS) -o $@ ChapelLexer.cpp

$(OBJ_SUBDIR)/ChapelParser.o: antlr_headers ChapelParser.cpp $(OBJ_SUBDIR_MADE)
	$(CXX) -c $(COMP_CXXFLAGS) -o $@ ChapelParser.cpp


antlr_headers: ../include/ChapelBaseVisitor.h ../include/ChapelVisitor.h ../include/ChapelLexer.h ../include/ChapelParser.h

ChapelParser.cpp ChapelLexer.cpp ChapelVisitor.cpp ChapelBaseVisitor.cpp ../include/ChapelBaseVisitor.h ../include/ChapelVisitor.h ../include/ChapelLexer.h ../include/ChapelParser.h: Chapel.g4
	java -Xmx500M -cp "/usr/local/lib/antlr-4.7.1-complete.jar:$CLASSPATH" org.antlr.v4.Tool -visitor -no-listener -Dlanguage=Cpp Chapel.g4
	mv ChapelBaseVisitor.h ../include/ChapelBaseVisitor.h
	mv ChapelVisitor.h ../include/ChapelVisitor.h
	mv ChapelLexer.h ../include/ChapelLexer.h
	mv ChapelParser.h ../include/ChapelParser.h
	
