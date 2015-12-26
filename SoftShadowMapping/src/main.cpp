//References:
//Shadow Mapping - L. Willians. Casting Curved Shadows on Curved Surfaces. 1978.
//Summed-Area Tables. F. Crow. Summed-Area Tables for Texture Mapping. 1980.
//Monte-Carlo Soft Shadow Volumes/Mapping - L. Brotman and N. Badler. Generating Soft Shadows with a Depth Buffer Algorithm. 1984.
//Accumulation Buffer - P. Haeberli and K. Akeley. The Accumulation Buffer: Hardware Support for High-Quality Rendering. 1990.
//G-Buffer - T. Saito and T. Takahashi. Comprehensible Rendering of 3-D Shapes. 1990.
//Percentage-Closer Soft Shadows - R. Fernando. Percentage-Closer Soft Shadows. 2005.
//Summed-Area Tables in GPU - J. Hensley et al. Fast Summed-Area Table Generation and Its Applications. 2005.
//Hierarchical Shadow Map - G. Guennebaud et al. Real-Time Soft Shadow Mapping by Backprojection. 2006.
//Summed-Area Variance Shadow Mapping - A. Lauritzen. Summed-Area Variance Shadow Maps. 2007.
//Variance Soft Shadow Mapping - B. Yang et al. Variance Soft Shadow Mapping. 2010.
//Exponential Soft Shadow Mapping - L. Shen et al. Exponential Soft Shadow Mapping. 2013.
//Moment Soft Shadow Mapping - C. Peters et al. Beyond Hard Shadows: Moment Shadow Maps for Single Scattering, Soft Shadows and Translucent Occluders. 2016.
//http://rastergrid.com/blog/2010/09/efficient-gaussian-blur-with-linear-sampling/ for gaussian mask weights
//http://www.3drender.com/challenges/ (.obj, .mtl)
//http://graphics.cs.williams.edu/data/meshes.xml (.obj, .mtl)

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <GL/glew.h>
#include <GL/glut.h>
#include "Viewers\MyGLTextureViewer.h"
#include "Viewers\MyGLGeometryViewer.h"
#include "Viewers\shader.h"
#include "Viewers\ShadowParams.h"
#include "IO\SceneLoader.h"
#include "Scene\Mesh.h"
#include "Scene\LightSource.h"
#include "Image.h"
#include "Filter.h"

enum 
{
	SHADOW_MAP_DEPTH = 0,
	SHADOW_MAP_COLOR = 1,
	TEMP_SHADOW_MAP_DEPTH = 2,
	TEMP_SHADOW_MAP_COLOR = 3,
	SAT_SHADOW_MAP_DEPTH = 4,
	SAT_SHADOW_MAP_COLOR = 5,
	HIERARCHICAL_SHADOW_MAP_DEPTH = 6,
	HIERARCHICAL_SHADOW_MAP_COLOR = 7,
	ACCUMULATION_MAP_DEPTH = 8,
	ACCUMULATION_MAP_COLOR = 9,
	TEMP_ACCUMULATION_MAP_DEPTH = 10,
	TEMP_ACCUMULATION_MAP_COLOR = 11,
	GBUFFER_MAP_DEPTH = 12,
	VERTEX_MAP_COLOR = 13,
	NORMAL_MAP_COLOR = 14,
};

enum
{
	SCENE_SHADER = 0,
	SHADOW_SHADER = 1,
	CLEAR_SHADER = 2,
	COPY_SHADER = 3,
	MONTE_CARLO_RENDER_SHADER = 4,
	GBUFFER_SHADER = 5,
	SOFT_SHADOW_SHADER = 6,
	MOMENT_SHADER = 7,
	EXPONENTIAL_SHADER = 8,
	SAT_HORIZONTAL_PASS_SHADER = 9,
	SAT_VERTICAL_PASS_SHADER = 10,
	PREPARE_MIN_MAX_SHADER = 11,
	MIN_MAX_SHADER = 12
};

enum
{
	SHADOW_FRAMEBUFFER = 0,
	TEMP_SHADOW_FRAMEBUFFER = 1,
	SAT_SHADOW_FRAMEBUFFER = 2,
	HIERARCHICAL_SHADOW_FRAMEBUFFER = 3,
	ACCUMULATION_FRAMEBUFFER = 4,
	TEMP_ACCUMULATION_FRAMEBUFFER = 5,
	GBUFFER_FRAMEBUFFER = 6
};

bool temp = false;
//Window size
int windowWidth = 1280;
int windowHeight = 720;

int shadowMapWidth = 512 * 2;
int shadowMapHeight = 512 * 2;

//  The number of frames
int frameCount = 0;
float fps = 0;
int currentTime = 0, previousTime = 0;
char fileName[1000];

MyGLTextureViewer myGLTextureViewer;
MyGLGeometryViewer myGLGeometryViewer;
ShadowParams shadowParams;

Mesh *scene;
SceneLoader *sceneLoader;
LightSource *lightSource;

GLuint textures[20];
GLuint frameBuffer[10];
GLuint sceneVBO[5];
GLuint sceneTextures[4];

