OUTFILE = beatbox
OUTDIR = $(HOME)/cmpt433/public/myApps

CROSS_COMPILE = arm-linux-gnueabihf-
CC_C = $(CROSS_COMPILE)gcc
CFLAGS = -Wall -g -std=c99 -D _POSIX_C_SOURCE=200809L -Werror -Wshadow -pthread
LFLAGS = -L$(HOME)/cmpt433/public/asound_lib_BBB

all: copy-files
	$(CC_C) $(CFLAGS) main.c functions.c audioMixer_template.c -o $(OUTDIR)/$(OUTFILE) $(LFLAGS) -lasound

app: copy-files
	$(CC_C) $(CFLAGS) main.c functions.c audioMixer_template.c $(OUTDIR)/$(OUTFILE)

clean:
	rm $(OUTDIR)/$(OUTFILE)

copy-files:
	cp -r beatbox-wave-files $(OUTDIR)/beatbox-wav-files/
	cp -r beatbox-server $(OUTDIR)/beatbox-server-copy/