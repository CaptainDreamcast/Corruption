#include "gamescreen.h"

#include "storyscreen.h"

static struct{
    int mLevel;
} gGameScreenData;

class GameScreen
{
public:
    MugenSpriteFile sprites;
    MugenAnimations animations;
    MugenSounds sounds;

    GameScreen()
    {
        sprites = loadMugenSpriteFileWithoutPalette("game/GAME.sff");
        animations = loadMugenAnimationFile("game/GAME.air");
        sounds = loadMugenSoundFile("game/GAME.snd");

        loadLevel();
        loadPlayerCharacter();
        loadEnemies();
        loadUI();
        loadTextEffects();
        addCollisionHandlerCheck(playerCollisionList, enemyCollisionList);

        if(gGameScreenData.mLevel != 2)
        {
            streamMusicFile("game/FIGHT.ogg");
        }
    }

    struct ActiveTextEffect
    {
        MugenAnimationHandlerElement* animation;
        int showTime;
        int removeTime;
    };
    std::vector<ActiveTextEffect> textEffects;

    int textEffectTime = 0;
    void startTextEffect(const std::vector<int>& effectAnimations)
    {
        for (int i = 0; i < effectAnimations.size(); i++) {
            auto animation = addMugenAnimation(getMugenAnimation(&animations, effectAnimations[i]), &sprites, Vector3D(160, 100 + 20 * i, 70));
            setMugenAnimationVisibility(animation, 0);
            textEffects.push_back(ActiveTextEffect{ animation, 40 * i, gGameScreenData.mLevel == 2 ? 300 : 240 });
        }
        textEffectTime = 0;
    }

    void loadTextEffects()
    {
        if(gGameScreenData.mLevel == 0)
        {
            startTextEffect({ 30, 31 });
        }
        else if(gGameScreenData.mLevel == 1)
        {
            startTextEffect({ 32, 33 });
        }
        else if(gGameScreenData.mLevel == 2)
        {
            startTextEffect({ 34, 35, 36 });
        }
        tryPlayMugenSound(&sounds, 2, gGameScreenData.mLevel);
    }

    double levelYToZ(double y)
    {
        return 10 + y / 240.0 * 19;
    }

    int life = 1000;
    int lifeMax = 1000;
    int special = 0;
    int specialMax = 1000;
    MugenAnimationHandlerElement* lifeBarBGAnimationElement;
    MugenAnimationHandlerElement* lifeBarFGAnimationElement;
    MugenAnimationHandlerElement* specialBarFGAnimationElement;

    void loadUI()
    {
        lifeBarBGAnimationElement = addMugenAnimation(getMugenAnimation(&animations, 60), &sprites, Vector3D(10, 10, 60));
        lifeBarFGAnimationElement = addMugenAnimation(getMugenAnimation(&animations, 61), &sprites, Vector3D(10, 10, 61) + Vector2D(11, 3));
        specialBarFGAnimationElement = addMugenAnimation(getMugenAnimation(&animations, 62), &sprites, Vector3D(10, 10, 61) + Vector2D(11, 9));
        //special = 1000;
        updateSpecialBar();
    }

    struct Enemy {
        int entityID;
        int isFacingRight;
        Vector2D target;
        int collisionID;

        // state
        int wasHit = 0;
        int isHittable = 1;
        int life = 100;
        int remove = 0;
    };

    std::list<Enemy> enemies;

    std::vector<int> enemyCounts = { 20, 30, 1};

    void addEnemy()
    {
        auto playerPos = getBlitzEntityPosition(playerEntityId);
        auto x = randfrom(160, 320);
        auto y = randfrom(100, 240);
        while (getDistance2D(playerPos, Vector3D(x, y, 0)) < 50) {
            x = randfrom(160, 320);
            y = randfrom(100, 240);
        }

        if (gGameScreenData.mLevel == 2)
        {
            x = 240;
            y = 180;
        }

        auto enemyEntityID = addBlitzEntity(Vector3D(x, y, levelYToZ(y)));
        addBlitzMugenAnimationComponent(enemyEntityID, &sprites, &animations, 10);
        setBlitzMugenAnimationFaceDirection(enemyEntityID, x < 160);
        addBlitzCollisionComponent(enemyEntityID);
        auto collisionId = addBlitzCollisionPassiveMugen(enemyEntityID, enemyCollisionList);
        setBlitzCollisionCollisionData(enemyEntityID, collisionId, this);
        enemies.push_back({ enemyEntityID, x < 160, Vector2D(x, y), collisionId });
        addBlitzCollisionCB(enemyEntityID, collisionId, [](void* tCaller, void* tCollisionData) {
            auto enemy = (Enemy*)tCaller;
            GameScreen* gameScreen = (GameScreen*)tCollisionData;
            if(!enemy->isHittable) return;
            auto pos = getBlitzEntityPosition(enemy->entityID);
            auto playerPos = getBlitzEntityPosition(gameScreen->playerEntityId);
            if(abs(pos.y - playerPos.y) > 20) return;
            enemy->wasHit = 1;
        }, &enemies.back());
    }

