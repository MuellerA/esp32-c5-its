INC = esp32-c5-its-otm.h ../esp32-c5-its-rx/include/log-data.h
SRC = esp32-c5-its-otm.cpp reader.cpp parser.cpp writer.cpp

esp32-c5-its-otm:	$(SRC) $(INC)
	g++ -O3 $(SRC) -o esp32-c5-its-otm -I ../esp32-c5-its-rx/include -pthread -lmosquitto -lmosquittopp