
CXX = g++
CXXFLAGS = -fPIC -g -O0 -Wall -Wshadow -Wextra -std=c++11 -pthread -DHAVE_CONFIG_H -DMBEDTLS_CONFIG_FILE='<config-thread.h>' -Wl,-rpath,/usr/local/lib
CFLAGS = $(CXXFLAGS)
#LinkFlags = -g -Og -Wall -Wextra -Wshadow -Werror -std=gnu++98 -Wno-c++14-compat -Wl,-rpath,/home/tautvydas/Documents/codelite_workspace/commissioner-lib/Debug
LinkFlags = $(CXXFLAGS)
IncludePaths = -I. -I./include -I./lib/borderrouter/third_party/mbedtls/repo/include/ -I./lib/borderrouter/src/ -I./lib/borderrouter/src/commissioner -I./lib/borderrouter/src/common -I./lib/borderrouter/include -I./lib/borderrouter/third_party/mbedtls/repo/configs -I./lib/borderrouter/src/agent -I./lib/borderrouter/src/web -I./lib/wakaama/plugin-manager
Libs = -ldl -ljansson -lpthread -lulfius
#LibPath = -L/home/tautvydas/Documents/codelite_workspace/commissioner-lib/Debug
LibPath = -L./Debug
SrcDir = src
LibDir = lib
BuildDir = Debug
PluginDir = $(LibDir)/plugins
ConstLibDir = $(LibDir)/const_libs
CommSrcDir = $(LibDir)/borderrouter/src/commissioner

MainSources = $(SrcDir)/main.cpp
MainObjects = $(patsubst %.cpp,%.o,$(MainSources))

UlfiusSources = ./lib/wakaama/plugin-manager/ulfius_http_framework.cpp
UlfiusObjects = $(patsubst %.cpp,%.o,$(UlfiusSources))

PluginSources = $(SrcDir)/comm.cpp $(CommSrcDir)/addr_utils.cpp $(CommSrcDir)/commissioner.cpp $(CommSrcDir)/joiner_session.cpp $(CommSrcDir)/commissioner_argcargv.cpp $(CommSrcDir)/device_hash.cpp
PluginObjects = $(patsubst %.cpp,%.o,$(PluginSources))

ConstLibs = $(ConstLibDir)/libmbedtls.a $(ConstLibDir)/libotbr-agent.a $(ConstLibDir)/libutils.a $(ConstLibDir)/libcoap-1.a $(ConstLibDir)/libotbr-web.a

all: plugins $(BuildDir)/comm-client

$(BuildDir)/comm-client: $(MainObjects) $(UlfiusObjects)
	$(CXX) -o $(BuildDir)/comm-client $(LinkFlags) $(MainObjects) $(UlfiusObjects) $(LibPath) $(Libs)

plugins: $(BuildDir)/libfoo.so $(BuildDir)/libbar.so $(BuildDir)/libcomm.so

$(BuildDir)/libcomm.so: $(PluginObjects) $(UlfiusObjects)
	$(CXX) -shared $(CXXFLAGS) -o $(BuildDir)/libcomm.so $(PluginObjects) $(UlfiusObjects) -Wl,--start-group $(ConstLibs) -ljansson -lpthread -lulfius -lutils -Wl,--end-group $(LinkFlags) -Wl,--no-undefined -Llib/borderrouter/src/utils/.libs

#$(PluginObjects): %.o : %.cpp
%.o : %.cpp
	$(CXX) $(CXXFLAGS) $(IncludePaths) -c $< -o $@
	@cp $@ $(BuildDir)/$(notdir $(MainObjectsP))

clean:
	rm $(BuildDir)/*.o $(PluginObjects) $(MainObjects) $(BuildDir)/comm-client
	
.PHONY : all plugins clean
	
	

