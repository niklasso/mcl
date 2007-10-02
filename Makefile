EXEC      = slask
DEPDIR    = mtl core simp

CFLAGS    = -Wall -ffloat-store
LFLAGS    = -lz

include $(MROOT)/mtl/template.mk
