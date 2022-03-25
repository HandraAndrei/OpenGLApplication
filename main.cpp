#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp> //core glm functionality
#include <glm/gtc/matrix_transform.hpp> //glm extension for generating common transformation matrices
#include <glm/gtc/matrix_inverse.hpp> //glm extension for computing inverse matrices
#include <glm/gtc/type_ptr.hpp> //glm extension for accessing the internal data structure of glm types

#include "Window.h"
#include "Shader.hpp"
#include "Camera.hpp"
#include "Model3D.hpp"
#include "SkyBox.hpp"

#include <iostream>

// window
gps::Window myWindow;

// matrices
glm::mat4 model;
glm::mat4 view;
glm::mat4 projection;
glm::mat3 normalMatrix;

// light parameters
glm::vec3 lightDir;
glm::vec3 lightColor;
glm::vec3 lightPosition;
glm::vec3 lightPosOn;
bool withLight = false;

//fog density
glm::vec3 fogDensity;
GLint fogDensityLoc;
bool withFog = false;

// shader uniform locations
GLint modelLoc;
GLint viewLoc;
GLint projectionLoc;
GLint normalMatrixLoc;
GLint lightDirLoc;
GLint lightColorLoc;

// camera
gps::Camera myCamera(
    glm::vec3(6.41f, 0.68f, 5.04f),
    glm::vec3(6.41f, 0.68f, 5.06f),
    glm::vec3(0.0f, 1.0f, 0.0f));

GLfloat cameraSpeed = 0.005f;

GLboolean pressedKeys[1024];

// models
gps::Model3D teapot;
GLfloat angle;
gps::Model3D scene;
gps::Model3D street_light;
gps::Model3D tractor;
gps::Model3D tractor_onRoad;
gps::Model3D boat;
gps::Model3D duck;
gps::Model3D gray_dog;
gps::Model3D white_dog;
gps::Model3D screenQuad;

// shaders
gps::Shader myBasicShader;
gps::Shader depthMapShader;
gps::Shader screenQuadShader;

//shadow
GLuint shadowMapFBO;
GLuint depthMapTexture;
bool showDepthMap;
const unsigned int SHADOW_WIDTH = 2048;
const unsigned int SHADOW_HEIGHT = 2048;
bool startPres = false;

//skybox
gps::SkyBox mySkyBox;
gps::Shader skyboxShader;

GLenum glCheckError_(const char *file, int line)
{
	GLenum errorCode;
	while ((errorCode = glGetError()) != GL_NO_ERROR) {
		std::string error;
		switch (errorCode) {
            case GL_INVALID_ENUM:
                error = "INVALID_ENUM";
                break;
            case GL_INVALID_VALUE:
                error = "INVALID_VALUE";
                break;
            case GL_INVALID_OPERATION:
                error = "INVALID_OPERATION";
                break;
            case GL_STACK_OVERFLOW:
                error = "STACK_OVERFLOW";
                break;
            case GL_STACK_UNDERFLOW:
                error = "STACK_UNDERFLOW";
                break;
            case GL_OUT_OF_MEMORY:
                error = "OUT_OF_MEMORY";
                break;
            case GL_INVALID_FRAMEBUFFER_OPERATION:
                error = "INVALID_FRAMEBUFFER_OPERATION";
                break;
        }
		std::cout << error << " | " << file << " (" << line << ")" << std::endl;
	}
	return errorCode;
}
#define glCheckError() glCheckError_(__FILE__, __LINE__)

void windowResizeCallback(GLFWwindow* window, int width, int height) {
	fprintf(stdout, "Window resized! New width: %d , and height: %d\n", width, height);
	//TODO
}

void keyboardCallback(GLFWwindow* window, int key, int scancode, int action, int mode) {
	if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GL_TRUE);
    }

    if (key == GLFW_KEY_M && action == GLFW_PRESS)
        showDepthMap = !showDepthMap;

	if (key >= 0 && key < 1024) {
        if (action == GLFW_PRESS) {
            pressedKeys[key] = true;
        } else if (action == GLFW_RELEASE) {
            pressedKeys[key] = false;
        }
    }
}