glm::vec3 cameraEye;
glm::vec3 cameraAt;
glm::vec3 cameraUp;
glm::mat4 lightMVP;	
glm::mat4 lightMV;
glm::mat4 lightP;

GLuint ProgramObject = 0;
GLuint VertexShaderObject = 0;
GLuint FragmentShaderObject = 0;
GLuint shaderVS, shaderFS, shaderProg[15];   // handles to objects
GLint  linked;

float translationVector[3] = {0.0, 0.0, 0.0};
float lightTranslationVector[3] = {0.0, 0.0, 0.0};
float rotationAngles[3] = {0.0, 0.0, 0.0};

bool translationOn = false;
bool lightTranslationOn = false;
bool rotationOn = false;
bool animationOn = false;
bool cameraOn = false;
bool shadowIntensityOn = false;
bool changeKernelSizeOn = false;
bool changeBlockerSearchSizeOn = false;
bool stop = false;

int vel = 1;
float animation = -1800;
int pointLightSample = -1;

void calculateFPS()
{

	frameCount++;
	currentTime = glutGet(GLUT_ELAPSED_TIME);

    int timeInterval = currentTime - previousTime;

    if(timeInterval > 1000)
    {
        fps = frameCount / (timeInterval / 1000.0f);
        previousTime = currentTime;
        frameCount = 0;
	
		printf("FPS: %f\n", fps);
	}

}

void reshape(int w, int h)
{
	
	windowWidth = w;
	windowHeight = h;

}

void displayScene()
{

	glm::mat4 projection = myGLGeometryViewer.getProjectionMatrix();
	glm::mat4 view = myGLGeometryViewer.getViewMatrix();
	glm::mat4 model = myGLGeometryViewer.getModelMatrix();
	
	model *= glm::translate(glm::vec3(translationVector[0], translationVector[1], translationVector[2]));
	model *= glm::rotate(rotationAngles[0], glm::vec3(1, 0, 0));
	model *= glm::rotate(rotationAngles[1], glm::vec3(0, 1, 0));
	model *= glm::rotate(rotationAngles[2], glm::vec3(0, 0, 1));

	myGLGeometryViewer.setProjectionMatrix(projection);
	myGLGeometryViewer.setViewMatrix(view);
	myGLGeometryViewer.setModelMatrix(model);
	myGLGeometryViewer.configurePhong(lightSource->getEye(pointLightSample), cameraEye);
	
	//glColor3f(0.0, 1.0, 0.0);
	myGLGeometryViewer.loadVBOs(sceneVBO, scene);
	myGLGeometryViewer.drawMesh(sceneVBO, scene->getIndicesSize(), scene->getTextureCoordsSize(), scene->getColorsSize(), scene->textureFromImage(), sceneTextures, 
		scene->getNumberOfTextures());

}

void updateLight()
{

	lightSource->setEye(glm::vec3(sceneLoader->getLightPosition()[0] + lightTranslationVector[0], sceneLoader->getLightPosition()[1] + lightTranslationVector[1], 
		sceneLoader->getLightPosition()[2] + lightTranslationVector[2]));

	if(animationOn)
		lightSource->setEye(glm::mat3(glm::rotate((float)animation/10, glm::vec3(0, 1, 0))) * lightSource->getEye(pointLightSample));

}

void displaySceneFromLightPOV()
{

	glViewport(0, 0, shadowMapWidth, shadowMapHeight);
	
	updateLight();

	if(shadowParams.SAVSM || shadowParams.VSSM || shadowParams.MSSM) {
		glUseProgram(shaderProg[MOMENT_SHADER]);
		myGLGeometryViewer.setShaderProg(shaderProg[MOMENT_SHADER]);
		myGLGeometryViewer.configureMoments(shadowParams);
		myGLGeometryViewer.configureLinearization();
	} else if(shadowParams.ESSM) {
		glUseProgram(shaderProg[EXPONENTIAL_SHADER]);
		myGLGeometryViewer.setShaderProg(shaderProg[EXPONENTIAL_SHADER]);
		myGLGeometryViewer.configureLinearization();
	} else {
		glUseProgram(shaderProg[SCENE_SHADER]);
		myGLGeometryViewer.setShaderProg(shaderProg[SCENE_SHADER]);
	}

	glPolygonOffset(4.0f, 20.0f);
	glEnable(GL_POLYGON_OFFSET_FILL);

	myGLGeometryViewer.setEye(lightSource->getEye(pointLightSample));
	myGLGeometryViewer.setLook(lightSource->getAt(pointLightSample));
	myGLGeometryViewer.setUp(lightSource->getUp());
	myGLGeometryViewer.configureAmbient(shadowMapWidth, shadowMapHeight);
	myGLGeometryViewer.setIsCameraViewpoint(false);

	glm::mat4 projection = myGLGeometryViewer.getProjectionMatrix();
	glm::mat4 view = myGLGeometryViewer.getViewMatrix();
	glm::mat4 model = myGLGeometryViewer.getModelMatrix();
	
	model *= glm::translate(glm::vec3(translationVector[0], translationVector[1], translationVector[2]));
	model *= glm::rotate(rotationAngles[0], glm::vec3(1, 0, 0));
	model *= glm::rotate(rotationAngles[1], glm::vec3(0, 1, 0));
	model *= glm::rotate(rotationAngles[2], glm::vec3(0, 0, 1));

	lightMVP = projection * view * model;
	lightMV = view * model;
	lightP = projection;

	displayScene();
	glUseProgram(0);

}

