
#include <stdio.h>
#include <chrono>
#include <thread>

#include "imgui/imgui.h"
#include "imgui_impl_glfw_game.h"
#include "imgui_impl_opengl3_game.h"
#include "glad/gl.h"
#include "GLFW/glfw3.h"
#include "draw_game.h"
#include <cstdlib>

#include "box2d/box2d.h"
#include <set>
#include <vector>

// GLFW main window pointer
GLFWwindow* g_mainWindow = nullptr;

b2World* g_world;
std::set<b2Body*> to_delete;


class GameObject
{
public:
    GameObject(b2Body* body_p) : body(body_p) {};
    GameObject::~GameObject()
    {
        if (body != nullptr) {
            g_world->DestroyBody(body);
        }
    }

    virtual void Update() = 0;
    virtual void OnKeyPress(int key, int scancode, int action, int mods) = 0;
    virtual void OnMousePress(int x, int y) = 0;

    
    bool ShouldDelete() const
    {
        return shouldDelete;
    }
protected:
    b2Body* GetBody() const;
    bool shouldDelete = false;

private:
    b2Body* body;
};

std::vector<std::unique_ptr<GameObject>> all_objects;


b2Body* GameObject::GetBody() const
{
    return body;
}

class Square : public GameObject
{
public:
    Square(b2Body* body_p) : GameObject(body_p) {}
    virtual ~Square() = default;
    virtual void OnMousePress(int x, int y) override {};
    virtual void Update() override {};

    void OnKeyPress(int key, int scancode, int action, int mods) override
    {
        if (key == GLFW_KEY_UP) 
        {
            b2Vec2 velocityVector = GetBody()->GetLinearVelocity();
            if (std::abs(velocityVector.x) + std::abs(velocityVector.y) < 0.1) {
                GetBody()->ApplyLinearImpulseToCenter(b2Vec2(0.0f, 500.0f), true);
            }
        }
    }
};

class Character {
public:
    Character(int p_size, float x, float y)
    {
        size = p_size;
        b2PolygonShape box_shape;
        box_shape.SetAsBox(size / 500.0f, size / 500.0f);
        b2FixtureDef box_fd;
        box_fd.shape = &box_shape;
        box_fd.density = 20.f;
        box_fd.friction = 0.1f;
        b2BodyDef box_bd;
        box_bd.userData.pointer = (uintptr_t)this;
        box_bd.type = b2_dynamicBody;
        box_bd.position.Set(x, y);
        body = g_world->CreateBody(&box_bd);
        body->CreateFixture(&box_fd);
    }

    void OnCollision(const Character* other)
    {
        if (other->size > size) {
            shouldDelete = true;
        }
    }

    virtual Character::~Character()
    {
        g_world->DestroyBody(body);
    }
    bool shouldDelete = false;
private:
    int size;
    b2Body* body;
};

std::vector<std::unique_ptr<Character>> characters;

class MyCollisionListener : public b2ContactListener {
public: 
    void BeginContact(b2Contact* contact) override
    {
        if (
            contact->GetFixtureA()->GetBody()->GetUserData().pointer == NULL
            || contact->GetFixtureB()->GetBody()->GetUserData().pointer == NULL
        ) 
        {
            return;
        }
        Character* character1 = (Character*)contact->GetFixtureA()->GetBody()->GetUserData().pointer;
        Character* character2 = (Character*)contact->GetFixtureB()->GetBody()->GetUserData().pointer;
        
        character1->OnCollision(character2);
        character2->OnCollision(character1);
    }
};


void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    for (auto& game_object : all_objects) {
        // code for keys here https://www.glfw.org/docs/3.3/group__keys.html
        // and modifiers https://www.glfw.org/docs/3.3/group__mods.html
        game_object->OnKeyPress(key, scancode, action, mods);
    }
}

void MouseMotionCallback(GLFWwindow*, double xd, double yd)
{
    // get the position where the mouse was pressed
    b2Vec2 ps((float)xd, (float)yd);
    // now convert this position to Box2D world coordinates
    b2Vec2 pw = g_camera.ConvertScreenToWorld(ps);
}

