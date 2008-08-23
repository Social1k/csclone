/*
 * Clone.cpp
 *
 *  Created on: 14-Авг-08
 *      Author: sa-bu
 */

#include <Ogre.h>
#include <OIS/OIS.h>
#include <MyGUI.h>
#include "Server.h"
#include "Client.h"

#define CSCLONE_VERSION "0.0.1"

#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32
	#if OGRE_DEBUG_MODE == 1
		#define PLUGINS_CFG ".\\plugins_win_d.cfg"
	#else
		#define PLUGINS_CFG ".\\plugins.cfg"
	#endif
	#define RESOURCES_CFG ".\\resources.cfg"
	#define OGRE_CFG ".\\ogre.cfg"
	#define CSCLONE_CFG ".\\csclone.cfg"
	#define OGRE_LOG ".\\ogre.log"
#elif OGRE_PLATFORM == OGRE_PLATFORM_LINUX
	#if OGRE_DEBUG_MODE == 1
		#define PLUGINS_CFG "./plugins_linux.cfg"
		#define RESOURCES_CFG "./resources.cfg"
		#define OGRE_CFG "./ogre.cfg"
		#define CSCLONE_CFG "./blockpost.cfg"
		#define OGRE_LOG "./ogre.log"
	#else
		#define PLUGINS_CFG "/etc/csclone/plugins.cfg"
		#define RESOURCES_CFG "/etc/csclone/resources.cfg"
		#define OGRE_CFG "/etc/csclone/ogre.cfg"
		#define CSCLONE_CFG "/etc/csclone/blockpost.cfg"
		#define OGRE_LOG "/var/log/ogre.log"
	#endif
#endif

enum PlayerState {
	cmForceChooce,
	cmBuying,
	cmPlaying,
	cmGuest
};