float lastX = myWindow.getWindowDimensions().width / 2, lastY = myWindow.getWindowDimensions().height / 2;
float yaw = -90.0f;
float pitch = 0.0f;
bool firstMouse = true;
bool mouseMoved = false;

void mouseCallback(GLFWwindow* window, double xpos, double ypos) {
    //TODO
    if (firstMouse)
    {
       lastX = xpos;
       lastY = ypos;
       firstMouse = false;
    }
    
    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    const float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;
    
    if (pitch > 89.0f)
        pitch = 89.0f;
    if (pitch < -89.0f)
        pitch = -89.0f;
    
    mouseMoved = true;
    myCamera.rotate(pitch, yaw);
}

void initFBO() { //for depth map texture,shadow algorithm
    glGenFramebuffers(1, &shadowMapFBO);

    //create depth texture for FBO
    glGenTextures(1, &depthMapTexture);
    glBindTexture(GL_TEXTURE_2D, depthMapTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
        SHADOW_WIDTH, SHADOW_HEIGHT, 0,
        GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    float borderColor[] = { 1.0f,1.0f,1.0f,1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

    //attach texture to FBO
    glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMapTexture, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

bool powerOn = false;
bool powerBoat = false;

void processMovement() {
	if (pressedKeys[GLFW_KEY_W]) {
		myCamera.move(gps::MOVE_FORWARD, cameraSpeed);
		//update view matrix
        view = myCamera.getViewMatrix();
        myBasicShader.useShaderProgram();
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        // compute normal matrix for teapot
        normalMatrix = glm::mat3(glm::inverseTranspose(view*model));
        
	}

	if (pressedKeys[GLFW_KEY_S]) {
		myCamera.move(gps::MOVE_BACKWARD, cameraSpeed);
        //update view matrix
        view = myCamera.getViewMatrix();
        myBasicShader.useShaderProgram();
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        // compute normal matrix for teapot
        normalMatrix = glm::mat3(glm::inverseTranspose(view*model));
        
	}

	if (pressedKeys[GLFW_KEY_A]) {
		myCamera.move(gps::MOVE_LEFT, cameraSpeed);
        //update view matrix
        view = myCamera.getViewMatrix();
        myBasicShader.useShaderProgram();
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        // compute normal matrix for teapot
        normalMatrix = glm::mat3(glm::inverseTranspose(view*model));
        
	}

	if (pressedKeys[GLFW_KEY_D]) {
		myCamera.move(gps::MOVE_RIGHT, cameraSpeed);
        //update view matrix
        view = myCamera.getViewMatrix();
        myBasicShader.useShaderProgram();
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        // compute normal matrix for teapot
        normalMatrix = glm::mat3(glm::inverseTranspose(view*model));
        
	}

    if (pressedKeys[GLFW_KEY_Q]) {
        angle -= 1.0f;
        // update model matrix for teapot
        model = glm::rotate(glm::mat4(1.0f), glm::radians(angle), glm::vec3(0, 1, 0));
        // update normal matrix for teapot
        normalMatrix = glm::mat3(glm::inverseTranspose(view*model));
    }

    if (pressedKeys[GLFW_KEY_E]) {
        angle += 1.0f;
        // update model matrix for teapot
        model = glm::rotate(glm::mat4(1.0f), glm::radians(angle), glm::vec3(0, 1, 0));
        // update normal matrix for teapot
        normalMatrix = glm::mat3(glm::inverseTranspose(view*model));
    }

    if (pressedKeys[GLFW_KEY_R]) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }

    if (pressedKeys[GLFW_KEY_T]) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    }

    if (pressedKeys[GLFW_KEY_Y]) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
    }

    if (pressedKeys[GLFW_KEY_F]) {
        if (withFog) {
            fogDensity.x = 0.0f;
            glUniform3fv(fogDensityLoc, 1, glm::value_ptr(fogDensity));
            withFog = false;
        }
        else {
            fogDensity.x = 0.2f;
            glUniform3fv(fogDensityLoc, 1, glm::value_ptr(fogDensity));
            withFog = true;
        }
    }

    if (pressedKeys[GLFW_KEY_P]) {
        if (withLight) {
            lightPosOn.x = 0;
            glUniform3fv(glGetUniformLocation(myBasicShader.shaderProgram, "lightPosOn"), 1, glm::value_ptr(lightPosOn));
            withLight = false;
        }
        else {
            lightPosOn.x = 1;
            glUniform3fv(glGetUniformLocation(myBasicShader.shaderProgram, "lightPosOn"), 1, glm::value_ptr(lightPosOn));
            withLight = true;
        }
    }

    if (pressedKeys[GLFW_KEY_Z]) {
        startPres = true;
    }

    if (pressedKeys[GLFW_KEY_C]) {
        powerOn = true;
    }
    if (pressedKeys[GLFW_KEY_X]) {
        powerBoat = true;
    }
    
    if (mouseMoved) {
        view = myCamera.getViewMatrix();
        myBasicShader.useShaderProgram();
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        normalMatrix = glm::mat3(glm::inverseTranspose(view * model));
        mouseMoved = false;
    }  
}

