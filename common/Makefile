obj-y				+= memory.o
obj-y				+= memory_display.o
pbl-$(CONFIG_PBL_CONSOLE)	+= memory_display.o
obj-y				+= clock.o
obj-y				+= console_common.o
obj-y				+= startup.o
obj-y				+= misc.o
obj-pbl-y			+= memsize.o
obj-y				+= resource.o
obj-y				+= bootsource.o
obj-y				+= restart.o
obj-$(CONFIG_AUTO_COMPLETE)	+= complete.o
obj-$(CONFIG_BANNER)		+= version.o
obj-$(CONFIG_BAREBOX_UPDATE)	+= bbu.o
obj-$(CONFIG_BINFMT)		+= binfmt.o
obj-$(CONFIG_BLOCK)		+= block.o
obj-$(CONFIG_BLSPEC)		+= blspec.o
obj-$(CONFIG_BOOTM)		+= bootm.o
obj-$(CONFIG_CMD_LOADS)		+= s_record.o
obj-$(CONFIG_CMD_MEMTEST)	+= memtest.o
obj-$(CONFIG_COMMAND_SUPPORT)	+= command.o
obj-$(CONFIG_CONSOLE_FULL)	+= console.o
obj-$(CONFIG_CONSOLE_SIMPLE)	+= console_simple.o
obj-y				+= console_countdown.o
obj-$(CONFIG_DDR_SPD)		+= ddr_spd.o
obj-$(CONFIG_ENV_HANDLING)	+= environment.o
obj-$(CONFIG_ENVIRONMENT_VARIABLES) += env.o
obj-$(CONFIG_FILETYPE)		+= filetype.o
obj-$(CONFIG_FLEXIBLE_BOOTARGS)	+= bootargs.o
obj-$(CONFIG_GLOBALVAR)		+= globalvar.o
obj-$(CONFIG_GREGORIAN_CALENDER) += date.o
obj-$(CONFIG_KALLSYMS)		+= kallsyms.o
obj-$(CONFIG_MALLOC_DLMALLOC)	+= dlmalloc.o
obj-$(CONFIG_MALLOC_TLSF)	+= tlsf_malloc.o tlsf.o
obj-$(CONFIG_MALLOC_DUMMY)	+= dummy_malloc.o
obj-$(CONFIG_MEMINFO)		+= meminfo.o
obj-$(CONFIG_MENU)		+= menu.o
obj-$(CONFIG_MODULES)		+= module.o
extra-$(CONFIG_MODULES)		+= module.lds
obj-$(CONFIG_OFTREE)		+= oftree.o
obj-$(CONFIG_PARTITION_DISK)	+= partitions.o partitions/
obj-$(CONFIG_PASSWORD)		+= password.o
obj-$(CONFIG_POLLER)		+= poller.o
obj-$(CONFIG_RESET_SOURCE)	+= reset_source.o
obj-$(CONFIG_SHELL_HUSH)	+= hush.o
obj-$(CONFIG_SHELL_SIMPLE)	+= parser.o
obj-$(CONFIG_STATE)		+= state.o
obj-$(CONFIG_RATP)		+= ratp.o
obj-$(CONFIG_UIMAGE)		+= image.o uimage.o
obj-$(CONFIG_FITIMAGE)		+= image-fit.o
obj-$(CONFIG_MENUTREE)		+= menutree.o
obj-$(CONFIG_EFI_GUID)		+= efi-guid.o
obj-$(CONFIG_EFI_DEVICEPATH)	+= efi-devicepath.o
lwl-$(CONFIG_IMD)		+= imd-barebox.o
obj-$(CONFIG_IMD)		+= imd.o
obj-$(CONFIG_FILE_LIST)		+= file-list.o
obj-$(CONFIG_FIRMWARE)		+= firmware.o
obj-$(CONFIG_UBIFORMAT)		+= ubiformat.o
obj-$(CONFIG_BAREBOX_UPDATE_IMX_NAND_FCB) += imx-bbu-nand-fcb.o
obj-$(CONFIG_CONSOLE_RATP)	+= ratp.o

quiet_cmd_pwd_h = PWDH    $@
ifdef CONFIG_PASSWORD
ifneq ($(CONFIG_PASSWORD_DEFAULT),"")
PASSWD_FILE := $(shell cd $(srctree); find $(CONFIG_PASSWORD_DEFAULT) -type f)
cmd_pwd_h = echo -n "static const char default_passwd[] = \"" > $@; \
	cat $< | tr -d '\n' >> $@; \
	echo "\";" >> $@

include/generated/passwd.h: $(PASSWD_FILE)
	$(call if_changed,pwd_h)
else
cmd_pwd_h = echo "static const char default_passwd[] = \"\";" > $@

include/generated/passwd.h: FORCE
	$(call if_changed,pwd_h)
endif

targets += include/generated/passwd.h

$(obj)/password.o: include/generated/passwd.h
endif

# dependencies on generated files need to be listed explicitly
$(obj)/version.o: include/generated/compile.h
$(obj)/imd-barebox.o: include/generated/compile.h

# compile.h changes depending on hostname, generation number, etc,
# so we regenerate it always.
# mkcompile_h will make sure to only update the
# actual file if its content has changed.

       chk_compile.h = :
 quiet_chk_compile.h = echo '  CHK     $@'
silent_chk_compile.h = :
include/generated/compile.h: FORCE
	@$($(quiet)chk_compile.h)
	$(Q)$(CONFIG_SHELL) $(srctree)/scripts/mkcompile_h $@ \
	"$(UTS_MACHINE)" "$(CC) $(KBUILD_CFLAGS)"

