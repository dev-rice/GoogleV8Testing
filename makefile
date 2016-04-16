NAME := mystuff

LIBRARIES := -lv8_{base,libbase,snapshot,libplatform}
INCLUDES := -I/usr/local

CXXFLAGS := --std=c++11

testing_class: testing_class.o V8Thing.o
	g++ $(CXXFLAGS) $(LIBRARIES) $^ -o testing_class

$(NAME): $(NAME).o
	g++ $(CXXFLAGS) $(LIBRARIES) $^ -o $(NAME)


%.o: %.cpp
	g++ $(CXXFLAGS) $(INCLUDES) $< -c

run:
	./$(NAME)

clean:
	rm -rf *.o $(NAME)
