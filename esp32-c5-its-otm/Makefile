FILES = esp32-c5-its-otm.h esp32-c5-its-otm.cpp reader.cpp parser.cpp writer.cpp

esp32-c5-its-otm:	$(FILES)
	g++ -O3 $(FILES) -o esp32-c5-its-otm -pthread -lmosquitto -lmosquittopp