class Application : public Ogre::FrameListener, public Ogre::WindowEventListener,
	public OIS::MouseListener, public OIS::KeyListener
{
private:
	Ogre::Root* 			mRoot;
	Ogre::RenderWindow* 	mWindow;
	Ogre::Camera* 			mCamera;
	Ogre::SceneManager*		mSceneMgr;

	OIS::InputManager*		mInputManager;
	OIS::Keyboard*			mKeyboard;
	OIS::Mouse*				mMouse;

	MyGUI::Gui*				mGUI;

	bool 					mExit;

	Ogre::Radian			mCameraAngleH;
	Ogre::Radian			mCameraAngleV;

	Server*					mServer;
	Client*					mClient;

	void initResources()
	{
		Ogre::ConfigFile cf;
		cf.load(RESOURCES_CFG);

		Ogre::ConfigFile::SectionIterator seci = cf.getSectionIterator();
		Ogre::String secName, typeName, archName;

		while (seci.hasMoreElements()) {
			secName = seci.peekNextKey();
			Ogre::ConfigFile::SettingsMultiMap* settings = seci.getNext();
			Ogre::ConfigFile::SettingsMultiMap::iterator i;
			for (i = settings->begin(); i != settings->end(); ++i) {
				typeName = i->first;
				archName = i->second;
				Ogre::ResourceGroupManager::getSingleton().addResourceLocation(
					archName, typeName, secName);
			}
		}
	}

public:
	Application() : mRoot(0), mWindow(0), mCamera(0),
		mInputManager(0), mKeyboard(0), mMouse(0), mExit(false),
		mCameraAngleH(0), mCameraAngleV(0), mServer(0), mClient(0)
	{

	}

	~Application()
	{
		Socket::uninitialise();

		Ogre::WindowEventUtilities::removeWindowEventListener(mWindow, this);

		if (mRoot) {
			if (mSceneMgr)
				mRoot->destroySceneManager(mSceneMgr);
			Ogre::WindowEventUtilities::removeWindowEventListener(mWindow, this);
			delete mRoot;
		}
	}

	void windowResized(Ogre::RenderWindow* rw)
	{
		const OIS::MouseState &ms = mMouse->getMouseState();
		ms.width = rw->getWidth();
		ms.height = rw->getHeight();
	}

	void windowClosed(Ogre::RenderWindow* rw)
	{
		mExit = true;

		if (mInputManager) {
			Ogre::LogManager::getSingletonPtr()->logMessage("*-*-* OIS Shutdown");
			mInputManager->destroyInputObject(mMouse);
			mInputManager->destroyInputObject(mKeyboard);
			OIS::InputManager::destroyInputSystem(mInputManager);
			mInputManager = 0;
		}
	}

	bool frameStarted(const Ogre::FrameEvent& evt)
	{
		if (mExit)
			return false;
		if (mWindow->isClosed())
			return false;

		mKeyboard->capture();
		mMouse->capture();

		if (mKeyboard->isKeyDown(OIS::KC_A) || mKeyboard->isKeyDown(OIS::KC_LEFT))
			mCamera->moveRelative(Ogre::Vector3(-evt.timeSinceLastFrame*20, 0, 0));

		if (mKeyboard->isKeyDown(OIS::KC_D) || mKeyboard->isKeyDown(OIS::KC_RIGHT))
			mCamera->moveRelative(Ogre::Vector3(evt.timeSinceLastFrame*20, 0, 0));

		if (mKeyboard->isKeyDown(OIS::KC_W) || mKeyboard->isKeyDown(OIS::KC_UP))
			mCamera->moveRelative(Ogre::Vector3(0, 0, -evt.timeSinceLastFrame*20));

		if (mKeyboard->isKeyDown(OIS::KC_S) || mKeyboard->isKeyDown(OIS::KC_DOWN))
			mCamera->moveRelative(Ogre::Vector3(0, 0, evt.timeSinceLastFrame*20));

		if (mServer)
			mServer->checkRecv();
		if (mClient)
			mClient->checkRecv();

		mGUI->injectFrameEntered(evt.timeSinceLastFrame);

		return true;
	}

	bool keyPressed(const OIS::KeyEvent &arg)
	{
		switch (arg.key) {
		case OIS::KC_ESCAPE:
			mExit = true;
			break;
		case OIS::KC_SPACE:
			mServer->sendChatMessageToAll("Hello, LAN!!!");
			break;
		case OIS::KC_RETURN:
			mClient->sendFindServer();
			break;
		default:
			break;
		}

		mGUI->injectKeyPress(arg);
		return true;
	}

	bool keyReleased(const OIS::KeyEvent &arg)
	{
		mGUI->injectKeyRelease(arg);
		return true;
	}

	bool mouseMoved(const OIS::MouseEvent &arg)
	{
		mCameraAngleH -= Ogre::Degree((Ogre::Real)arg.state.X.rel);
		mCameraAngleV -= Ogre::Degree((Ogre::Real)arg.state.Y.rel);
		mCamera->setOrientation(Ogre::Quaternion::IDENTITY);
		mCamera->yaw(mCameraAngleH*0.1);
		mCamera->pitch(mCameraAngleV*0.1);

		mGUI->injectMouseMove(arg);
		return true;
	}

	bool mousePressed(const OIS::MouseEvent &arg, OIS::MouseButtonID id)
	{
		mGUI->injectMousePress(arg, id);
		return true;
	}

	bool mouseReleased(const OIS::MouseEvent &arg, OIS::MouseButtonID id)
	{
		mGUI->injectMouseRelease(arg, id);
		return true;
	}

    bool initialise()
    {
		mRoot = new Ogre::Root(PLUGINS_CFG, OGRE_CFG, OGRE_LOG);

		if (!mRoot->restoreConfig())
			if (!mRoot->showConfigDialog())
				return false;

		initResources();

        mWindow = mRoot->initialise(true, "CS Clone v"CSCLONE_VERSION);
        Ogre::WindowEventUtilities::addWindowEventListener(mWindow, this);

		mSceneMgr = mRoot->createSceneManager(Ogre::ST_GENERIC);
		mSceneMgr->setAmbientLight(Ogre::ColourValue(0.7, 0.7, 0.7));
		mCamera = mSceneMgr->createCamera("camera");
        mWindow->addViewport(mCamera);
        mCamera->setAutoAspectRatio(true);
        mCamera->setNearClipDistance(0.1);
        mCamera->setFarClipDistance(10000);
        mCamera->setPosition(10, 10, 10);
//        mCamera->lookAt(0, 0, 0);

        mRoot->addFrameListener(this);

		Ogre::ResourceGroupManager::getSingleton().initialiseAllResourceGroups();
//Initializing OIS
		Ogre::LogManager::getSingletonPtr()->logMessage("*-*-* OIS Initialising");

		OIS::ParamList pl;
		size_t windowHnd = 0;
		mWindow->getCustomAttribute("WINDOW", &windowHnd);
		pl.insert(std::make_pair(std::string("WINDOW"), Ogre::StringConverter::toString(windowHnd)));

#if OGRE_DEBUG_MODE == 1
	#if OGRE_PLATFORM == OGRE_PLATFORM_LINUX
		#define NO_EXCLUSIVE_INPUT
	#endif
#endif

#ifdef NO_EXCLUSIVE_INPUT
	#if defined OIS_WIN32_PLATFORM
		pl.insert(std::make_pair(std::string("w32_mouse"), std::string("DISCL_FOREGROUND" )));
		pl.insert(std::make_pair(std::string("w32_mouse"), std::string("DISCL_NONEXCLUSIVE")));
		pl.insert(std::make_pair(std::string("w32_keyboard"), std::string("DISCL_FOREGROUND")));
		pl.insert(std::make_pair(std::string("w32_keyboard"), std::string("DISCL_NONEXCLUSIVE")));
	#elif defined OIS_LINUX_PLATFORM
		pl.insert(std::make_pair(std::string("x11_mouse_grab"), std::string("false")));
		pl.insert(std::make_pair(std::string("x11_mouse_hide"), std::string("false")));
		pl.insert(std::make_pair(std::string("x11_keyboard_grab"), std::string("false")));
		pl.insert(std::make_pair(std::string("XAutoRepeatOn"), std::string("true")));
	#endif
#endif
		mInputManager = OIS::InputManager::createInputSystem(pl);

		mKeyboard = static_cast<OIS::Keyboard*>(mInputManager->createInputObject(OIS::OISKeyboard, true));
		mKeyboard->setEventCallback(this);
		mMouse = static_cast<OIS::Mouse*>(mInputManager->createInputObject(OIS::OISMouse, true));
		mMouse->setEventCallback(this);

		windowResized(mWindow);
//Initializing GUI
		Ogre::LogManager::getSingletonPtr()->logMessage("*-*-* MyGUI Initialising");
		mGUI = new MyGUI::Gui;
		mGUI->initialise(mWindow);
		mGUI->load("csclone.layout");
		MyGUI::MultiListPtr list = mGUI->findWidget<MyGUI::MultiList>("Servers/List");
		list->addColumn(175, "Name");
		list->addColumn(85, "Map");
		list->addColumn(70, "Players");
		list->addColumn(55, "Latency");
//Initializing Game
		Socket::initialise();
		Ogre::LogManager::getSingletonPtr()->logMessage("*-*-* Initialising Game ***");
		Ogre::LogManager::getSingletonPtr()->logMessage("*-*-* CS Clone v"CSCLONE_VERSION);
		mServer = new Server("testServer");
		mClient = new Client(new WorldManagerClient(mSceneMgr));
		Ogre::LogManager::getSingletonPtr()->logMessage("*** Game initialised ***");

		mClient->loadMap("test.map");
		mClient->sendFindServer();
		return (true);
    }

	void executeCommand(const Ogre::String& command)
	{
		std::vector<Ogre::String> params;
		params = Ogre::StringUtil::split(command, " ");

		if (params[0] == "map") {
			mClient->loadMap(params[1]);
			return;
		}

		if (params[0] == "exit") {
			mExit = true;
			return;
		}

		Ogre::LogManager::getSingleton().logMessage("CS Clone: Unknown command: " + params[0]);
	}

	void run()
	{
		mRoot->startRendering();
	}
};


