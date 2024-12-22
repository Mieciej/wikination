CXX = g++
CXXFLAGS = -Wall -I./imgui -I./imgui/backends -I/usr/include/SDL2 
LIBS = -lsqlite3 -lSDL2

IMGUI_DIR = imgui
BUILD_DIR = build


IMGUI_SRC = $(IMGUI_DIR)/imgui.cpp $(IMGUI_DIR)/imgui_draw.cpp $(IMGUI_DIR)/imgui_tables.cpp $(IMGUI_DIR)/imgui_widgets.cpp $(IMGUI_DIR)/backends/imgui_impl_sdl2.cpp $(IMGUI_DIR)/backends/imgui_impl_sdlrenderer2.cpp 
SRC = $(IMGUI_SRC)
# Object files
OBJ = $(SRC:.cpp=.o)

# Target executable
TARGET = wikination

# Default target
all: $(TARGET)

$(TARGET): $(OBJ) wikination.cpp
	$(CXX) wikination.cpp $(OBJ)  -o $(TARGET) $(LIBS) $(CXXFLAGS)

$(BUILD_DIR)/%.o: $(SRC)/%.cpp
	mkdir -p $(BUILD_DIR)
	$(CXX) $< -c $(CXXFLAGS) -o $@

clean:
	rm -rf $(BUILD_DIR) $(TARGET)

.PHONY: all clean

