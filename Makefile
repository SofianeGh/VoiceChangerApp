# Compilateur et options
# MINGW_PREFIX est défini par le shell MSYS2 (ex: /ucrt64)
MINGW_PREFIX ?= /ucrt64
CXX      = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -O3 -fPIC -Isrc -MMD -MP \
           -isystem $(MINGW_PREFIX)/include/miniaudio \
           -isystem $(MINGW_PREFIX)/include/soundtouch

# Édition de liens (Création d'une DLL pour l'interopérabilité C#)
# -static : la DLL n'a besoin d'aucune DLL MinGW à l'exécution
LDFLAGS  = -shared -static -lSoundTouch -lavrt

# Cibles principales
TARGET   = bin/CoreAudio.dll
PLUGIN_DIR = bin/plugins

# Dossiers
SRC_DIR  = src
OBJ_DIR  = obj

# Fichiers (chaque plugin est compilé à part, dans sa propre DLL)
ECHO_SRC    = $(SRC_DIR)/MyEchoPlugin.cpp
PLUGIN_SRCS = $(wildcard $(SRC_DIR)/plugins/*.cpp)
PLUGINS     = $(PLUGIN_DIR)/MyEchoPlugin.dll \
              $(patsubst $(SRC_DIR)/plugins/%.cpp, $(PLUGIN_DIR)/%.dll, $(PLUGIN_SRCS))
PLUGIN_FLAGS = -std=c++20 -Wall -Wextra -O3 -Isrc
SRCS     = $(filter-out $(ECHO_SRC), $(wildcard $(SRC_DIR)/*.cpp))
OBJS     = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRCS))
DEPS     = $(OBJS:.o=.d)

# Cible par défaut
all: $(TARGET) $(PLUGINS)

# Création de la DLL (Édition de liens)
$(TARGET): $(OBJS) | bin
	$(CXX) $^ -o $@ $(LDFLAGS)

# Plugin d'exemple
$(PLUGIN_DIR)/MyEchoPlugin.dll: $(ECHO_SRC) $(SRC_DIR)/PluginAPI.h | $(PLUGIN_DIR)
	$(CXX) $(PLUGIN_FLAGS) $< -o $@ -shared -static

# Plugins du dossier src/plugins
$(PLUGIN_DIR)/%.dll: $(SRC_DIR)/plugins/%.cpp $(SRC_DIR)/plugins/PluginHelper.h $(SRC_DIR)/PluginAPI.h | $(PLUGIN_DIR)
	$(CXX) $(PLUGIN_FLAGS) $< -o $@ -shared -static

# Compilation de chaque .cpp en .o
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Création des dossiers si absents
bin $(PLUGIN_DIR) $(OBJ_DIR):
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