void MouseButtonCallback(GLFWwindow* window, int32 button, int32 action, int32 mods)
{
    // code for mouse button keys https://www.glfw.org/docs/3.3/group__buttons.html
    // and modifiers https://www.glfw.org/docs/3.3/group__buttons.html
    // action is either GLFW_PRESS or GLFW_RELEASE
    double xd, yd;
    // get the position where the mouse was pressed
    glfwGetCursorPos(g_mainWindow, &xd, &yd);
    b2Vec2 ps((float)xd, (float)yd);
    // now convert this position to Box2D world coordinates
    b2Vec2 pw = g_camera.ConvertScreenToWorld(ps);

    if (action == GLFW_PRESS) {
        characters.push_back(std::make_unique<Character>(yd, pw.x, pw.y));
       /* b2Body* box;
        b2PolygonShape box_shape;
        box_shape.SetAsBox(1.0f, 1.0f);
        b2FixtureDef box_fd;
        box_fd.shape = &box_shape;
        box_fd.density = 20.0f;
        box_fd.friction = 0.1f;
        b2BodyDef box_bd;
        box_bd.type = b2_dynamicBody;
        box_bd.position.Set(pw.x, pw.y);
        box = g_world->CreateBody(&box_bd);
        box->CreateFixture(&box_fd);*/
    }
}



