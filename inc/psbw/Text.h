#include "psbw/Component.h"
#include "psbw/Vector.h"
#include "psbw/Texture.h"
#include "psbw/GameObject.h"
#include "psbw/Font.h";

class Text : public Component {
    public:
        char* text;
        void execute(GameObject* parent) override;
        void setFont(Font* font);
    private:
        Font* _fnt;
};