    CollisionListData* enemyCollisionList;
    void loadEnemies()
    {
        enemyCollisionList = addCollisionListToHandler();
        for (int i = 0; i < enemyCounts[gGameScreenData.mLevel]; i++) {
            addEnemy();
        };
    }

    MugenAnimationHandlerElement* bgAnimation;
    void loadLevel()
    {
        bgAnimation = addMugenAnimation(getMugenAnimation(&animations, 1), &sprites, Vector3D(0, 0, 1));
    }

    CollisionListData* playerCollisionList;
    int isFacingRight = 1;
    int playerEntityId;
    MugenAnimationHandlerElement* specialBarEffectAnimation;
    void loadPlayerCharacter()
    {
        playerCollisionList = addCollisionListToHandler();

        playerEntityId = addBlitzEntity(Vector3D(80, 180, 20));
        addBlitzMugenAnimationComponent(playerEntityId, &sprites, &animations, 40);
        setBlitzMugenAnimationFaceDirection(playerEntityId, !isFacingRight);
        addBlitzCollisionComponent(playerEntityId);
        int collisionId = addBlitzCollisionAttackMugen(playerEntityId, playerCollisionList);
        setBlitzCollisionCollisionData(playerEntityId, collisionId, this);

        specialBarEffectAnimation = addMugenAnimation(getMugenAnimation(&animations, 50), &sprites, Vector3D(80, 260, 19));
    }


    void update(){
        updatePlayer();
        updateEnemies();
        updateTextEffects();
        updateWinning();
    }

    int isFadingOut = 0;
    int hasWon = 0;
    void updateWinning()
    {
        // if there are no more enemies, set hasWon to true
        if (!hasWon && enemies.empty()) {
            hasWon = 1;
            startTextEffect({ 70 });
        }

        if(hasWon && textEffects.empty() && !isFadingOut)
        {
            addFadeOut(20, [](void*) {
                gGameScreenData.mLevel++;
                if(gGameScreenData.mLevel == 3)
                {
                    setCurrentStoryDefinitionFile("game/OUTRO.def", 0);
                    setNewScreen(getStoryScreen());
                }
                else
                {
                    setNewScreen(getGameScreen());
                }
            }, NULL);
            isFadingOut = 1;
        }
    }

    void updateTextEffects()
    {
        // if there are no text effects, early out
        if (textEffects.empty()) return;

        for (auto& textEffect : textEffects) {
            updateSingleTextEffect(textEffect);
        }

        // if all text effects are beyond remove time + 20, clear out text effects
        if (textEffectTime > textEffects.back().removeTime + 20) {
            textEffects.clear();
        }

        textEffectTime++;
    }

    void updateSingleTextEffect(ActiveTextEffect& textEffect)
    {
        // if time < show time, set the animation invisible
        if (textEffectTime < textEffect.showTime) {
            setMugenAnimationVisibility(textEffect.animation, 0);
        }
        // if time > show time and time < show time + 20, fade the animation in
        else if (textEffectTime < textEffect.showTime + 20) {
            setMugenAnimationVisibility(textEffect.animation, 1);
            setMugenAnimationTransparency(textEffect.animation, (textEffectTime - textEffect.showTime) / 20.0);
        }
        // if time > remove time and time < remove time + 20, fade the animation out
        else if (textEffectTime > textEffect.removeTime && textEffectTime < textEffect.removeTime + 20) {
            setMugenAnimationTransparency(textEffect.animation, 1 - (textEffectTime - textEffect.removeTime) / 20.0);
        }
        // if time > remove time + 20, set the animation invisible
        else if (textEffectTime > textEffect.removeTime + 20) {
            setMugenAnimationVisibility(textEffect.animation, 0);
        }
    }

    int areEnemiesActive()
    {
        return textEffects.empty();
    }

