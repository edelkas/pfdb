# Nota: Es preciso instalar GLFW (libglfw3 y libglfw3-dev) y GLEW.
# Con ello, viene GLUT (ligGL). Ambas hay que linkarlas.

CPPFLAGS = -Iinclude
LDFLAGS = -Llib -lcurl

build:
	rm -f pfdb;
	g++ src/*.cpp $(CPPFLAGS) $(LDFLAGS) -o pfdb