void initOpenGLWindow() {
    myWindow.Create(1914, 991, "OpenGL Project Core");   
}

void setWindowCallbacks() {
	glfwSetWindowSizeCallback(myWindow.getWindow(), windowResizeCallback);
    glfwSetKeyCallback(myWindow.getWindow(), keyboardCallback);
    glfwSetCursorPosCallback(myWindow.getWindow(), mouseCallback);
    glfwSetInputMode(myWindow.getWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

void initOpenGLState() {
	glClearColor(0.7f, 0.7f, 0.7f, 1.0f);
	glViewport(0, 0, myWindow.getWindowDimensions().width, myWindow.getWindowDimensions().height);
    glEnable(GL_FRAMEBUFFER_SRGB);
	glEnable(GL_DEPTH_TEST); // enable depth-testing
	glDepthFunc(GL_LESS); // depth-testing interprets a smaller value as "closer"
	glEnable(GL_CULL_FACE); // cull face
	glCullFace(GL_BACK); // cull back face
	glFrontFace(GL_CCW); // GL_CCW for counter clock-wise
}

void initModels() {
    scene.LoadModel("models/scene_final/project_scene.obj");
    street_light.LoadModel("models/light/street_lamp2.obj");
    tractor.LoadModel("models/tractor_for_animation/tractor.obj");
    tractor_onRoad.LoadModel("models/tractor_on_road_for_animation/tractor_road.obj");
    boat.LoadModel("models/boat_animation/boat.obj");
    duck.LoadModel("models/duck_for_animation/duck.obj");
    gray_dog.LoadModel("models/gray_dog_animation/gray_dog.obj");
    white_dog.LoadModel("models/white_dog_animation/white_dog.obj");
    screenQuad.LoadModel("models/quad/quad.obj");  
}

void initShaders() {
	myBasicShader.loadShader(
        "shaders/basic.vert",
        "shaders/basic.frag");
    depthMapShader.loadShader("shaders/light.vert", "shaders/light.frag");
    screenQuadShader.loadShader("shaders/screenQuad.vert", "shaders/screenQuad.frag");
   
}

void initUniforms() {
	myBasicShader.useShaderProgram();

    // create model matrix 
    model = glm::rotate(glm::mat4(1.0f), glm::radians(angle), glm::vec3(0.0f, 1.0f, 0.0f));
	modelLoc = glGetUniformLocation(myBasicShader.shaderProgram, "model");

	// get view matrix for current camera
	view = myCamera.getViewMatrix();
	viewLoc = glGetUniformLocation(myBasicShader.shaderProgram, "view");
	// send view matrix to shader
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));

    // compute normal matrix for teapot
    normalMatrix = glm::mat3(glm::inverseTranspose(view*model));
	normalMatrixLoc = glGetUniformLocation(myBasicShader.shaderProgram, "normalMatrix");

	// create projection matrix
	projection = glm::perspective(glm::radians(45.0f),
                               (float)myWindow.getWindowDimensions().width / (float)myWindow.getWindowDimensions().height,
                               0.1f, 20.0f);
	projectionLoc = glGetUniformLocation(myBasicShader.shaderProgram, "projection");
	// send projection matrix to shader
	glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));	

	//set the light direction (direction towards the light)
	lightDir = glm::vec3(0.0f, 1.0f, 1.0f);
	lightDirLoc = glGetUniformLocation(myBasicShader.shaderProgram, "lightDir");
	// send light dir to shader
	glUniform3fv(lightDirLoc, 1, glm::value_ptr(lightDir));

	//set light color
	lightColor = glm::vec3(1.0f, 1.0f, 1.0f); //white light
	lightColorLoc = glGetUniformLocation(myBasicShader.shaderProgram, "lightColor");
	// send light color to shader
	glUniform3fv(lightColorLoc, 1, glm::value_ptr(lightColor));

    //create fog density
    fogDensity = glm::vec3(0.0f, 0.0f, 0.0f);
    fogDensityLoc = glGetUniformLocation(myBasicShader.shaderProgram, "fogDensity");
    glUniform3fv(fogDensityLoc, 1, glm::value_ptr(fogDensity));

    //set light position
    lightPosition = glm::vec3(6.08f, 0.60f, 4.68f);
    glUniform3fv(glGetUniformLocation(myBasicShader.shaderProgram, "lightPosition"), 1, glm::value_ptr(lightPosition));

    //set light point on
    lightPosOn = glm::vec3(0, 0, 0);
    glUniform3fv(glGetUniformLocation(myBasicShader.shaderProgram, "lightPosOn"), 1, glm::value_ptr(lightPosOn));

}

