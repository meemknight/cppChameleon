#include "gameLayer.h"
#include "ClientGameplay.h"
#include "ServerGameplay.h"

#include <enet/enet.h>
#include <gl2d/gl2d.h>
#include <glad/glad.h>
#include <glui/glui.h>


gl2d::Renderer2D renderer;
ClientGameplay clientGameplay;
ServerGameplay serverGameplay;

glui::RendererUi rendererUI;
gl2d::Texture uiTexture;
gl2d::Font font;
char serverIpAddress[128] = "localhost";
std::string menuStatusMessage;
bool enetInitialized = false;


bool initGame()
{
	if (enet_initialize() != 0)
	{
		return false;
	}
	enetInitialized = true;

	gl2d::init();
	renderer.create();

	uiTexture.loadFromFile(RESOURCES_PATH "ui.png", true, true);
	font.createFromFile(RESOURCES_PATH "Arial.ttf");

	return true;
}


enum gameState
{
	inMainMenu = 0,
	inGame = 1,
	inServer = 2

};

int currentGameState = 0;

bool gameLogic(float deltaTime, platform::Input &input)
{
	const int w = platform::getFrameBufferSizeX();
	const int h = platform::getFrameBufferSizeY();

	glViewport(0, 0, w, h);
	glDisable(GL_DITHER);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	bool result = 1;

	renderer.updateWindowMetrics(w, h);

	if (currentGameState == 0)
	{

		rendererUI.Begin(0);


		rendererUI.Text("C++ Chameleon", Colors_White);
		rendererUI.SetAlignModeFixedSizeWidgets({0, 200});
		rendererUI.InputText("connect IP: ", serverIpAddress, sizeof(serverIpAddress), Colors_White);

		if (rendererUI.Button("Start client", Colors_White, uiTexture))
		{
			clientGameplay.shutdown();

			if (clientGameplay.init(serverIpAddress))
			{
				currentGameState = inGame;
				menuStatusMessage.clear();
			}
			else
			{
				menuStatusMessage = clientGameplay.clientNetworking.lastStatus;
			}
		}

		if (rendererUI.Button("Start server", Colors_White, uiTexture))
		{
			serverGameplay.close();

			if (serverGameplay.init())
			{
				currentGameState = inServer;
				menuStatusMessage.clear();
			}
			else
			{
				menuStatusMessage = "Failed to start server on port 7769.";
			}
		}

		if (!menuStatusMessage.empty())
		{
			rendererUI.Text(menuStatusMessage, Colors_Red);
		}

		rendererUI.End();

		rendererUI.renderFrame(renderer, font, platform::getRelMousePosition(),
			platform::isLMousePressed(), platform::isLMouseHeld(), platform::isLMouseReleased(),
			platform::isButtonReleased(platform::Button::Escape), platform::getTypedInput(),
			deltaTime);

	}
	else if (currentGameState == inGame)
	{
		result = clientGameplay.update(deltaTime, input, renderer, font);
	}
	else if (currentGameState == inServer)
	{
		serverGameplay.update();

		rendererUI.Begin(1);
		rendererUI.Text("Server running on port 7769", Colors_White);
		rendererUI.Text("Clients connected: " + std::to_string(serverGameplay.connectedClients), Colors_White);
		rendererUI.Text(serverGameplay.connectedClients == 0 ? "Waiting for clients..." : "Clients joined.", Colors_White);
		rendererUI.End();

		rendererUI.renderFrame(renderer, font, platform::getRelMousePosition(),
			platform::isLMousePressed(), platform::isLMouseHeld(), platform::isLMouseReleased(),
			platform::isButtonReleased(platform::Button::Escape), platform::getTypedInput(),
			deltaTime);
	}



	renderer.flush();
	return result;
}

void closeGame()
{
	clientGameplay.shutdown();
	serverGameplay.close();

	if (enetInitialized)
	{
		enet_deinitialize();
		enetInitialized = false;
	}
}
