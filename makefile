NAME := mystuff

LIBRARIES := -lv8_{base,libbase,snapshot,libplatform}
INCLUDES := -I/usr/local

CXXFLAGS := --std=c++11

$(NAME): $(NAME).o
	g++ $(CXXFLAGS) $(LIBRARIES) $^ -o $(NAME)

%.o: %.cpp
	g++ $(CXXFLAGS) $(INCLUDES) $< -c

run:
	./$(NAME)

clean:
	rm -rf *.o $(NAME)
