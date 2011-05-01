GYM_TEST_DEPENDS:=lj_gym.c lj_ym2612.c
GYM_TEST2_DEPENDS:=lj_gym.c fm2612.c
VGM_TEST_DEPENDS:=lj_vgm.c lj_ym2612.c
VGM_TEST2_DEPENDS:=lj_vgm.c fm2612.c

PROJECTS:=gym_test\
		  gym_test2\
		  vgm_test\
		  vgm_test2\

all: $(PROJECTS)

C_COMPILE:=gcc -c
C_COMPILE_FLAGS:=-g -Wall -Werror

LINK:=gcc
LINK_FLAGS:=-g -lm

define upperString
$(shell echo $1 | tr [a-z] [A-Z] )
endef

define PROJECT_template
$2_SRCFILES += $1.c
$2_SRCFILES += $($2_DEPENDS)
$2_DFILES:=$$($2_SRCFILES:.c=.d)

$2_OBJFILE:=$1.o
$2_OBJFILES:=$$($2_SRCFILES:.c=.o)

SRCFILES += $$($2_SRCFILES)
OBJFILES += $$($2_OBJFILES)
DFILES += $$($2_DFILES)

TARGETS += $1

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
	@echo DFILES=$(DFILES)
	@echo GYM_TEST_SRCFILES=$(GYM_TEST_SRCFILES)
	@echo GYM_TEST_OBJFILES=$(GYM_TEST_OBJFILES)
	@echo GYM_TEST_DEPENDS=$(GYM_TEST_DEPENDS)
	@echo GYM_TEST_DFILES=$(GYM_TEST_DFILES)
	@echo GYM_TEST_OBJFILE=$(GYM_TEST_OBJFILE)

%.o: %.c
	@echo Compiling $<
	@$(C_COMPILE) -MMD $(C_COMPILE_FLAGS) $<

%: %.o 
	@echo Linking $@
	@$(LINK) $(LINK_FLAGS) -o $@ $^

.PHONY: all clean nuke format
.SUFFIXES:            # Delete the default suffixes

FORCE:

clean: FORCE
	rm -f $(OBJFILES)
	rm -f $(DFILES)

nuke: clean
	rm -f $(TARGETS)


sinclude $(DFILES)

