debug:
	bash compileShaders.sh
	cmake -B build -DCMAKE_BUILD_TYPE=Debug
	cmake --build build

debug-stl:
	bash compileShaders.sh
	cmake -B build -DCMAKE_BUILD_TYPE=Debug -DENABLE_STL_DEBUG=ON
	cmake --build build

release:
	bash compileShaders.sh
	cmake -B build -DCMAKE_BUILD_TYPE=Release
	cmake --build build

release-debug:
	bash compileShaders.sh
	cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
	cmake --build build

release-debug-unoptimized:
	bash compileShaders.sh
	cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DRELWITHDEBINFO_OPT=-O0
	cmake --build build

run:
	./build/vxen

run-exp:
	./build/experiment

valgrind:
	valgrind --leak-check=full --track-origins=yes ./build/vxen

renderdoc:
	renderdoccmd capture --working-dir . ./build/vxen

clearlogs:
	rm -rf logs/*
