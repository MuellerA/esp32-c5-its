# https://github.com/Technica-Engineering/vector_blf
VECTOR_BLF = ../../../../extern/Technica-Engineering_vector-blf/usr/local
# https://github.com/fpcap/fpcap
FPCAP = ../../../../extern/fpcap_fpcap

SRC = log-cvt.cpp blf.cpp pcap.cpp ../../esp32-c5-its-rx/include/log-data.h
INC = log-cvt.h

log-cvt: $(SRC) $(INC)
	g++ -std=c++20 -o log-cvt $(SRC) -I ../../esp32-c5-its-rx/include -I $(VECTOR_BLF)/include -I $(FPCAP)/include -L $(VECTOR_BLF)/lib -L $(FPCAP)/build -L $(FPCAP)/build/_deps/zstd-build/lib -lVector_BLF -lfpcap -lzstd