    void updateEnemies()
    {
        updateEnemiesWalking();
        updateEnemiesGettingHit();
        updateEnemiesZ();
        updateEnemiesDying();
    }

    void updateEnemiesZ()
    {
        for (auto& enemy : enemies) {
            auto pos = getBlitzEntityPosition(enemy.entityID);
            pos.z = levelYToZ(pos.y);
            setBlitzEntityPosition(enemy.entityID, pos);
        }
    }

    void updateEnemiesDying()
    {
        auto it = enemies.begin();
        while (it != enemies.end()) {
            auto next = it;
            next++;
            if (getBlitzMugenAnimationAnimationNumber(it->entityID) == 14 && getBlitzMugenAnimationRemainingAnimationTime(it->entityID) == 0){
                removeBlitzEntity(it->entityID);
                enemies.erase(it);
            }
            it = next;
        }
    }

    int hasHitThisFrame = 0;
    void updateEnemiesGettingHit()
    {
        hasHitThisFrame = 0;
        for (auto& enemy : enemies) {
            updateSingleEnemyGettingHit(enemy);
        }
        if (hasHitThisFrame)
        {
            tryPlayMugenSound(&sounds, 1, 4);
        }
    }

    int enemyCanNoLongerBeHit(Enemy& enemy)
    {
        return getBlitzMugenAnimationAnimationNumber(enemy.entityID) == 14;
    }

    void updateSpecialBar()
    {
        setMugenAnimationRectangleWidth(specialBarFGAnimationElement, 1.0 * special / specialMax * 170);
    }

    void showHitEffect(const Vector3D& tPos)
    {
        auto animation = addMugenAnimation(getMugenAnimation(&animations, randfromInteger(20, 23)), &sprites, tPos.xy().xyz(50) - Vector2D(0, 33));
        setMugenAnimationVisibility(animation, 1);
        setMugenAnimationTransparency(animation, 0.5);
        setMugenAnimationNoLoop(animation);
    }

    void updateSingleEnemyGettingHit(Enemy& enemy)
    {
        if(enemyCanNoLongerBeHit(enemy)) return;

        if (enemy.wasHit) {
            enemy.wasHit = 0;
            enemy.isHittable = 0;
            enemy.life -= isInSpecialMode ? 30 : 10;
            special = min(special + 10, specialMax);
            hasHitThisFrame = 1;
            updateSpecialBar();
            showHitEffect(getBlitzEntityPosition(enemy.entityID));
            if(enemy.life <= 0)
            {
                changeBlitzMugenAnimation(enemy.entityID, 14);
            }
            else{
                changeBlitzMugenAnimation(enemy.entityID, 13);
            }
        }

        auto animationNo = getBlitzMugenAnimationAnimationNumber(enemy.entityID);
        if (animationNo == 13 && getBlitzMugenAnimationRemainingAnimationTime(enemy.entityID) == 0) {
            changeBlitzMugenAnimation(enemy.entityID, 10);
        }
    }

    void updateEnemiesWalking()
    {
        if (!areEnemiesActive()) return;
        for (auto& enemy : enemies) {
            updateSingleEnemyWalking(enemy);
        }
    }

    int canEnemyWalk(Enemy& enemy)
    {
        return  getBlitzMugenAnimationAnimationNumber(enemy.entityID) != 14 && getBlitzMugenAnimationAnimationNumber(enemy.entityID) != 13;
    }

    void updateSingleEnemyWalking(Enemy& enemy)
    {
        if (!canEnemyWalk(enemy)) return;
        auto pos = getBlitzEntityPosition(enemy.entityID);
        if (getDistance2D(pos.xy(), enemy.target) < 2) {
            enemy.target = Vector2D(randfrom(0, 320), randfrom(100, 240));
        }

        // move towards target
        auto dx = enemy.target - pos.xy();
        dx = vecNormalize(dx);
        pos.x += dx.x;
        pos.y += dx.y;
        setBlitzEntityPosition(enemy.entityID, pos);

        if (vecLength(dx) > 0)
        {
            changeBlitzMugenAnimationIfDifferent(enemy.entityID, 11);
        }
        else
        {
            changeBlitzMugenAnimationIfDifferent(enemy.entityID, 10);
        }

        // face player
        auto playerPos = getBlitzEntityPosition(playerEntityId);
        enemy.isFacingRight = pos.x < playerPos.x;
        setBlitzMugenAnimationFaceDirection(enemy.entityID, enemy.isFacingRight);
    }

    int isPlayerActive()
    {
        return textEffects.empty();
    }

