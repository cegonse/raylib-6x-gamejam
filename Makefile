all:
	@mkdir -p build
	@cd build && emcmake cmake .. && make && cd ..
	@cd build/raylib-game-template && npx http-server

clean:
	@rm -rf build

.PHONY: all clean