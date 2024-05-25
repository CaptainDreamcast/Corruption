#include "gameoverscreen.h"
#include "gamescreen.h"

static void startNewGame(void* tCaller) {
    (void)tCaller;
    resetGame();
    setNewScreen(getGameScreen());
}

class GameOverScreen
{
public:
    MugenSpriteFile sprites;
    MugenAnimations animations;

    GameOverScreen()
    {
        sprites = loadMugenSpriteFileWithoutPalette("game/GAMEOVER.sff");
        animations = loadMugenAnimationFile("game/GAMEOVER.air");

        streamMusicFileOnce("game/LOSING_JINGLE.ogg");

        addFadeIn(20, NULL, NULL);
    }

    void update(){
        if(hasPressedAFlank() || hasPressedStartFlank()){
            addFadeOut(20, startNewGame, NULL);
        }
    }
    
    void draw(){}
};
