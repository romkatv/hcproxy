.RECIPEPREFIX = >

appname := hcproxy

CXX := g++
CXXFLAGS := -std=c++17 -fno-exceptions -Wall -Werror -D_GNU_SOURCE -O2 -DNDEBUG
LDFLAGS := -static-libstdc++ -static-libgcc -pthread

SRCS := $(shell find src -name "*.cc")
OBJS := $(patsubst src/%.cc, obj/%.o, $(SRCS))

all: $(appname)

$(appname): $(OBJS)
> $(CXX) $(LDFLAGS) -o $(appname) $(OBJS)

obj:
> mkdir -p obj

obj/%.o: src/%.cc Makefile | obj
> $(CXX) $(CXXFLAGS) -MM -MT $@ src/$*.cc >obj/$*.dep
> $(CXX) $(CXXFLAGS) -c -o $@ src/$*.cc

clean:
> rm -rf obj

install: $(appname)
> cp -f $(appname) /usr/sbin/
> cp -f $(appname).service /lib/systemd/system/
> systemd-analyze verify /lib/systemd/system/$(appname).service
> systemctl stop $(appname) || true
> systemctl disable $(appname) || true
> systemctl enable --now $(appname)

-include $(OBJS:.o=.dep)