int main()
{

    // glfw initialization things
    if (glfwInit() == 0) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return -1;
    }

    g_mainWindow = glfwCreateWindow(g_camera.m_width, g_camera.m_height, "My game", NULL, NULL);

    if (g_mainWindow == NULL) {
        fprintf(stderr, "Failed to open GLFW g_mainWindow.\n");
        glfwTerminate();
        return -1;
    }

    // Set callbacks using GLFW
    glfwSetMouseButtonCallback(g_mainWindow, MouseButtonCallback);
    glfwSetKeyCallback(g_mainWindow, KeyCallback);
    glfwSetCursorPosCallback(g_mainWindow, MouseMotionCallback);

    glfwMakeContextCurrent(g_mainWindow);

    // Load OpenGL functions using glad
    int version = gladLoadGL(glfwGetProcAddress);

    // Setup Box2D world and with some gravity
    b2Vec2 gravity;
    gravity.Set(0.0f, -10.0f);
    g_world = new b2World(gravity);

    // Create debug draw. We will be using the debugDraw visualization to create
    // our games. Debug draw calls all the OpenGL functions for us.
    g_debugDraw.Create();
    g_world->SetDebugDraw(&g_debugDraw);
    CreateUI(g_mainWindow, 20.0f /* font size in pixels */);


    // Some starter objects are created here, such as the ground
    b2Body* ground;
    b2EdgeShape ground_shape;
    ground_shape.SetTwoSided(b2Vec2(-40.0f, 0.0f), b2Vec2(40.0f, 0.0f));
    b2BodyDef ground_bd;
    ground = g_world->CreateBody(&ground_bd);
    ground->CreateFixture(&ground_shape, 0.0f);

    //Jonathans Dominos

    b2PolygonShape shape;
    shape.SetAsBox(0.1f, 1.0f);

    b2FixtureDef fd;
    fd.shape = &shape;
    fd.density = 20.0f;
    fd.friction = 0.1f;

    for (int i = 0; i < 10; ++i)
    {
        b2BodyDef bd;
        bd.type = b2_dynamicBody;
        bd.position.Set(-5.0f + 1.0f * i, 0.01f);
        b2Body* body = g_world->CreateBody(&bd);
        body->SetAngularVelocity(1.0f);
        body->CreateFixture(&fd);
    }

    b2Body* box;
    b2PolygonShape box_shape;
    box_shape.SetAsBox(1.0f, 1.0f);
    b2FixtureDef box_fd;
    box_fd.shape = &box_shape;
    box_fd.density = 20.0f;
    box_fd.friction = 0.1f;
    b2BodyDef box_bd;
    box_bd.type = b2_dynamicBody;
    box_bd.position.Set(8.0f, 11.25f);
    box = g_world->CreateBody(&box_bd);
    box->CreateFixture(&box_fd);

    //std::make_unique<Square>(box);
    
    all_objects.push_back(std::make_unique<Square>(box));
    

    MyCollisionListener collision;
    g_world->SetContactListener(&collision);

    // This is the color of our background in RGB components
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);

    // Control the frame rate. One draw per monitor refresh.
    std::chrono::duration<double> frameTime(0.0);
    std::chrono::duration<double> sleepAdjust(0.0);

    // Main application loop
    while (!glfwWindowShouldClose(g_mainWindow)) {
        // Use std::chrono to control frame rate. Objective here is to maintain
        // a steady 60 frames per second (no more, hopefully no less)
        std::chrono::steady_clock::time_point t1 = std::chrono::steady_clock::now();

        glfwGetWindowSize(g_mainWindow, &g_camera.m_width, &g_camera.m_height);

        int bufferWidth, bufferHeight;
        glfwGetFramebufferSize(g_mainWindow, &bufferWidth, &bufferHeight);
        glViewport(0, 0, bufferWidth, bufferHeight);

        // Clear previous frame (avoid creates shadows)
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // Setup ImGui attributes so we can draw text on the screen. Basically
        // create a window of the size of our viewport.
        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(ImVec2(float(g_camera.m_width), float(g_camera.m_height)));
        ImGui::SetNextWindowBgAlpha(0.0f);
        ImGui::Begin("Overlay", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar);
        ImGui::End();

        // Enable objects to be draw
        uint32 flags = 0;
        flags += b2Draw::e_shapeBit;
        g_debugDraw.SetFlags(flags);

        // When we call Step(), we run the simulation for one frame
        float timeStep = 60 > 0.0f ? 1.0f / 60 : float(0.0f);
        g_world->Step(timeStep, 8, 3);
        
        characters.erase(
            std::remove_if(characters.begin(), characters.end(), [](const auto& character) {
                return character->shouldDelete;
                }),
            characters.end()
        );

        for (b2Body* b = g_world->GetBodyList(); b; b = b->GetNext())
        {
            if (b->GetType() == b2_dynamicBody)
            {
                b2ContactEdge* edge = b->GetContactList();
                while (edge != nullptr) {
                    if (edge->contact->IsTouching()) {
                        if (edge->other->GetType() == b2_dynamicBody) {
                            to_delete.insert(edge->other);
                        }
                    }
                    edge = edge->next;
                }
            }
        }
       /* all_objects.erase(
            std::remove_if(
                all_objects.begin(), all_objects.end()
                , [](const std::unique_ptr<GameObject> object) {return object->ShouldDelete(); })
                , all_objects.end()
            );*/

        for (auto element : to_delete) {
           g_world->DestroyBody(element);
        }
        to_delete.clear();
      
        // Render everything on the screen
        g_world->DebugDraw();
        g_debugDraw.Flush();
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(g_mainWindow);

        // Process events (mouse and keyboard) and call the functions we
        // registered before.
        glfwPollEvents();

        // Throttle to cap at 60 FPS. Which means if it's going to be past
        // 60FPS, sleeps a while instead of doing more frames.
        std::chrono::steady_clock::time_point t2 = std::chrono::steady_clock::now();
        std::chrono::duration<double> target(1.0 / 60.0);
        std::chrono::duration<double> timeUsed = t2 - t1;
        std::chrono::duration<double> sleepTime = target - timeUsed + sleepAdjust;
        if (sleepTime > std::chrono::duration<double>(0)) {
            // Make the framerate not go over by sleeping a little.
            std::this_thread::sleep_for(sleepTime);
        }
        std::chrono::steady_clock::time_point t3 = std::chrono::steady_clock::now();
        frameTime = t3 - t1;

        // Compute the sleep adjustment using a low pass filter
        sleepAdjust = 0.9 * sleepAdjust + 0.1 * (target - frameTime);

        
    }

    // Terminate the program if it reaches here
    glfwTerminate();
    g_debugDraw.Destroy();
    delete g_world;

    return 0;
}
