GYM_LOADER_DEPENDS:=gym.c

PROJECTS:=gym_loader\

all: $(PROJECTS)

C_COMPILE:=gcc -c
C_COMPILE_FLAGS:=-g -Wall

LINK:=gcc
LINK_FLAGS:=-g

define upperString
$(shell echo $1 | tr [a-z] [A-Z] )
endef

define PROJECT_template
$2_SRCFILES += $1.c
$2_SRCFILES += $($2_DEPENDS)
$2_DEPEND_OBJS:=$($2_DEPENDS:.c=.o)

$2_OBJFILE:=$1.o
$2_OBJFILES:=$$($2_SRCFILES:.c=.o)

SRCFILES += $$($2_SRCFILES)
OBJFILES += $$($2_OBJFILES)

TARGETS += $1

$$($2_OBJFILE): $$($2_DEPEND_OBJS) $1.c
$1: $$($2_OBJFILES) 
endef
     
$(foreach project,$(PROJECTS),$(eval $(call PROJECT_template,$(project),$(call upperString,$(project)))))

test:
	@echo C_COMPILE=$(C_COMPILE)
	@echo C_COMPILE_FLAGS=$(C_COMPILE_FLAGS)
	@echo LINK=$(LINK)
	@echo LINK_FLAGS=$(LINK_FLAGS)
	@echo PROJECTS=$(PROJECTS)
	@echo TARGETS=$(TARGETS)
	@echo SRCFILES=$(SRCFILES)
	@echo OBJFILES=$(OBJFILES)
	@echo GYM_LOADER_SRCFILES=$(GYM_LOADER_SRCFILES)
	@echo GYM_LOADER_OBJFILES=$(GYM_LOADER_OBJFILES)
	@echo GYM_LOADER_DEPENDS=$(GYM_LOADER_DEPENDS)
	@echo GYM_LOADER_DEPEND_OBJS=$(GYM_LOADER_DEPEND_OBJS)
	@echo GYM_LOADER_OBJFILE=$(GYM_LOADER_OBJFILE)

%.o: %.c
	$(C_COMPILE) $(C_COMPILE_FLAGS) $<

%: %.o
	$(LINK) $(LINK_FLAGS) -o $@ $<

.PHONY: all clean nuke format
.SUFFIXES:            # Delete the default suffixes

FORCE:

clean: FORCE
	rm -f $(OBJFILES)

nuke: clean
	rm -f $(TARGETS)