void displaySceneFromCameraPOV(GLuint shader)
{

	glViewport(0, 0, windowWidth, windowHeight);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	updateLight();
	lightSource->setEye(glm::mat3(glm::rotate((float)180.0, glm::vec3(0, 1, 0))) * lightSource->getEye(pointLightSample));

	glUseProgram(shader);
	myGLGeometryViewer.setShaderProg(shader);
	shadowParams.shadowMap = textures[SHADOW_MAP_DEPTH];
	if(shadowParams.SAVSM || shadowParams.VSSM || shadowParams.ESSM || shadowParams.MSSM) {
		if(shadowParams.SAT)
			shadowParams.SATShadowMap = textures[SAT_SHADOW_MAP_COLOR];
		else
			shadowParams.SATShadowMap = textures[SHADOW_MAP_COLOR];
		if(shadowParams.VSSM)
			shadowParams.hierarchicalShadowMap = textures[HIERARCHICAL_SHADOW_MAP_COLOR];
	} else if(shadowParams.monteCarlo) 
		shadowParams.accumulationMap = textures[ACCUMULATION_MAP_COLOR];
	
	shadowParams.lightMVP = lightMVP;

	myGLGeometryViewer.setEye(cameraEye);
	myGLGeometryViewer.setLook(cameraAt);
	myGLGeometryViewer.setUp(cameraUp);
	myGLGeometryViewer.configureAmbient(windowWidth, windowHeight);
	myGLGeometryViewer.configureShadow(shadowParams);
	myGLGeometryViewer.setIsCameraViewpoint(true);

	displayScene();
	glUseProgram(0);

}

void displaySceneFromGBuffer()
{

	glViewport(0, 0, windowWidth, windowHeight);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	updateLight();
	lightSource->setEye(glm::mat3(glm::rotate((float)180.0, glm::vec3(0, 1, 0))) * lightSource->getEye(pointLightSample));

	glUseProgram(shaderProg[SHADOW_SHADER]);
	myGLGeometryViewer.setShaderProg(shaderProg[SHADOW_SHADER]);
	
	shadowParams.shadowMap = textures[SHADOW_MAP_DEPTH];
	shadowParams.accumulationMap = textures[ACCUMULATION_MAP_COLOR];
	shadowParams.vertexMap = textures[VERTEX_MAP_COLOR];
	shadowParams.normalMap = textures[NORMAL_MAP_COLOR];
	shadowParams.lightMVP = lightMVP;
	
	myGLGeometryViewer.setEye(cameraEye);
	myGLGeometryViewer.setLook(cameraAt);
	myGLGeometryViewer.setUp(cameraUp);
	myGLGeometryViewer.configureAmbient(windowWidth, windowHeight);
	myGLGeometryViewer.configureShadow(shadowParams);
	myGLGeometryViewer.setIsCameraViewpoint(true);
	
	glm::mat4 model = myGLGeometryViewer.getModelMatrix();
	
	model *= glm::translate(glm::vec3(translationVector[0], translationVector[1], translationVector[2]));
	model *= glm::rotate(rotationAngles[0], glm::vec3(1, 0, 0));
	model *= glm::rotate(rotationAngles[1], glm::vec3(0, 1, 0));
	model *= glm::rotate(rotationAngles[2], glm::vec3(0, 0, 1));

	myGLGeometryViewer.setModelMatrix(model);
	myGLGeometryViewer.configurePhong(lightSource->getEye(pointLightSample), cameraEye);

	myGLTextureViewer.drawTextureQuad();
	
	glUseProgram(0);

}

void renderMonteCarlo()
{

	myGLTextureViewer.setShaderProg(shaderProg[CLEAR_SHADER]);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0);
	glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer[ACCUMULATION_FRAMEBUFFER]);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glViewport(0, 0, windowWidth, windowHeight);
	myGLTextureViewer.drawTextureOnShader(textures[SHADOW_MAP_COLOR], windowWidth, windowHeight);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	
	pointLightSample = -1;

	glClearColor(0.63f, 0.82f, 0.96f, 1.0);
	glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer[GBUFFER_FRAMEBUFFER]);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	displaySceneFromCameraPOV(shaderProg[GBUFFER_SHADER]);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	
	pointLightSample = 0;

	while(pointLightSample < lightSource->getNumberOfPointLights()) {

		glClearColor(1.0f, 1.0f, 1.0f, 1.0);
		glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer[SHADOW_FRAMEBUFFER]);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		displaySceneFromLightPOV();
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	
		glDisable(GL_CULL_FACE);
		glDisable(GL_POLYGON_OFFSET_FILL);
	
		glClearColor(0.63f, 0.82f, 0.96f, 1.0);
		glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer[TEMP_ACCUMULATION_FRAMEBUFFER]);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		displaySceneFromGBuffer();
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		
		pointLightSample++;

		myGLTextureViewer.setShaderProg(shaderProg[COPY_SHADER]);
		glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer[ACCUMULATION_FRAMEBUFFER]);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glViewport(0, 0, windowWidth, windowHeight);
		myGLTextureViewer.drawTextureOnShader(textures[TEMP_ACCUMULATION_MAP_COLOR], windowWidth, windowHeight);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		
	}
	
	pointLightSample = -1;
	
	displaySceneFromCameraPOV(shaderProg[MONTE_CARLO_RENDER_SHADER]);
	
}

