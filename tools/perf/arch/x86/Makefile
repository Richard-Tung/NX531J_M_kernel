ifndef NO_DWARF
PERF_HAVE_DWARF_REGS := 1
LIB_OBJS += $(OUTPUT)arch/$(ARCH)/util/dwarf-regs.o
endif
ifndef NO_LIBUNWIND
LIB_OBJS += $(OUTPUT)arch/$(ARCH)/util/unwind-libunwind.o
endif
ifndef NO_LIBDW_DWARF_UNWIND
LIB_OBJS += $(OUTPUT)arch/$(ARCH)/util/unwind-libdw.o
endif
ifndef NO_DWARF_UNWIND
LIB_OBJS += $(OUTPUT)arch/$(ARCH)/tests/regs_load.o
LIB_OBJS += $(OUTPUT)arch/$(ARCH)/tests/dwarf-unwind.o
endif
LIB_OBJS += $(OUTPUT)arch/$(ARCH)/util/header.o
LIB_OBJS += $(OUTPUT)arch/$(ARCH)/util/tsc.o
LIB_H += arch/$(ARCH)/util/tsc.h
HAVE_KVM_STAT_SUPPORT := 1
LIB_OBJS += $(OUTPUT)arch/$(ARCH)/util/kvm-stat.o
