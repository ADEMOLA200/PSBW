#pragma once
#include "psbw/GameObject.h"
#include "psbw/Fudgebundle.h"
#include "psbw/BWM.h"
#include "psbw/Camera.h"

typedef enum SceneType {
    SCENE_2D = 0,
    SCENE_3D = 1
};

typedef struct GAMEOBJECT_ENTRY {
    GameObject* object;
    GAMEOBJECT_ENTRY* next;
} GAMEOBJECT_ENTRY;


/**
 * \class Scene
 * \brief A container for all the GameObject within a Scene. The engine uses this to load data
 */

class Scene {
    public:
        ~Scene();
        void loadData();
        
        SceneType type = SCENE_2D;
        char* name;
        
        Vector2D *backgroundImage = nullptr;

        Camera *camera;
        
        void addGameObject(GameObject *object);
        GAMEOBJECT_ENTRY _linked_list;

        Texture* getTexture(char *name);
        Sound* getSound(char *name);
        void setBackground(char* name);
        BWM *getMesh(char *name);

        virtual void sceneSetup() = 0;
        virtual void sceneLoop() = 0;

    protected:
        Scene(char *sceneName);
        Fudgebundle* _fdg;
};