void renderSoftShadows() 
{

	glClearColor(0.0f, 0.0f, 0.0f, 0.0);
	glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer[SHADOW_FRAMEBUFFER]);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	displaySceneFromLightPOV();
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	
	glBindTexture(GL_TEXTURE_2D, textures[SHADOW_MAP_COLOR]);
	glGenerateMipmap(GL_TEXTURE_2D);

	if(shadowParams.SAT) {
	
		int m = std::logf(shadowMapWidth)/std::logf(2);
		for(int iteration = 0; iteration < m; iteration++) {

			glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer[TEMP_SHADOW_FRAMEBUFFER]);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glViewport(0, 0, shadowMapWidth, shadowMapHeight);
			myGLTextureViewer.setShaderProg(shaderProg[SAT_HORIZONTAL_PASS_SHADER]);
			glUseProgram(shaderProg[SAT_HORIZONTAL_PASS_SHADER]);
			glUniform1i(glGetUniformLocation(shaderProg[SAT_HORIZONTAL_PASS_SHADER], "iteration"), iteration);
			if(iteration == 0)
				myGLTextureViewer.drawTextureOnShader(textures[SHADOW_MAP_COLOR], shadowMapWidth, shadowMapHeight);
			else
				myGLTextureViewer.drawTextureOnShader(textures[SAT_SHADOW_MAP_COLOR], shadowMapWidth, shadowMapHeight);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer[SAT_SHADOW_FRAMEBUFFER]);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glViewport(0, 0, shadowMapWidth, shadowMapHeight);
			myGLTextureViewer.setShaderProg(shaderProg[SAT_VERTICAL_PASS_SHADER]);
			glUseProgram(shaderProg[SAT_VERTICAL_PASS_SHADER]);
			glUniform1i(glGetUniformLocation(shaderProg[SAT_VERTICAL_PASS_SHADER], "iteration"), iteration);
			myGLTextureViewer.drawTextureOnShader(textures[TEMP_SHADOW_MAP_COLOR], shadowMapWidth, shadowMapHeight);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			
		}
		
	}

	if(shadowParams.VSSM) {
		
		glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer[HIERARCHICAL_SHADOW_FRAMEBUFFER]);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, textures[HIERARCHICAL_SHADOW_MAP_DEPTH], 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[HIERARCHICAL_SHADOW_MAP_COLOR], 0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		
		myGLTextureViewer.setShaderProg(shaderProg[PREPARE_MIN_MAX_SHADER]);
		glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer[HIERARCHICAL_SHADOW_FRAMEBUFFER]);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glViewport(0, 0, shadowMapWidth, shadowMapHeight);
		myGLTextureViewer.drawTextureOnShader(textures[SHADOW_MAP_COLOR], shadowMapWidth, shadowMapHeight);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

		int m = std::logf(shadowMapWidth)/std::logf(2);
		int factor;

		for(int iteration = 1; iteration < m; iteration++) {
		
			factor = powf(2.0, iteration);
		
			glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer[TEMP_SHADOW_FRAMEBUFFER]);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, textures[TEMP_SHADOW_MAP_DEPTH], 0);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[TEMP_SHADOW_MAP_COLOR], iteration);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		
			myGLTextureViewer.setShaderProg(shaderProg[MIN_MAX_SHADER]);
			glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer[TEMP_SHADOW_FRAMEBUFFER]);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glViewport(0, 0, shadowMapWidth/factor, shadowMapHeight/factor);
			glUseProgram(shaderProg[MIN_MAX_SHADER]);
			glUniform1i(glGetUniformLocation(shaderProg[MIN_MAX_SHADER], "iteration"), iteration);
			myGLTextureViewer.drawTextureOnShader(textures[HIERARCHICAL_SHADOW_MAP_COLOR], shadowMapWidth/factor, shadowMapHeight/factor);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);

			glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer[HIERARCHICAL_SHADOW_FRAMEBUFFER]);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, textures[HIERARCHICAL_SHADOW_MAP_DEPTH], 0);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[HIERARCHICAL_SHADOW_MAP_COLOR], iteration);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		
			myGLTextureViewer.setShaderProg(shaderProg[COPY_SHADER]);
			glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer[HIERARCHICAL_SHADOW_FRAMEBUFFER]);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
			glViewport(0, 0, shadowMapWidth/factor, shadowMapHeight/factor);
			myGLTextureViewer.drawTextureOnShader(textures[TEMP_SHADOW_MAP_COLOR], shadowMapWidth/factor, shadowMapHeight/factor);
			glBindFramebuffer(GL_FRAMEBUFFER, 0);

		}
		
		glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer[TEMP_SHADOW_FRAMEBUFFER]);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, textures[TEMP_SHADOW_MAP_DEPTH], 0);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[TEMP_SHADOW_MAP_COLOR], 0);
		glBindFramebuffer(GL_FRAMEBUFFER, 0);

	}
	
	glDisable(GL_CULL_FACE);
	glDisable(GL_POLYGON_OFFSET_FILL);
	
	glClearColor(0.63f, 0.82f, 0.96f, 1.0);
	displaySceneFromCameraPOV(shaderProg[SOFT_SHADOW_SHADER]);

}

