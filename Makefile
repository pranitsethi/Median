CXX = g++
CXXFLAGS = -std=c++0x -O0 -ggdb -I.
DEPS = Median_Engine.h
OBJ =  Median_Engine.o 

PATH += 

LIBS= 

%.o: %.cpp $(DEPS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

Median_Engine: $(OBJ)
	$(CXX) -o $@ $^ $(CXXFLAGS) $(LIBS)

clean:
	rm -f *.o Median_Engine core
