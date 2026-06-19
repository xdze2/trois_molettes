LIB     = IRremoteESP8266/src
TEST    = IRremoteESP8266/test

CXX     = g++
CPPFLAGS = -DUNIT_TEST -D_IR_LOCALE_=en-AU
CXXFLAGS = -std=gnu++11 -Wall -Wextra -g -I$(LIB) -I$(TEST)

# All ir_*.cpp protocol files + common support files
PROTO_SRCS = $(wildcard $(LIB)/ir_*.cpp)
COMMON_SRCS = $(LIB)/IRsend.cpp $(LIB)/IRrecv.cpp $(LIB)/IRtimer.cpp \
              $(LIB)/IRutils.cpp $(LIB)/IRtext.cpp $(LIB)/IRac.cpp

SRCS = main.cpp $(COMMON_SRCS) $(PROTO_SRCS)

daikin_mock: $(SRCS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ -o $@

clean:
	rm -f daikin_mock

.PHONY: clean
