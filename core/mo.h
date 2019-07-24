/*! @mainpage KEngine 導入方法

@section 空のプロジェクトを新規作成する

ここではとりあえず、KEngine のソースコードを取り込んでビルドする方法を解説します。<br>
※<b>Visual Studio 2017</b>を想定した手順ですので、他のバージョンの場合は適当に読み変えてください

Windowdsデスクトップアプリの作成手順についての公式の説明は、<a href="https://docs.microsoft.com/ja-jp/cpp/windows/walkthrough-creating-windows-desktop-applications-cpp?view=vs-2019">こちら</a>を参照してください<br>


<ol>
	<li> Visual Studio 2017 を起動し、[ファイル|新規作成|プロジェクト]をクリック
	<li> 「新しいプロジェクト」ダイアログが出るので、左のツリーから[インストール済み/Visual C++/Windowsデスクトップ]を選ぶ
	<li> 出てきた一覧から「Windowsデスクトップ ウィザード」を選択し、保存先を決めて[OK]を押す（ここでは仮に MyProject を C:\MyProject に作ったものとする）
	<li> 「Windowsデスクトッププロジェクト」ダイアログが出るので、「アプリケーションの種類」を 「Windowsアプリケーション(.exe)」にして、「空のプロジェクト」だけにチェックがついている状態にする。他のチェックはすべて外す。次のような状態になっていることを確認する：
	<ul>
		<li> アプリケーションの種類 → 「<b>Windowsアプリケーション(.exe)</b>」
		<li> 空のプロジェクト → <b>チェックあり</b><small>（チェックを外すと、余計な .cpp ファイルがついてくる）</small>
		<li> セキュリティ開発ライフサイクル → <b>チェック無し</b><small>(チェックを入れると、非推奨な古い文字列関数 strcpy に対する警告がエラーに格上げされ、コンパイルできなくなる）</small>
		<li> ATL → <b>チェック無し</b>
	</ul>
	<li> OKボタンを押す
</ol>


@section エンジンのファイルをプロジェクトに追加する

<ol>
	<li> エクスプローラーを開き、上で作成したプロジェクトのフォルダ(C:\MyProject) を開く。中には MyProject.sln ファイルなどが入っているはず。
	<li> そこに Engine フォルダを丸ごとコピーして入れておく
	<li> Visual Studio に戻り、ソリューションエクスプローラーを表示させる
	<li> ソリューションエクスプローラ内の "MyProject" 項目を右クリックし、「追加|新しいフィルター」を選び、適当な名前（ここでは仮に "Engine" としておく）で決定する。
	<li> 上で追加したフィルター "Engine" に、さきほど C:\MyProejct にコピーした Engine の中にある core フォルダ (C:\MyProject\Engine\core) を丸ごとドラッグドロップしてプロジェクトにソースを追加する
	<li> 必要ならば、 C:\MyProject\Engine\libs フォルダも同様に追加しておく
</ol>


@section ソースファイルを追加する

<ol>
	<li> ソリューションエクスプローラ内の "MyProject" 項目を右クリックし、「追加|新しい項目」を選び、「C++ファイル(.cpp)」を選んで「追加」をクリックする。<br>
		<small>ここでは仮に "main.cpp" という名前で保存したものとする</small>
	<li> プソリューションエクスプローラ内の "MyProject" 項目を右クリックし、「プロパティ」を選び、プロパティウィンドウを開く。
	<li> 「構成プロパティ/リンカー/システム」を選び、サブシステムを「<b>Windows (/SUBSYSTEM:WINDOWS)</b>」に設定する。<br>
	  <small>※この作業は省略可能です。省略した場合は「Console (/SUBSYSTEM:CONSOLE)」になっているので、以下のサンプルで WinMain(...) ではなく int main() と書いてください</small>

	<li> 「構成プロパティ/C/C++/コード生成」を選び、ランタイムライブラリを「<b>マルチスレッド デバッグ(/MTd)</b>」を選ぶ。<br>
	  <small>※この作業は省略可能です。省略した場合はランタイムライブラリが「マルチスレッド デバッグ DLL (/MDd)」になっているので、作成した exe を他の PC で実行するときに追加の DLL を要求される可能性があります</small>

	<li> 「構成プロパティ/C/C++/プリプロセッサ」を選び、プロセッサの定義に <b>_CRT_SECURE_NO_WARNINGS</b> を追加する。<br>
	  <small>※この作業は省略可能です。省略した場合はコンパイル時に 「warning C4996 ... 」という警告が大量に出ますが、コンパイル自体は成功します</small>
</ol>


@section コンパイルできることを確認する
<ol>
	<li> main.cpp に以下の内容をそのままコピペする
	@code
		#include <Windows.h>
		#include "engine/core/mo.h"
		int WINAPI WinMain(HINSTANCE hCurrInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
			return 0;
		}
	@endcode

	<li> 「デバッグ|デバッグの開始」を選ぶか、F5を押してコンパイル＆実行し、エラーが起きないことを確認する。
	エラーが起きずに実行できたらOK （何もしないプログラムなので実行しても一瞬で終わる）
	※ここで、「未解決のシンボル _main が関数 ....」 というエラーが出る場合、サブシステムが「Windows」になっていることを確認する（手順「ソースファイルを追加する」でのサブシステム追加を参照）
	※ここで、「warning C4996: 'strcpy': This function なんたらかんたら ...」が大量に出るのが気になる場合は、プリプロセッサで _CRT_SECURE_NO_WARNINGS を定義しておく （手順 C4 を参照）
</ol>

@section 三角形を表示してみる

@code
	#include <Windows.h>
	#include <mo.h>
	using namespace mo;

	// 青い背景にRGBグラデーションの三角形が表示されればOK
	Engine g_Engine;

	class CTest: public System, public IScreenCallback {
	public:
		virtual void onRawRender() override {
			Video::setViewport(0, 0, 640, 480);
			Video::clearColor(Color(0, 0, 1, 1));
			Vertex v[] = {
				{Vec3(0,     0, 0), Color32(255, 0, 0, 255), Vec2()}, // Color32 の引数順序に注意
				{Vec3(200, 200, 0), Color32(0, 255, 0, 255), Vec2()},
				{Vec3(200,   0, 0), Color32(0, 0, 255, 255), Vec2()},
			};
			Video::setProjection(Matrix4::fromOrtho(640, 480, -1000, 1000));
			Video::setTransform(Matrix4());
			Video::beginMaterial(NULL);
			Video::draw(v, 3, Primitive_TriangleList);
			Video::endMaterial(NULL);
		}
	};
	
	int WINAPI WinMain(HINSTANCE hCurrInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
		CTest test;
		EngineDef def;
		def.resolution_x = 640;
		def.resolution_y = 480;
		if (g_Engine.init(def)) {
			g_Engine.addSystem(&test);
			g_Engine.addScreenCallback(&test);
			g_Engine.setWindowTitle("Hello World");
			g_Engine.run();
			g_Engine.removeScreenCallback(&test);
			g_Engine.callEnd();
			g_Engine.shutdown();
		}
		return 0;
	}
@endcode

@section GUIを使う
@code
	#include <Windows.h>
	#include <mo.h>
	using namespace mo;
	
	// GUIが表示されればOK
	Engine g_Engine;

	class CTest: public System, public IScreenCallback {
		bool toggle_;
	public:
		CTest() {
			toggle_ = false;
		}
		virtual void onUpdateGui() override {
			if (toggle_) {
				ImGui::Text("HELLO WORLD!");
			} else {
				ImGui::Text(u8"こんにちは");
			}
			if (ImGui::Button("Click me!")) {
				toggle_ = !toggle_;
			}
		}
	};
	
	int WINAPI WinMain(HINSTANCE hCurrInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
		CTest test;
		EngineDef def;
		def.resolution_x = 640;
		def.resolution_y = 480;
		if (g_Engine.init(def)) {
			ImGuiVideo_Init(g_Engine.getWindow());
			ImGuiVideo_SetDefaultFont(ImGuiVideo_AddFont(FF_MEIRYO_UI, 14));
			g_Engine.addSystem(&test);
			g_Engine.addScreenCallback(&test);
			g_Engine.setWindowTitle("GUI Test");
			g_Engine.run();
			g_Engine.removeScreenCallback(&test);
			g_Engine.callEnd();
			g_Engine.shutdown();
		}
		return 0;
	}
@endcode

@section 三角形とGUIを同時に使う
@code
	#include <Windows.h>
	#include <mo.h>
	using namespace mo;
	
	// 三角形とGUIが同時に表示されればOK
	Engine g_Engine;

	class CTest : public System, public IScreenCallback {
		bool toggle_;
	public:
		CTest() {
			toggle_ = false;
		}
		virtual void onRawRender() override {
			Video::setViewport(0, 0, 640, 480);
			Video::clearColor(Color(0, 0, 1, 1));
			Vertex v[] = {
				{Vec3(0,     0, 0), Color32(255, 0, 0, 255), Vec2()}, // Color32 の引数順序に注意
				{Vec3(200, 200, 0), Color32(0, 255, 0, 255), Vec2()},
				{Vec3(200,   0, 0), Color32(0, 0, 255, 255), Vec2()},
			};
			Video::setProjection(Matrix4::fromOrtho(640, 480, -1000, 1000));
			Video::setTransform(Matrix4());
			Video::beginMaterial(NULL);
			Video::draw(v, 3, Primitive_TriangleList);
			Video::endMaterial(NULL);
		}
		virtual void onUpdateGui() override {

			if (toggle_) {
				ImGui::Text("HELLO WORLD!");
			}
			else {
				ImGui::Text(u8"こんにちは");
			}
			if (ImGui::Button("Click me!")) {
				toggle_ = !toggle_;
			}
		}
	};
	
	int WINAPI WinMain(HINSTANCE hCurrInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
		CTest test;
		EngineDef def;
		def.resolution_x = 640;
		def.resolution_y = 480;
		if (g_Engine.init(def)) {
			ImGuiVideo_Init(g_Engine.getWindow());
			ImGuiVideo_SetDefaultFont(ImGuiVideo_AddFont(FF_MEIRYO_UI, 14));
			g_Engine.addSystem(&test);
			g_Engine.addScreenCallback(&test);
			g_Engine.setWindowTitle("Box2D Test");
			g_Engine.run();
			g_Engine.removeScreenCallback(&test);
			g_Engine.callEnd();
			g_Engine.shutdown();
		}
		return 0;
	}
@endcode

*/