    void updatePlayer()
    {
        updatePlayerWalking();
        updatePlayerPunching();
        updatePlayerSpecialEffect();
        updatePlayerZ();
    }

    void updatePlayerZ()
    {
        auto pos = getBlitzEntityPosition(playerEntityId);
        pos.z = levelYToZ(pos.y);
        setBlitzEntityPosition(playerEntityId, pos);
    }

    int isInSpecialMode = 0;

    void updatePlayerSpecialEffect(){
        updatePlayerSpecialEffectActivation();
        updatePlayerSpecialEffectFinish();
        updatePlayerSpecialEffectAnimation();
    }

    void updatePlayerSpecialEffectActivation()
    {
        if(isInSpecialMode) return;
        if (special >= specialMax && hasPressedBFlank()) {
            isInSpecialMode = 1;
        }
    }

    void updatePlayerSpecialEffectFinish()
    {
        if(!isInSpecialMode) return;
            
        special = std::max(special - 5, 0);
        updateSpecialBar();

        if(special == 0)
        {
            isInSpecialMode = 0;
        }
    }

    void updatePlayerSpecialEffectAnimation()
    {
        auto pos = getBlitzEntityPosition(playerEntityId);
        setMugenAnimationPosition(specialBarEffectAnimation, pos - Vector3D(0, 0, 1));
        setMugenAnimationVisibility(specialBarEffectAnimation, isInSpecialMode);
    }

    int canPlayerPunch()
    {
        return 1;
    }

    int isInPunch()
    {
        return getBlitzMugenAnimationAnimationNumber(playerEntityId) == 44 || getBlitzMugenAnimationAnimationNumber(playerEntityId) == 45 || getBlitzMugenAnimationAnimationNumber(playerEntityId) == 46;
    }

    void startPunch()
    {
        playMugenSound(&sounds, 1, 6);
    }

    void setAllEnemiesHittable()
    {
        for(auto& enemy : enemies)
        {
            enemy.isHittable = 1;
        }
    }

    void updatePlayerPunching()
    {
        if (!isPlayerActive()) return;
        if(!canPlayerPunch()) return;

        int animationNo = getBlitzMugenAnimationAnimationNumber(playerEntityId);

        if(hasPressedAFlank())
        {
            if(animationNo == 44)
            {
                changeBlitzMugenAnimation(playerEntityId, 45);
                startPunch();
            }
            else if(animationNo == 45)
            {
                changeBlitzMugenAnimation(playerEntityId, 46);
                startPunch();
            }
            else
            {
                changeBlitzMugenAnimation(playerEntityId, 44);
                startPunch();
            }
            setAllEnemiesHittable();

        }

        if(isInPunch() && getBlitzMugenAnimationRemainingAnimationTime(playerEntityId) == 0)
        {
            changeBlitzMugenAnimation(playerEntityId, 40);
        }
    }

    int canWalk()
    {
        int animationNo = getBlitzMugenAnimationAnimationNumber(playerEntityId);
        return animationNo == 40 || animationNo == 41;
    }

    void updatePlayerWalking()
    {
        if (!isPlayerActive()) return;
        if(!canWalk()) return;
        Vector2DI dx = Vector2DI(0, 0);

        if (hasPressedLeft()) {
            dx.x = -1;
        }
        else if (hasPressedRight()) {
           dx.x = 1;
        }

        if(hasPressedUp()){
            dx.y = -1;
        }
        else if(hasPressedDown()){
            dx.y = 1;
        }

        if(dx.x < 0)
        {
            isFacingRight = 0;
        }
        else if(dx.x > 0)
        {
            isFacingRight = 1;
        }
        setBlitzMugenAnimationFaceDirection(playerEntityId, !isFacingRight);

        if(vecLength(dx) > 0)
        {
            changeBlitzMugenAnimationIfDifferent(playerEntityId, 41);
        }
        else
        {
            changeBlitzMugenAnimationIfDifferent(playerEntityId, 40);
        }

        const auto speed = isInSpecialMode ? 5 : 3;
        auto pos = getBlitzEntityPosition(playerEntityId);
        pos = clampPositionToGeoRectangle(pos + dx * speed, GeoRectangle2D(0, 100, 320, 140));
        setBlitzEntityPosition(playerEntityId, pos);
    }

    void draw(){}

    void reset()
    {
        gGameScreenData.mLevel = 0;
    }
};

EXPORT_SCREEN_CLASS(GameScreen);

void resetGame()
{
    gGameScreen->reset();
}