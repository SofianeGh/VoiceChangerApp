CXX      = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -Isrc
LDFLAGS  = -lSDL2 -lSDL2_ttf   # ← ajout de -lSDL2_ttf

TARGET   = bin/game
SRC_DIR  = src
OBJ_DIR  = obj

SRCS     = $(wildcard $(SRC_DIR)/*.cpp)
OBJS     = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))

# ── Cible principale ──────────────────────────────────────────
all: $(TARGET)

$(TARGET): $(OBJS) | bin
	$(CXX) $^ -o $@ $(LDFLAGS)

# ── Compilation de chaque .cpp → .o ──────────────────────────
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ── Création des dossiers si absents ─────────────────────────
bin:
	mkdir -p bin

$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

# ── Nettoyage ─────────────────────────────────────────────────
clean:
	rm -f $(OBJ_DIR)/*.o $(TARGET)

fclean: clean
	rm -rf bin obj

re: fclean all

.PHONY: all clean fclean re