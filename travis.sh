#!/bin/bash

if [[ "$TRAVIS_OS_NAME" == "linux" ]]; then
	if [[ "$CHECK" == "clang-format" ]]; then
		clang-format-7 --version
		echo Validating code formatting.

		clang-format-7 --style=file -i OrionUO/*.{h,cpp} OrionUO/*/*.{h,cpp}
		dirty=$(git ls-files --modified)
		if [[ $dirty ]]; then
			echo Failed. The following files are incorrectly formatted:
			echo $dirty
			echo "Diff:"
			git --no-pager diff --no-color
			exit 1
		else
			echo Code formatting validation passed.
		fi
	else
		echo Building Release
		mkdir release && cd release && cmake -G Ninja .. -DCMAKE_BUILD_TYPE=Release && time ninja OrionUO_unity -j8
		file ./OrionUO/unity/OrionUO
		##./OrionUO/unity/OrionUO --headless
		cd ..

		echo Building Debug
		mkdir debug && cd debug && cmake -G Ninja .. -DCMAKE_BUILD_TYPE=Debug && time ninja OrionUO_unity -j8
		file ./OrionUO/unity/OrionUO
		#./OrionUO/unity/OrionUO --headless
	fi
else
	brew update
	brew install sdl2 ninja glew
	brew outdated cmake || brew upgrade cmake

	echo Building Release
	mkdir release && cd release && cmake -G Ninja .. -DCMAKE_BUILD_TYPE=Release && ninja orion_unity -j8
	cd ..

	echo Building Debug
	mkdir debug && cd debug && cmake -G Ninja .. -DCMAKE_BUILD_TYPE=Debug && ninja orion_unity -j8
fi;
