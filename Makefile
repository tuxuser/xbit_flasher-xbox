OBJECTS = main.o
LIBS = -lhidapi
CFLAGS = -g -Wall -I/usr/local/Cellar/hidapi/0.8.0-rc1/include/
LDFLAGS = -L/usr/local/Cellar/hidapi/0.8.0-rc1/lib

NAME = xbit_flasher

xbit_flasher: $(OBJECTS)
	$(CXX) -o $(NAME) $(OBJECTS) $(LIBS) $(LDFLAGS)

%.o: %.cpp
	$(CXX) -c $(CFLAGS) $<

clean:
	rm *.o $(NAME)
