appname := hcproxy

CXX := g++

CXXFLAGS :=         \
	-std=c++17        \
	-fno-exceptions   \
	-Wall             \
	-Werror           \
	-D_GNU_SOURCE     \
	-O2               \
	-static-libstdc++ \
	-static-libgcc    \
	-pthread

srcs := $(shell find . -name "*.cc")
objs  := $(patsubst %.cc, %.o, $(srcs))

all: $(appname)

$(appname): $(objs)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $(appname) $(objs) $(LDLIBS)

depend: depend.list

depend.list: $(srcs)
	rm -f depend.list
	$(CXX) $(CXXFLAGS) -MM $^>>depend.list

clean:
	rm -f depend.list $(objs)

install: $(appname)
	systemctl stop hcproxy || true
	systemctl disable hcproxy || true
	cp -f $(appname) /usr/sbin/
	cp -f $(appname).service /lib/systemd/system/
	systemd-analyze verify /lib/systemd/system/$(appname).service
	systemctl enable $(appname)
	systemctl start $(appname)

-include depend.list