#ifdef __cplusplus
extern "C" {
#endif

#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32
INT WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, INT nShowCmd)
#else
int main(int argc, char** argv)
#endif
{
	Application app;

	try {
		if (!app.initialise())
			return 0;
		//command line parser
		Ogre::String command;
#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32
//		char* spch = lpCmdLine;
//		char* pch = lpCmdLine;
//		while (pch = strchr(pch, '-')) {
//
//		}
#else
//		int i = 1;
//		for (int i = 1; i < argc; i++) {
//			if (argv[i][0] == '-') {
//				command = &(argv[i][1]);
//				while (argv[i][0] != '-') {
//					command += " ";
//					command += &(argv[i][0]);
//					i++;
//				}
//				app.executeCommand(command);
//			}
//		}
#endif

		app.run();
	} catch(Ogre::Exception& e) {
#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32
		MessageBox(NULL, e.getFullDescription().c_str(), "An exception has occurred!", MB_OK|MB_ICONERROR|MB_TASKMODAL);
#else
        std::cerr << "An exception has occurred: " << e.getFullDescription();
#endif
        return 1;
	} catch(OIS::Exception& e) {
#if OGRE_PLATFORM == OGRE_PLATFORM_WIN32
		MessageBox(NULL, e.eText, "An exception has occurred!", MB_OK|MB_ICONERROR|MB_TASKMODAL);
#else
		std::cerr << "An exception has occurred: " << e.eText << " at "
			<< e.eFile << "(" << e.eLine << ")";
#endif
		return 1;
	}

	return 0;
}

#ifdef __cplusplus
}
#endif
