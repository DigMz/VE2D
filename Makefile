BUILD_DIR := build
GENERATOR := Ninja

CMAKE_FLAGS := \
	-G $(GENERATOR) \
	-DENABLE_CPP20_MODULE=OFF \
	-DCMAKE_EXPORT_COMPILE_COMMANDS=ON

TARGET := $(BUILD_DIR)/VulkanTutorial/VulkanTutorial

.PHONY: test conf_debug conf_release configure compile run clean clean_clangd

test: compile run

conf_debug:
	$(MAKE) configure BUILD_TYPE=Debug

conf_release:
	$(MAKE) configure BUILD_TYPE=Release

configure:
	cmake -B $(BUILD_DIR) \
		$(CMAKE_FLAGS) \
		-DCMAKE_BUILD_TYPE=$(BUILD_TYPE)
	ln -sf $(BUILD_DIR)/compile_commands.json compile_commands.json

compile:
	cmake --build $(BUILD_DIR)

run:
	$(TARGET)

clean:
	rm -rf $(BUILD_DIR)

clean_clangd:
	rm -f compile_commands.json