void display()
{
	
	if(shadowParams.monteCarlo)
		renderMonteCarlo();
	else
		renderSoftShadows();

	glutSwapBuffers();
	glutPostRedisplay();

}

void idle()
{
	
	calculateFPS();

	if(animationOn) {
		if(!stop)
			animation += 6;
		if(animation == 1800)
			animation = -1800;
	}

}	

void keyboard(unsigned char key, int x, int y) 
{

	switch(key) {
	case 27:
		exit(0);
		break;
	case 's':
		shadowParams.SAT = !shadowParams.SAT;
		break;
	}

}

void specialKeyboard(int key, int x, int y)
{

	float temp;
	switch(key) {
	case GLUT_KEY_UP:
		if(cameraOn) {
			if(translationOn) {
				cameraEye[1] += vel;
				cameraAt[1] += vel;
			}
			if(rotationOn) {
				temp = cameraAt[2];
				cameraAt = glm::mat3(glm::rotate((float)5 * vel, glm::vec3(1.0, 0.0, 0.0))) * cameraAt;
				cameraAt[2] = temp;
				cameraUp = glm::mat3(glm::rotate((float)5 * vel, glm::vec3(1.0, 0.0, 0.0))) * glm::vec3(0.0, 1.0, 0.0);
			}
		} else {
			if(translationOn)
				translationVector[1] += vel;
			if(rotationOn)
				rotationAngles[1] += 5 * vel;
		}
		if(lightTranslationOn)
			lightTranslationVector[1] += 5 * vel;
		if(shadowIntensityOn)
			shadowParams.shadowIntensity += 0.05;
		if(changeKernelSizeOn)
			shadowParams.kernelSize++;
		if(changeBlockerSearchSizeOn)
			shadowParams.blockerSearchSize++;
		break;
	case GLUT_KEY_DOWN:
		if(cameraOn) {
			if(translationOn) {
				cameraEye[1] -= vel;
				cameraAt[1] -= vel;
			}
			if(rotationOn) {
				temp = cameraAt[2];
				cameraAt = glm::mat3(glm::rotate((float)-5 * vel, glm::vec3(1.0, 0.0, 0.0))) * cameraAt;
				cameraAt[2] = temp;
				cameraUp = glm::mat3(glm::rotate((float)-5 * vel, glm::vec3(1.0, 0.0, 0.0))) * glm::vec3(0.0, 1.0, 0.0);
			}
		} else {
			if(translationOn)
				translationVector[1] -= vel;
			if(rotationOn)
				rotationAngles[1] -= 5 * vel;
		}
		if(lightTranslationOn)
			lightTranslationVector[1] -= 5 * vel;
		if(shadowIntensityOn)
			shadowParams.shadowIntensity -= 0.05;
		if(changeKernelSizeOn)
			shadowParams.kernelSize--;
		if(changeBlockerSearchSizeOn)
			shadowParams.blockerSearchSize--;
		break;
	case GLUT_KEY_LEFT:
		if(cameraOn) {
			if(translationOn) {
				cameraEye[0] -= vel;
				cameraAt[0] -= vel;
			}
			if(rotationOn) {
				temp = cameraAt[2];
				cameraAt = glm::mat3(glm::rotate((float)-5 * vel, glm::vec3(0.0, 1.0, 0.0))) * cameraAt;
				cameraAt[2] = temp;
				cameraUp = glm::mat3(glm::rotate((float)-5 * vel, glm::vec3(0.0, 1.0, 0.0))) * glm::vec3(0.0, 1.0, 0.0);
			}
		} else {
			if(translationOn)
				translationVector[0] -= vel;
			if(rotationOn)
				rotationAngles[0] -= 5 * vel;
		}
		if(lightTranslationOn)
			lightTranslationVector[0] -= 5 * vel;
		break;
	case GLUT_KEY_RIGHT:
		if(cameraOn) {
			if(translationOn) {
				cameraEye[0] += vel;
				cameraAt[0] += vel;
			}
			if(rotationOn) {
				temp = cameraAt[2];
				cameraAt = glm::mat3(glm::rotate((float)5 * vel, glm::vec3(0.0, 1.0, 0.0))) * cameraAt;
				cameraAt[2] = temp;
				cameraUp = glm::mat3(glm::rotate((float)5 * vel, glm::vec3(0.0, 1.0, 0.0))) * glm::vec3(0.0, 1.0, 0.0);
			}
		} else {
			if(translationOn)
				translationVector[0] += vel;
			if(rotationOn)
				rotationAngles[0] += 5 * vel;
		}
		if(lightTranslationOn)
			lightTranslationVector[0] += 5 * vel;
		break;
	case GLUT_KEY_PAGE_UP:
		if(cameraOn) {
			if(translationOn) {
				cameraEye[2] += vel;
				cameraAt[2] += vel;
			}
			if(rotationOn) {
				cameraAt = glm::mat3(glm::rotate((float)5 * vel, glm::vec3(0.0, 0.0, 1.0))) * cameraAt;
				cameraUp = glm::mat3(glm::rotate((float)5 * vel, glm::vec3(0.0, 0.0, 1.0))) * glm::vec3(0.0, 1.0, 0.0);
			}
		} else {
			if(translationOn)
				translationVector[2] += vel;
			if(rotationOn)
				rotationAngles[2] += 5 * vel;
		}
		if(lightTranslationOn)
			lightTranslationVector[2] += 5 * vel;
		break;
	case GLUT_KEY_PAGE_DOWN:
		if(cameraOn) {
			if(translationOn) {
				cameraEye[2] -= vel;
				cameraAt[2] -= vel;
			}
			if(rotationOn) {
				cameraAt = glm::mat3(glm::rotate((float)-5 * vel, glm::vec3(0.0, 0.0, 1.0))) * cameraAt;
				cameraUp = glm::mat3(glm::rotate((float)-5 * vel, glm::vec3(0.0, 0.0, 1.0))) * glm::vec3(0.0, 1.0, 0.0);
			}
		} else {
			if(translationOn)
				translationVector[2] -= vel;
			if(rotationOn)
				rotationAngles[2] -= 5 * vel;
		}
		if(lightTranslationOn)
			lightTranslationVector[2] -= 5 * vel;
		break;
	}

}

