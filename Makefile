
CXX = g++
CXXFLAGS = -fPIC -g -O0 -Wall -Wshadow -Wextra -std=c++11 -pthread -DHAVE_CONFIG_H -DMBEDTLS_CONFIG_FILE='<config-thread.h>' -Wl,-rpath,/usr/local/lib
CFLAGS = $(CXXFLAGS)
LinkFlags = -fPIC -g -O0 -Wall -Wshadow -Wextra -std=c++11 -pthread -Wl,-rpath,/usr/local/lib
IncludePaths = -I. -I./include -Ilib/wakaama/http-framework/include/ -Ilib/wakaama/plugin-manager/include/ -Ilib/borderrouter/src/commissioner/ -Ilib/borderrouter/src/ -Ilib/borderrouter/third_party/mbedtls/repo/configs/ -Ilib/borderrouter/third_party/mbedtls/repo/include/ -Ilib/borderrouter/src/common/ -Ilib/wakaama/lwm2m-framework/include/
Libs = -ldl -lpthread -lulfius
LibPath = -L./Debug
SrcDir = src
MainSrcDir = lib/wakaama/main
LibDir = lib
BuildDir = Debug
PluginDir = $(LibDir)/plugins
ConstLibDir = $(LibDir)/const_libs
CommSrcDir = $(LibDir)/borderrouter/src/commissioner

MainSources = $(MainSrcDir)/main.cpp ./lib/wakaama/plugin-manager/src/basic_plugin_manager.cpp ./lib/wakaama/plugin-manager/src/basic_plugin_manager_core.cpp ./lib/wakaama/lwm2m-framework/src/basic_lwm2m_framework.cpp
MainObjects = $(patsubst %.cpp,%.o,$(MainSources))

UlfiusSources = ./lib/wakaama/http-framework/src/ulfius_http_framework.cpp ./lib/wakaama/http-framework/src/incoming_ulfius_request.cpp ./lib/wakaama/http-framework/src/outgoing_ulfius_response.cpp
UlfiusObjects = $(patsubst %.cpp,%.o,$(UlfiusSources))

PluginSources = $(SrcDir)/comm.cpp $(CommSrcDir)/addr_utils.cpp $(CommSrcDir)/commissioner.cpp $(CommSrcDir)/joiner_session.cpp $(CommSrcDir)/commissioner_argcargv.cpp $(CommSrcDir)/device_hash.cpp
PluginObjects = $(patsubst %.cpp,%.o,$(PluginSources))

ConstLibs = $(ConstLibDir)/libmbedtls.a $(ConstLibDir)/libotbr-agent.a $(ConstLibDir)/libutils.a $(ConstLibDir)/libcoap-1.a $(ConstLibDir)/libotbr-web.a

all: plugins $(BuildDir)/comm-client

$(BuildDir)/comm-client: $(MainObjects) $(UlfiusObjects)
	$(CXX) -o $(BuildDir)/comm-client $(LinkFlags) $(MainObjects) $(UlfiusObjects) $(LibPath) $(Libs)

plugins: $(BuildDir)/libcomm.so

$(BuildDir)/libcomm.so: $(PluginObjects)
	$(CXX) -shared -fPIC -o $(BuildDir)/libcomm.so -Wl,--start-group $(PluginObjects) /usr/local/lib/libjansson.a $(ConstLibs) -lpthread -Wl,--end-group -Wl,--no-undefined 
#	$(CXX) -shared $(LinkFlags) -o $(BuildDir)/libcomm.so $(PluginObjects) -Wl,--start-group $(ConstLibs) -lpthread -Wl,--end-group -Llib/borderrouter/src/utils/.libs

#$(PluginObjects): %.o : %.cpp
%.o : %.cpp
	$(CXX) $(CXXFLAGS) $(IncludePaths) -c $< -o $@
	@cp $@ $(BuildDir)/$(notdir $(MainObjectsP))

clean:
	rm $(BuildDir)/* $(PluginObjects) $(MainObjects) $(UlfiusObjects)
	
.PHONY : all plugins clean
	
	

