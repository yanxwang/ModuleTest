CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -pthread

# Optional: override queue size at compile time for static version
# Example: make QUEUE_SIZE=2048
ifdef QUEUE_SIZE
CXXFLAGS += -DQUEUE_SIZE=$(QUEUE_SIZE)
endif

all: baseline uintrpoller


# Baseline test for comparing performance
baseline: baseline.cpp
	$(CXX) $(CXXFLAGS) -muintr -o baseline baseline.cpp

uintrpoller: uintrpoller.cpp
	$(CXX) $(CXXFLAGS) -muintr -o uintrpoller uintrpoller.cpp

clean:
	rm -f baseline uintrpoller uintr_debug.log uintr_verbose.log kernel_log*.txt

# Helper targets for common queue sizes (static version only)
q512: QUEUE_SIZE=512
q512: clean cxl_direct_test

q2048: QUEUE_SIZE=2048
q2048: clean cxl_direct_test

q4096: QUEUE_SIZE=4096
q4096: clean cxl_direct_test

.PHONY: all clean q512 q2048 q4096 baseline uintrpoller