void resetShadowParameters() {
	
	shadowParams.monteCarlo = false;
	shadowParams.PCSS = false;
	shadowParams.SAVSM = false;
	shadowParams.VSSM = false;
	shadowParams.ESSM = false;
	shadowParams.MSSM = false;

}

void transformationMenu(int id) {

	switch(id)
	{
		case 0:
			translationOn = true;
			rotationOn = false;
			break;
		case 1:
			rotationOn = true;
			translationOn = false;
			break;
		case 2:
			lightTranslationOn = !lightTranslationOn;
			translationOn = false;
			break;
		case 3:
			cameraOn = !cameraOn;
			break;
	}

}

void otherFunctionsMenu(int id) {

	switch(id)
	{
		case 0:
				
			if(animationOn)
				stop = true;
			else
				animationOn = !animationOn;
			/*
			animationOn = !animationOn;
			animation = 0;
			*/
			break;
		case 1:
			shadowIntensityOn = !shadowIntensityOn;
			break;
		case 2:
			printf("LightPosition: %f %f %f\n", lightSource->getEye(pointLightSample)[0], lightSource->getEye(pointLightSample)[1], lightSource->getEye(pointLightSample)[2]);
			printf("LightAt: %f %f %f\n", lightSource->getAt(pointLightSample)[0], lightSource->getAt(pointLightSample)[1], lightSource->getAt(pointLightSample)[2]);
			printf("CameraPosition: %f %f %f\n", cameraEye[0], cameraEye[1], cameraEye[2]);
			printf("CameraAt: %f %f %f\n", cameraAt[0], cameraAt[1], cameraAt[2]);
			printf("Global Translation: %f %f %f\n", translationVector[0], translationVector[1], translationVector[2]);
			printf("Global Rotation: %f %f %f\n", rotationAngles[0], rotationAngles[1], rotationAngles[2]);
			break;
	}

}

void softShadowMenu(int id) {

	switch(id) {
	case 0:
		resetShadowParameters();
		shadowParams.monteCarlo = true;
		break;
	case 1:
		resetShadowParameters();
		shadowParams.PCSS = true;
		shadowParams.SAT = false;
		break;
	case 2:
		resetShadowParameters();
		shadowParams.SAVSM = true;
		break;
	case 3:
		resetShadowParameters();
		shadowParams.VSSM = true;
		break;
	case 4:
		resetShadowParameters();
		shadowParams.ESSM = true;
		break;
	case 5:
		resetShadowParameters();
		shadowParams.MSSM = true;
		break;
	}
}

void softShadowParametersMenu(int id) {

	switch(id) {
	case 0:
		changeKernelSizeOn = !changeKernelSizeOn;
		break;
	case 1:
		changeBlockerSearchSizeOn = !changeBlockerSearchSizeOn;
		break;
	}

}

void mainMenu(int id) {

	

}

