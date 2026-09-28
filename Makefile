# Compilateur et options
CXX      = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -O3 -fPIC -Isrc -MMD -MP

# Édition de liens (Création d'une DLL pour l'interopérabilité C#)
LDFLAGS  = -shared -lavrt -lsoundtouch

# Cible principale
TARGET   = bin/CoreAudio.dll

# Dossiers
SRC_DIR  = src
OBJ_DIR  = obj

# Fichiers
SRCS     = $(wildcard $(SRC_DIR)/*.cpp)
OBJS     = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))
DEPS     = $(OBJS:.o=.d)

# Cible par défaut
all: $(TARGET)

# Création de la DLL (Édition de liens)
$(TARGET): $(OBJS) | bin
	$(CXX) $^ -o $@ $(LDFLAGS)

# Compilation de chaque .cpp en .o
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Création des dossiers si absents
bin $(OBJ_DIR):
	mkdir -p $@

# Inclusion des fichiers de dépendances (.d)
-include $(DEPS)

# Nettoyage
clean:
	rm -f $(OBJ_DIR)/*.o $(OBJ_DIR)/*.d

fclean: clean
	rm -rf bin obj

re: fclean all

.PHONY: all clean fclean re