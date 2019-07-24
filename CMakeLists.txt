cmake_minimum_required(VERSION 2.6)
project(Engine)



##############################################################


# オプションライブラリを選択
option(MO_BOX2D  "Box2D / 2D Physics Engine"  OFF)
option(MO_BULLET "Bullet / 3D Physics Engine" OFF)

# DirectX SDK version
if (MSVC)
	# 新旧どちらの SDK を使うか選択する。
	# デフォルトは旧 SDK (DirectX SDK 2004-Oct)
	# ちなみにこれを設定しても SDK のパスがセットされるわけではなく、
	# 互換性に関する一部の設定が切り替わるだけ。
	option(D3D_LEGACY "Use Legacy DirectX SDK (2004 Oct)" ON)
endif()



# ソースファイルのリストを作成する

# Engine
if (1)
	file(GLOB coreFiles
		${CMAKE_CURRENT_LIST_DIR}/core/*.c
		${CMAKE_CURRENT_LIST_DIR}/core/*.cpp
		${CMAKE_CURRENT_LIST_DIR}/core/*.h
	)
	include_directories(${CMAKE_CURRENT_LIST_DIR}/core)
endif()


# ImGui
if (1)
	file(GLOB imguiFiles 
		${CMAKE_CURRENT_LIST_DIR}/core/imgui/*.h
		${CMAKE_CURRENT_LIST_DIR}/core/imgui/*.cpp
	)
endif()


# Lua
if (1)
	file(GLOB luaFiles
		${CMAKE_CURRENT_LIST_DIR}/core/lua/*.c
		${CMAKE_CURRENT_LIST_DIR}/core/lua/*.h
	)
endif()


# miniz
if (1)
	file(GLOB minizFiles
		${CMAKE_CURRENT_LIST_DIR}/core/miniz/*.c
		${CMAKE_CURRENT_LIST_DIR}/core/miniz/*.h
	)
endif()


# TinyXML
if (1) 
	file(GLOB tinyxmlFiles
		${CMAKE_CURRENT_LIST_DIR}/core/tinyxml/*.cpp
		${CMAKE_CURRENT_LIST_DIR}/core/tinyxml/*.h
	)
endif()


# Box2D (optional)
if (MO_BOX2D)
	file(GLOB_RECURSE box2dFiles
		${CMAKE_CURRENT_LIST_DIR}/libs/Box2D/*.h
		${CMAKE_CURRENT_LIST_DIR}/libs/Box2D/*.cpp
	)
	# Box2D は自身をインクルードするために "Box2D/" で始まるパスを使っているので
	# Box2D の一つ上のフォルダを指定する
	include_directories("${CMAKE_CURRENT_LIST_DIR}/libs") 

endif()


# Bullet physics (optional)
if (MO_BULLET)
	file(GLOB_RECURSE bulletFiles
		${CMAKE_CURRENT_LIST_DIR}/libs/bullet/*.cpp
		${CMAKE_CURRENT_LIST_DIR}/libs/bullet/*.h
	)
	include_directories(${CMAKE_CURRENT_LIST_DIR}/libs/bullet)
endif()


##############################################################

# ソースグループを構築する。
# 存在しないファイルリストはそのまま無視されるだけなので、
# オプションによって ON/OFF しているライブラリについて場合分けしなくてもよい
source_group("Engine"          FILES ${coreFiles})
source_group("Engine\\ImGUI"   FILES ${imguiFiles})
source_group("Engine\\Lua"     FILES ${luaFiles})
source_group("Engine\\MiniZ"   FILES ${minizFiles})
source_group("Engine\\TinyXml" FILES ${tinyxmlFiles})
source_group("Engine\\Box2D"   FILES ${box2dFiles})
source_group("Engine\\Bullet"  FILES ${bulletFiles})

# ソースファイルを追加する
add_library(Engine STATIC
	${coreFiles}
	${imguiFiles}
	${minizFiles}
	${luaFiles}
	${tinyxmlFiles}
	${bulletFiles}
	${box2dFiles}
)

##############################################################

if (MSVC)
	# セキュリティ警告を無効にする
	add_definitions(-D_CRT_SECURE_NO_WARNINGS)

	# VCライブラリの min, max マクロを無効にするために NOMINMAX を定義する
	add_definitions(-DNOMINMAX)

	# Unicode の利用
	add_definitions(-DUNICODE -D_UNICODE)

	# ランタイムDLLを使わないようにする
	set(CMAKE_CXX_FLAGS_DEBUG    "${CMAKE_CXX_FLAGS_DEBUG}   /MT  /FAcs")
	set(CMAKE_CXX_FLAGS_RELEASE  "${CMAKE_CXX_FLAGS_RELEASE} /MTd /FAcs")
endif()