void createMenu() {

	GLint softShadowMenuID, transformationMenuID, softShadowParametersMenuID, otherFunctionsMenuID;

	softShadowMenuID = glutCreateMenu(softShadowMenu);
		glutAddMenuEntry("Monte-Carlo Sampling", 0);
		glutAddMenuEntry("Percentage-Closer Soft Shadow Mapping", 1);
		glutAddMenuEntry("Summed-Area Variance Shadow Mapping", 2);
		glutAddMenuEntry("Variance Soft Shadow Mapping", 3);
		glutAddMenuEntry("Exponential Soft Shadow Mapping", 4);
		glutAddMenuEntry("Moment Soft Shadow Mapping", 5);

	transformationMenuID = glutCreateMenu(transformationMenu);
		glutAddMenuEntry("Translation", 0);
		glutAddMenuEntry("Rotation", 1);
		glutAddMenuEntry("Light Translation [On/Off]", 2);
		glutAddMenuEntry("Camera Movement [On/Off]", 3);

	softShadowParametersMenuID = glutCreateMenu(softShadowParametersMenu);
		glutAddMenuEntry("Change Kernel Size", 0);
		glutAddMenuEntry("Change Blocker Search Size", 1);

	otherFunctionsMenuID = glutCreateMenu(otherFunctionsMenu);
		glutAddMenuEntry("Animation [On/Off]", 0);
		glutAddMenuEntry("Shadow Intensity [On/Off]", 1);
		glutAddMenuEntry("Print Data", 2);
		
	glutCreateMenu(mainMenu);
		glutAddSubMenu("Soft Shadow Mapping", softShadowMenuID);
		glutAddSubMenu("Soft Shadow Parameters", softShadowParametersMenuID);
		glutAddSubMenu("Transformation", transformationMenuID);
		glutAddSubMenu("Other Functions", otherFunctionsMenuID);
		glutAttachMenu(GLUT_RIGHT_BUTTON);

}

