#include "gameLayer.h"
#include "ClientGameplay.h"

#include <gl2d/gl2d.h>
#include <glad/glad.h>
#include <glui/glui.h>


gl2d::Renderer2D renderer;
ClientGameplay clientGameplay;

glui::RendererUi rendererUI;
gl2d::Texture uiTexture;
gl2d::Font font;


bool initGame()
{
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

		if (rendererUI.Button("Start game", Colors_White, uiTexture))
		{
			currentGameState = inGame;
			clientGameplay.init(renderer);
		}


		rendererUI.End();

		rendererUI.renderFrame(renderer, font, platform::getRelMousePosition(),
			platform::isLMousePressed(), platform::isLMouseHeld(), platform::isLMouseReleased(),
			platform::isButtonReleased(platform::Button::Escape), platform::getTypedInput(),
			deltaTime);

	}
	else if (currentGameState == inGame)
	{
		result = clientGameplay.update(deltaTime, input, renderer);
	}



	renderer.flush();
	return result;
}

void closeGame()
{
	clientGameplay.shutdown();
}
