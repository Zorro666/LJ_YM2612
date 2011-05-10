GYM_TEST_DEPENDS:=lj_gym.c lj_ym2612.c
GYM_TEST2_DEPENDS:=lj_gym.c fm2612.c
VGM_TEST_DEPENDS:=lj_vgm.c lj_ym2612.c lj_wav_file.c vgm_test_main.c
VGM_TEST2_DEPENDS:=lj_vgm.c fm2612.c lj_wav_file.c vgm_test_main.c

PROJECTS:=\
		  gym_test\
		  gym_test2\
		  vgm_test\
		  vgm_test2\

all: $(PROJECTS)

C_COMPILE:=gcc -c
C_COMPILE_FLAGS:=-g -Wall -Werror

LINK:=gcc
LINK_FLAGS:=-g -lm

ifdef WINDIR
TARGET_EXTENSION := .exe
else
TARGET_EXTENSION := 
endif	# ifdef WINDIR

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

TARGET_EXES := $(foreach target,$(TARGETS),$(target)$(TARGET_EXTENSION))

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
	@echo VGM_TEST_SRCFILES=$(VGM_TEST_SRCFILES)
	@echo VGM_TEST_OBJFILES=$(VGM_TEST_OBJFILES)
	@echo VGM_TEST_DEPENDS=$(VGM_TEST_DEPENDS)
	@echo VGM_TEST_DFILES=$(VGM_TEST_DFILES)
	@echo VGM_TEST_OBJFILE=$(VGM_TEST_OBJFILE)
	@echo TARGET_EXTENSION=$(TARGET_EXTENSION)
	@echo TARGET_EXES=$(TARGET_EXES)

%.o: %.c
	@echo Compiling $<
	@$(C_COMPILE) -MMD $(C_COMPILE_FLAGS) $<

%: %.o 
	@echo Linking $@
	@$(LINK) $(LINK_FLAGS) -o $@ $^

.PHONY: all clean nuke format tags
.SUFFIXES:            # Delete the default suffixes

FORCE:

tags:
	@ctags -R --exclude=CMakeFiles --c++-kinds=+p --fields=+iaS --extra=+q .

clean: FORCE
	@$(RM) -vf $(OBJFILES)
	@$(RM) -vf $(DFILES)
	@$(RM) -vf tags
	@$(RM) -vf cscope.out
	@$(RM) -vf cscope.in.out
	@$(RM) -vf cscope.po.out


nuke: clean
	@$(RM) -vf $(TARGET_EXES)


sinclude $(DFILES)