void initGL(char *configurationFile) {

	glClearColor(0.0f, 0.0f, 0.0f, 1.0);
	glShadeModel(GL_SMOOTH);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);  

	if(textures[0] == 0)
		glGenTextures(20, textures);
	if(frameBuffer[0] == 0)
		glGenFramebuffers(10, frameBuffer);
	if(sceneVBO[0] == 0)
		glGenBuffers(5, sceneVBO);
	if(sceneTextures[0] == 0)
		glGenTextures(4, sceneTextures);

	scene = new Mesh();
	sceneLoader = new SceneLoader(configurationFile, scene);
	sceneLoader->load();

	float centroid[3];
	lightSource = new LightSource();
	scene->computeCentroid(centroid);
	cameraEye[0] = sceneLoader->getCameraPosition()[0]; cameraEye[1] = sceneLoader->getCameraPosition()[1]; cameraEye[2] = sceneLoader->getCameraPosition()[2];
	cameraAt[0] = sceneLoader->getCameraAt()[0]; cameraAt[1] = sceneLoader->getCameraAt()[1]; cameraAt[2] = sceneLoader->getCameraAt()[2];
	cameraUp[0] = 0.0; cameraUp[1] = 0.0; cameraUp[2] = 1.0;
	lightSource->setAt(glm::vec3(sceneLoader->getLightAt()[0], sceneLoader->getLightAt()[1], sceneLoader->getLightAt()[2]));
	lightSource->setUp(glm::vec3(0.0, 0.0, 1.0));
	lightSource->setSize(32.0);
	lightSource->setNumberOfPointLights(64.0);

	resetShadowParameters();
	shadowParams.MSSM = true;
	shadowParams.SAT = false;
	shadowParams.shadowMapWidth = shadowMapWidth;
	shadowParams.shadowMapHeight = shadowMapHeight;
	shadowParams.windowWidth = windowWidth;
	shadowParams.windowHeight = windowHeight;
	shadowParams.shadowIntensity = 0.25;
	shadowParams.accFactor = 1.0/lightSource->getNumberOfPointLights();
	shadowParams.blockerSearchSize = 7;
	shadowParams.kernelSize = 15;
	shadowParams.lightSourceRadius = lightSource->getSize()/2;

	myGLTextureViewer.loadQuad();
	createMenu();

	if(scene->textureFromImage())
		for(int num = 0; num < scene->getNumberOfTextures(); num++)
			myGLTextureViewer.loadRGBTexture(scene->getTexture()[num]->getData(), sceneTextures, num, scene->getTexture()[num]->getWidth(), scene->getTexture()[num]->getHeight());

	myGLTextureViewer.loadRGBATexture((float*)NULL, textures, SHADOW_MAP_COLOR, shadowMapWidth, shadowMapHeight);
	myGLTextureViewer.loadRGBATexture((float*)NULL, textures, TEMP_SHADOW_MAP_COLOR, shadowMapWidth, shadowMapHeight);
	myGLTextureViewer.loadRGBATexture((float*)NULL, textures, SAT_SHADOW_MAP_COLOR, shadowMapWidth, shadowMapHeight);
	myGLTextureViewer.loadRGBATexture((float*)NULL, textures, HIERARCHICAL_SHADOW_MAP_COLOR, shadowMapWidth, shadowMapHeight);
	myGLTextureViewer.loadRGBATexture((float*)NULL, textures, ACCUMULATION_MAP_COLOR, windowWidth, windowHeight);
	myGLTextureViewer.loadRGBATexture((float*)NULL, textures, TEMP_ACCUMULATION_MAP_COLOR, windowWidth, windowHeight, GL_NEAREST);
	myGLTextureViewer.loadRGBATexture((float*)NULL, textures, VERTEX_MAP_COLOR, windowWidth, windowHeight, GL_NEAREST);
	myGLTextureViewer.loadRGBATexture((float*)NULL, textures, NORMAL_MAP_COLOR, windowWidth, windowHeight, GL_NEAREST);

	myGLTextureViewer.loadDepthComponentTexture(NULL, textures, SHADOW_MAP_DEPTH, shadowMapWidth, shadowMapHeight);
	myGLTextureViewer.loadDepthComponentTexture(NULL, textures, TEMP_SHADOW_MAP_DEPTH, shadowMapWidth, shadowMapHeight);
	myGLTextureViewer.loadDepthComponentTexture(NULL, textures, SAT_SHADOW_MAP_DEPTH, shadowMapWidth, shadowMapHeight);
	myGLTextureViewer.loadDepthComponentTexture(NULL, textures, HIERARCHICAL_SHADOW_MAP_DEPTH, shadowMapWidth, shadowMapHeight);
	myGLTextureViewer.loadDepthComponentTexture(NULL, textures, ACCUMULATION_MAP_DEPTH, windowWidth, windowHeight);
	myGLTextureViewer.loadDepthComponentTexture(NULL, textures, TEMP_ACCUMULATION_MAP_DEPTH, windowWidth, windowHeight);
	myGLTextureViewer.loadDepthComponentTexture(NULL, textures, GBUFFER_MAP_DEPTH, windowWidth, windowHeight);
	
	glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer[SHADOW_FRAMEBUFFER]);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, textures[SHADOW_MAP_DEPTH], 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[SHADOW_MAP_COLOR], 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer[TEMP_SHADOW_FRAMEBUFFER]);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, textures[TEMP_SHADOW_MAP_DEPTH], 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[TEMP_SHADOW_MAP_COLOR], 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer[SAT_SHADOW_FRAMEBUFFER]);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, textures[SAT_SHADOW_MAP_DEPTH], 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[SAT_SHADOW_MAP_COLOR], 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer[ACCUMULATION_FRAMEBUFFER]);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, textures[ACCUMULATION_MAP_DEPTH], 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[ACCUMULATION_MAP_COLOR], 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer[TEMP_ACCUMULATION_FRAMEBUFFER]);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, textures[TEMP_ACCUMULATION_MAP_DEPTH], 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[TEMP_ACCUMULATION_MAP_COLOR], 0);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer[GBUFFER_FRAMEBUFFER]);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, textures[GBUFFER_MAP_DEPTH], 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textures[VERTEX_MAP_COLOR], 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, textures[NORMAL_MAP_COLOR], 0);
	GLenum bufs[2];
	bufs[0] = GL_COLOR_ATTACHMENT0;
	bufs[1] = GL_COLOR_ATTACHMENT1;
	glDrawBuffers(2,bufs);
	glBindFramebuffer(GL_FRAMEBUFFER, 0);

}

int main(int argc, char **argv) {

	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH | GLUT_ALPHA);
	glutInitWindowSize(windowWidth, windowHeight);
	glutCreateWindow("Soft Shadow Mapping");

	glutReshapeFunc(reshape);
	glutDisplayFunc(display);
	glutIdleFunc(idle);
	glutKeyboardFunc(keyboard);
	glutSpecialFunc(specialKeyboard);

	glewInit();
	initGL(argv[1]);

	initShader("Shaders/Scene", SCENE_SHADER);
	initShader("Shaders/MonteCarlo/Shadow", SHADOW_SHADER);
	initShader("Shaders/MonteCarlo/Clear", CLEAR_SHADER);
	initShader("Shaders/MonteCarlo/Copy", COPY_SHADER);
	initShader("Shaders/MonteCarlo/Render", MONTE_CARLO_RENDER_SHADER);
	initShader("Shaders/MonteCarlo/GBuffer", GBUFFER_SHADER);
	initShader("Shaders/SoftShadow", SOFT_SHADOW_SHADER);
	initShader("Shaders/Moments/Moments", MOMENT_SHADER);
	initShader("Shaders/Exponential", EXPONENTIAL_SHADER);
	initShader("Shaders/Moments/SATHorizontalPass", SAT_HORIZONTAL_PASS_SHADER);
	initShader("Shaders/Moments/SATVerticalPass", SAT_VERTICAL_PASS_SHADER);
	initShader("Shaders/Moments/PrepareMinMax", PREPARE_MIN_MAX_SHADER);
	initShader("Shaders/Moments/MinMax", MIN_MAX_SHADER);
	glUseProgram(0); 

	glutMainLoop();

	delete scene;
	delete sceneLoader;
	delete lightSource;
	return 0;

}