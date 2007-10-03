EXEC      = slask
DEPDIR    = mtl core simp utils

CFLAGS    = -Wall -ffloat-store
LFLAGS    = -lz

include $(MROOT)/mtl/template.mk
