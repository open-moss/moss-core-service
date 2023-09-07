.PHONY: speaker clean

speaker:
	cmake-js configure -- -DUSE_ALSA=ON
	cmake-js compile

clean:
	rm -rf build/
