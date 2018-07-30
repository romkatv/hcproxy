appname := hcproxy

CXX := g++
CXXFLAGS := -std=c++17 -fno-exceptions -Wall -Werror -D_GNU_SOURCE -O2
LDFLAGS := -static-libstdc++ -static-libgcc -pthread

srcs := $(shell find src -name "*.cc")
objs  := $(patsubst src/%.cc, obj/%.o, $(srcs))

all: $(appname)

$(appname): $(objs)
	$(CXX) $(LDFLAGS) -o $(appname) $(objs)

$(objs): | obj

obj:
	mkdir -p obj

obj/%.o: src/%.cc
	$(CXX) $(CXXFLAGS) -c -o $@ $<

depend: obj/depend.list

obj/depend.list: $(srcs) | obj
	rm -f obj/depend.list
	$(CXX) $(CXXFLAGS) -MM $^>>obj/depend.list

clean:
	rm -f obj/depend.list obj/*.o
	test ! -d obj || rmdir --ignore-fail-on-non-empty obj

install: $(appname)
	systemctl stop hcproxy || true
	systemctl disable hcproxy || true
	cp -f $(appname) /usr/sbin/
	cp -f $(appname).service /lib/systemd/system/
	systemd-analyze verify /lib/systemd/system/$(appname).service
	systemctl enable $(appname)
	systemctl start $(appname)

-include obj/depend.list