glm::mat4 computeLightSpaceTrMatrix() {
    glm::mat4 lightView = glm::lookAt(lightDir, glm::vec3(0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    const GLfloat near_plane = 0.0001f, far_plane = 500.0f;
    glm::mat4 lightProjection = glm::ortho(-1.0f, 1.0f, -1.0f, 1.0f, near_plane, far_plane);
    glm::mat4 lightSpaceTrMatrix = lightProjection * lightView;
    return lightSpaceTrMatrix;
}

void renderTeapot(gps::Shader shader) {
    // select active shader program
    shader.useShaderProgram();

    //send teapot model matrix data to shader
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));

    //send teapot normal matrix data to shader
    glUniformMatrix3fv(normalMatrixLoc, 1, GL_FALSE, glm::value_ptr(normalMatrix));

    // draw teapot
    teapot.Draw(shader);
}

void renderStreetLight(gps::Shader shader, bool depthPass) {
    shader.useShaderProgram();
    
    model = glm::mat4(1.0f);
    glUniformMatrix4fv(glGetUniformLocation(shader.shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    if (!depthPass) {
        glUniformMatrix3fv(normalMatrixLoc, 1, GL_FALSE, glm::value_ptr(normalMatrix));
    }
    street_light.Draw(shader);
}

void renderPrincipalScene(gps::Shader shader, bool depthPass) {
    shader.useShaderProgram();
    model = glm::mat4(1.0f);
    //send scene model matrix data to shader
    glUniformMatrix4fv(glGetUniformLocation(shader.shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));

    //send scene normal matrix data to shader
    if (!depthPass) {
        glUniformMatrix3fv(normalMatrixLoc, 1, GL_FALSE, glm::value_ptr(normalMatrix));
    }  
    scene.Draw(shader);
}


void renderDuck(gps::Shader shader, bool depthPass) {
    // select active shader program
    shader.useShaderProgram();
    model = glm::mat4(1.0f);
    glUniformMatrix4fv(glGetUniformLocation(shader.shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    if (!depthPass) {
        glUniformMatrix3fv(normalMatrixLoc, 1, GL_FALSE, glm::value_ptr(normalMatrix));
    }
    duck.Draw(shader);
}

void renderGrayDog(gps::Shader shader, bool depthPass) {
    // select active shader program
    shader.useShaderProgram();
    model = glm::mat4(1.0f);
    glUniformMatrix4fv(glGetUniformLocation(shader.shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    if (!depthPass) {
        glUniformMatrix3fv(normalMatrixLoc, 1, GL_FALSE, glm::value_ptr(normalMatrix));
    }
    gray_dog.Draw(shader);
}

void renderWhiteDog(gps::Shader shader, bool depthPass) {
    // select active shader program
    shader.useShaderProgram();
    model = glm::mat4(1.0f);
    glUniformMatrix4fv(glGetUniformLocation(shader.shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    if (!depthPass) {
        glUniformMatrix3fv(normalMatrixLoc, 1, GL_FALSE, glm::value_ptr(normalMatrix));
    }
    white_dog.Draw(shader);
}

float delta_tractor = 0.0f;
float delta_tractor_back = 0.0f;
float movementSpeed_tractor = 0.001;
int move_tractor = 0;
int move_tractor_back = 1300;

float updateDelta(double elapsedSeconds, float movementSpeed, float delta) {
    float newDelta;
    newDelta = delta + movementSpeed * elapsedSeconds;
    return newDelta;
}

double lastTimeStamp = glfwGetTime();

void animation_for_tractor(gps::Shader shader, bool depthPass) {
    model = glm::mat4(1.0f);
    delta_tractor += 0.001f;
    //get current time
    double currentTimeStamp = glfwGetTime();
    delta_tractor = updateDelta(currentTimeStamp - lastTimeStamp,movementSpeed_tractor,delta_tractor);
    lastTimeStamp = currentTimeStamp;
    model = glm::translate(model, glm::vec3(-delta_tractor, 0, 0));
    glUniformMatrix4fv(glGetUniformLocation(shader.shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));   
}

float lastDeltaTractor = 0;

void animation_for_tractor_back(gps::Shader shader, bool depthPass) {
    model = glm::mat4(1.0f);
    delta_tractor_back += 0.001f;
    //get current time
    double currentTimeStamp = glfwGetTime();
    delta_tractor_back = updateDelta(currentTimeStamp - lastTimeStamp, movementSpeed_tractor, delta_tractor_back);
    lastTimeStamp = currentTimeStamp;
    model = glm::translate(model, glm::vec3(-lastDeltaTractor + delta_tractor_back, 0, 0));
    glUniformMatrix4fv(glGetUniformLocation(shader.shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
}

void renderTractor(gps::Shader shader, bool depthPass) {
    // select active shader program
    shader.useShaderProgram();
    // draw tractor
    if (move_tractor < 1250) {
        animation_for_tractor(shader,depthPass);
        lastDeltaTractor = delta_tractor;
        move_tractor++;
    }
    else if(move_tractor_back > 0){
        animation_for_tractor_back(shader,depthPass);
        move_tractor_back--;
    }
    else {
        move_tractor = 0;
        move_tractor_back = 1300;
        delta_tractor = 0.0f;
        delta_tractor_back = 0.0f;
    }   
    tractor.Draw(shader);
}

float delta_tractor_onRoad = 0.0f;
float movementSpeed_tractor_onRoad = 0.02;
int move_tractor_onRoad = 0;
float lastDeltaTractor_onRoad = 0;

void animation_for_tractor_onRoad(gps::Shader shader, bool depthPass) {
    model = glm::mat4(1.0f);
    delta_tractor_onRoad += 0.001f;
    //get current time
    double currentTimeStamp = glfwGetTime();
    delta_tractor_onRoad = updateDelta(currentTimeStamp - lastTimeStamp, movementSpeed_tractor, delta_tractor_onRoad);
    lastTimeStamp = currentTimeStamp;
    model = glm::translate(model, glm::vec3(-delta_tractor_onRoad, 0, (-delta_tractor_onRoad)/2));
    glUniformMatrix4fv(glGetUniformLocation(shader.shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
}

void renderTractor_onRoad(gps::Shader shader, bool depthPass, bool powerOn) {
    // select active shader program
    shader.useShaderProgram();
    if (powerOn) {
        if (move_tractor_onRoad < 1700) {
            animation_for_tractor_onRoad(shader, depthPass);
            lastDeltaTractor_onRoad = delta_tractor_onRoad;
            move_tractor_onRoad++;
        }
        else {
            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(-lastDeltaTractor_onRoad, 0, (-delta_tractor_onRoad) / 2));
            glUniformMatrix4fv(glGetUniformLocation(shader.shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
        }
    }
    else {
        model = glm::mat4(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(shader.shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
        if (!depthPass) {
            glUniformMatrix3fv(normalMatrixLoc, 1, GL_FALSE, glm::value_ptr(normalMatrix));
        }
    }
    // draw tractor
    tractor_onRoad.Draw(shader);
}

float delta_boat = 0.0f;

void animation_for_boat(gps::Shader shader, bool depthPass) {
    model = glm::mat4(1.0f);
    delta_boat += 0.001f;
    //get current time
    double currentTimeStamp = glfwGetTime();
    delta_boat = updateDelta(currentTimeStamp - lastTimeStamp, movementSpeed_tractor, delta_boat);
    lastTimeStamp = currentTimeStamp;
    model = glm::translate(model, glm::vec3(delta_boat, 0, 0));
    glUniformMatrix4fv(glGetUniformLocation(shader.shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
}

int move_boat = 0;
float lastDeltaBoat = 0.0f;

void renderBoat(gps::Shader shader, bool depthPass) {
    // select active shader program
    shader.useShaderProgram();
    if(powerBoat) {
        if (move_boat < 450) {
            animation_for_boat(shader, depthPass);
            lastDeltaBoat = delta_boat;
            move_boat++;
        }
        else {
            model = glm::mat4(1.0f);
            model = glm::translate(model, glm::vec3(lastDeltaBoat, 0, 0));
            glUniformMatrix4fv(glGetUniformLocation(shader.shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
        }
    }
    else {
        model = glm::mat4(1.0f);
        glUniformMatrix4fv(glGetUniformLocation(shader.shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
        if (!depthPass) {
            glUniformMatrix3fv(normalMatrixLoc, 1, GL_FALSE, glm::value_ptr(normalMatrix));
        }
    }
    boat.Draw(shader);
}

//for shadow we make a draw Objects function where I put the conent from renderScene function

void drawObjects(gps::Shader shader, bool depthPass) {
    renderPrincipalScene(shader, depthPass);
    renderStreetLight(shader, depthPass);
    renderTractor(shader, depthPass);
    renderTractor_onRoad(shader, depthPass,powerOn);
    renderBoat(shader, depthPass);
    renderDuck(shader, depthPass);
    renderGrayDog(shader, depthPass);
    renderWhiteDog(shader, depthPass);
}

//new renderScene function, for the shadow

void renderScene() {
    depthMapShader.useShaderProgram();
    glUniformMatrix4fv(glGetUniformLocation(depthMapShader.shaderProgram, "lightSpaceTrMatrix"),
        1,
        GL_FALSE,
        glm::value_ptr(computeLightSpaceTrMatrix()));

    glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
    glClear(GL_DEPTH_BUFFER_BIT);

    //render the scene
    drawObjects(depthMapShader, true);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (showDepthMap) {
        glViewport(0, 0, myWindow.getWindowDimensions().width, myWindow.getWindowDimensions().height);

        glClear(GL_COLOR_BUFFER_BIT);

        screenQuadShader.useShaderProgram();

        //bind the depth map
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, depthMapTexture);
        glUniform1i(glGetUniformLocation(screenQuadShader.shaderProgram, "depthMap"), 0);

        glDisable(GL_DEPTH_TEST);
        screenQuad.Draw(screenQuadShader);
        glEnable(GL_DEPTH_TEST);
    }
    else {
        //render the scene with shadows
        glViewport(0, 0, myWindow.getWindowDimensions().width, myWindow.getWindowDimensions().height);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        myBasicShader.useShaderProgram();

        view = myCamera.getViewMatrix();
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));

        //bind the shadow map
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, depthMapTexture);
        glUniform1i(glGetUniformLocation(myBasicShader.shaderProgram, "shadowMap"), 3);

        glUniformMatrix4fv(glGetUniformLocation(myBasicShader.shaderProgram, "lightSpaceTrMatrix"),
            1,
            GL_FALSE,
            glm::value_ptr(computeLightSpaceTrMatrix()));

        drawObjects(myBasicShader, false);
    }
    mySkyBox.Draw(skyboxShader, view, projection);
}

void cleanup() {
    myWindow.Delete();
    //cleanup code for your own data
}

float go_z = 0;
float go_x = 0;
float rotate = 90.0f;
float rotate_back = 0.0f;
float rotate_2 = 90.0f;
float go_z2 = 0;
float rotate_3 = 250.0f;
float go_x2 = 0;

int main(int argc, const char * argv[]) {

    try {
        initOpenGLWindow();
    } catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    std::vector<const GLchar*> faces;
    faces.push_back("skybox/hills_rt.tga");
    faces.push_back("skybox/hills_lf.tga");
    faces.push_back("skybox/hills_up.tga");
    faces.push_back("skybox/hills_dn.tga");
    faces.push_back("skybox/hills_bk.tga");
    faces.push_back("skybox/hills_ft.tga");

    initOpenGLState();
	initModels();
	initShaders();
	initUniforms();
    setWindowCallbacks();


	glCheckError();

    mySkyBox.Load(faces);
    skyboxShader.loadShader("shaders/skyboxShader.vert", "shaders/skyboxShader.frag");
    skyboxShader.useShaderProgram();

    view = myCamera.getViewMatrix();
    glUniformMatrix4fv(glGetUniformLocation(skyboxShader.shaderProgram, "view"), 1, GL_FALSE,
        glm::value_ptr(view));

    projection = glm::perspective(glm::radians(45.0f), (float)myWindow.getWindowDimensions().width / (float)myWindow.getWindowDimensions().height, 0.1f, 1000.0f);
    glUniformMatrix4fv(glGetUniformLocation(skyboxShader.shaderProgram, "projection"), 1, GL_FALSE,
        glm::value_ptr(projection));
	// application loop
	while (!glfwWindowShouldClose(myWindow.getWindow())) {
        processMovement();

        if (startPres == true && go_z<=5.0) {
            //startPresentation();
            myCamera.changePosition(glm::vec3(0.0f, 0.0f, go_z), 0.001f);
            go_z += 0.01;
            //startPres = false;
        }
        if (startPres == true && go_z > 5.00 && rotate > 0.0f) {
            myCamera.rotate(0, rotate);
            rotate -= 1.0f;
        }
        if (startPres == true && go_z > 5.00 && rotate < 1.0f && rotate_back <= 90.0f) {
            myCamera.rotate(0, rotate_back);
            rotate_back += 1.0f;
        }
        if (startPres == true && go_z > 5.0 && go_x <= 6.0 && rotate < 1.0f && rotate_back > 90.0f) {
            myCamera.changePosition(glm::vec3(-go_x, 0.0f, -1.0f), 0.001f);
            go_x += 0.01;
        }
        if (startPres == true && go_z > 5.0 && go_x > 6.0 && rotate < 1.0f && rotate_back > 90.0f && rotate_2 <= 250.0f) {
            myCamera.rotate(0,rotate_2);
            rotate_2 += 1.0f;
        }
        if (startPres == true && go_z > 5.0 && go_x > 6.0 && rotate < 1.0f && rotate_back > 90.0f && rotate_2 > 250.0f && go_z2 <= 9.0) {
            myCamera.changePosition(glm::vec3(-1.0f, 0.0f, -go_z2), 0.001f);
            go_z2 += 0.01f;
        }
        if (startPres == true && go_z > 5.0 && go_x > 6.0 && rotate < 1.0f && rotate_back > 90.0f && rotate_2 > 250.0f && go_z2 > 9.0 && rotate_3 > 200.0f) {
            myCamera.rotate(0, rotate_3);
            rotate_3 -= 1.0f;
        }
        if (startPres == true && go_z > 5.0 && go_x > 6.0 && rotate < 1.0f && rotate_back > 90.0f && rotate_2 > 250.0f && go_z2 > 9.0 && rotate_3 < 201.0f && go_x2 <= 5.0f) {
            myCamera.changePosition(glm::vec3(-go_x2, 0.0f, -1.0f), 0.001f);
            go_x2 += 0.01f;
        }
	    renderScene();

		glfwPollEvents();
		glfwSwapBuffers(myWindow.getWindow());

		glCheckError();
	}

	cleanup();

    return EXIT_SUCCESS;
}
