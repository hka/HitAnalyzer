gcc -o HitAnalyzer HitAnalyzer.c parg.c $(pkg-config --libs --cflags sdl2) -lm