#pragma once
#include "__Config.h"

#include "GameAction.h"
#include "GameAnimation.h"
#include "GameAnimationCurve.h"
#include "GameAudio.h"
#include "GameCamera.h"
#include "GameCollision.h"
#include "GameGizmo.h"
#include "GameHierarchy.h"
#include "GameId.h"
#include "GameInput.h"
#include "GameLua.h"
#include "GameRender.h"
#include "GameScene.h"
#include "GameShadow.h"
#include "GameSnapshot.h"
#include "GameSprite.h"
#include "GameText.h"
#include "GameUserData.h"
#include "GameWindowDragging.h"
#include "GameWindowSizer.h"

#include "Engine.h"
#include "asset_path.h"
#include "build_callback.h"
#include "build_edge.h"
#include "build_imgpack.h"
#include "build_sprite.h"
#include "inspector.h"
#include "inspector_engine.h"
#include "inspector_entity.h"
#include "inspector_systems.h"
#include "mo_cstr.h"
#include "Screen.h"
#include "system.h"
#include "VideoBank.h"

#include "KBezier.h"
#include "KChunkedFile.h"
#include "KClock.h"
#include "KCodeTime.h"
#include "KCollisionMath.h"
#include "KColor.h"
#include "KConv.h"
#include "KCrc32.h"
#include "KCursor.h"
#include "KDialog.h"
#include "KDirScan.h"
#include "KDrawList.h"
#include "KDxGetErrorString.h"
#include "KEasing.h"
#include "KEdgeFile.h"
#include "KEmbeddedFiles.h"
#include "KErrorReport.h"
#include "KExcelFile.h"
#include "KFile.h"
#include "KFileLoader.h"
#include "KFileTime.h"
#include "KFont.h"
#include "KIcon.h"
#include "KImage.h"
#include "KImagePack.h"
#include "KImgui.h"
#include "KJoystick.h"
#include "KKeyboard.h"
#include "KLocalTime.h"
#include "KLog.h"
#include "KLoop.h"
#include "KLua.h"
#include "KLuapp.h"
#include "KMatrix4.h"
#include "KMenu.h"
#include "KMesh.h"
#include "KMouse.h"
#include "KNamedValues.h"
#include "KNum.h"
#include "KNumval.h"
#include "KOrderedParameters.h"
#include "KPacFile.h"
#include "KPath.h"
#include "KPlatform.h"
#include "KPlatformFonts.h"
#include "KQuat.h"
#include "KRandom.h"
#include "KRect.h"
#include "KRef.h"
#include "KSound.h"
#include "KStd.h"
#include "KSysInfo.h"
#include "KText.h"
#include "KThread.h"
#include "KToken.h"
#include "KVec2.h"
#include "KVec3.h"
#include "KVec4.h"
#include "KVideo.h"
#include "KWindow.h"
#include "KXmlFile.h"
#include "KZipFile.h"
#include "KZlib.h"
