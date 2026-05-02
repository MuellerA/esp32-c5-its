# https://github.com/Technica-Engineering/vector_blf
VECTOR_BLF = ../../../../extern/Technica-Engineering_vector-blf/usr/local
# https://github.com/fpcap/fpcap
FPCAP = ../../../../extern/fpcap_fpcap

log-cvt:	log-cvt.cpp blf.cpp pcap.cpp log-cvt.h
	g++ -std=c++20 -o log-cvt log-cvt.cpp blf.cpp pcap.cpp -I $(VECTOR_BLF)/include -I $(FPCAP)/include -L $(VECTOR_BLF)/lib -L $(FPCAP)/build -L $(FPCAP)/build/_deps/zstd-build/lib -lVector_BLF -lfpcap -lzstd