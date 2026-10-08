# ============================================================
#  Makefile du serveur domotique
#  make        : compile ce qui a changé
#  make clean  : supprime tout ce qui a été compilé
# ============================================================

CC      = gcc
CFLAGS  = -Wall -Wextra -g -std=c11 -D_POSIX_C_SOURCE=200809L -MMD -MP -I/usr/include/postgresql
LDLIBS  = -lcjson -lpq

EXEC    = build/serveur

# Tous les .c du dossier back et de ses sous-dossiers (sauf build)
SRC     = $(shell find . -name '*.c' -not -path './build/*')
# Pour chaque .c, un .o du même nom rangé dans build/
OBJ     = $(patsubst ./%.c,build/%.o,$(SRC))

# Règle par défaut : fabriquer l'exécutable
all: $(EXEC)

# Édition de liens : on assemble tous les .o en un exécutable
$(EXEC): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDLIBS)

# Compilation : un .c donne un .o (on crée le sous-dossier si besoin)
build/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf build/*

# Recompile un .c si un .h qu'il inclut a changé
-include $(OBJ:.o=.d)

.PHONY: all clean