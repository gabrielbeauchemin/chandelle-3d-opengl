// Include standard headers
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <algorithm>
// Include standard headers
#include <stdio.h>
#include <stdlib.h>
#include <vector>

// Include GLEW
#include <GL/glew.h>

// Include GLFW
#include <glfw3.h>
GLFWwindow* window;

// Include GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
using namespace glm;

#include <common/shader.hpp>
#include <common/texture.hpp>
#include <common/controls.hpp>
#include <common/objloader.hpp>
#include <common/vboindexer.hpp>
glm::vec3 calculateHermit(float u, glm::vec3 p1, glm::vec3 p2, glm::vec3 tangentP1, glm::vec3 tangentP2)
{
	return (((2 * pow(u, 3)) - (3 * pow(u, 2)) + 1) * p1) +
		(((-2 * pow(u, 3)) + (3 * pow(u, 2))) * p2) +
		((pow(u, 3) - (2 * pow(u, 2)) + u) * tangentP1) +
		((pow(u, 3) - pow(u, 2)) * tangentP2);
}

// CPU representation of a particle
struct Particle {
	glm::vec3 pos, speed;
	unsigned char r, g, b, a; // Color
	float size, angle, weight;
	float life; // Remaining life of the particle. if <0 : dead and unused.
	float cameradistance; // *Squared* distance to the camera. if dead : -1.0f
	bool isBlue = true;
	bool isCentered = false;

	//Next attributes are used to throw particule in a hermit curve
	float u = 0; //Normalized u used for curvature interpolation
	glm::vec3 beginningPos;
	glm::vec3 tangent1;
	glm::vec3 tangent2;

	bool operator<(const Particle& that) const {
		// Sort in reverse order : far particles drawn first.
		return this->cameradistance > that.cameradistance;
	}
};

const int MaxParticles = 10000;
glm::vec3 flammeStart, endFlamme;
Particle ParticlesContainer[MaxParticles];
int LastUsedParticle = 0;

// Finds a Particle in ParticlesContainer which isn't used yet.
// (i.e. life < 0);
int FindUnusedParticle() {

	for (int i = LastUsedParticle; i<MaxParticles; i++) {
		if (ParticlesContainer[i].life < 0) {
			LastUsedParticle = i;
			return i;
		}
	}

	for (int i = 0; i<LastUsedParticle; i++) {
		if (ParticlesContainer[i].life < 0) {
			LastUsedParticle = i;
			return i;
		}
	}

	return 0; // All particles are taken, override the first one
}

void SortParticles() {
	std::sort(&ParticlesContainer[0], &ParticlesContainer[MaxParticles]);
}

struct Wind
{
	glm::vec3 generateWind()
	{
		if (windStillBlow())
		{
			return currentWind;
		}

		else
		{
			const float windIntensity = 0.1;
			float x = windIntensity *((float)rand()) / (float)RAND_MAX;
			if ((rand() % 2) == 0) x *= -1;
			float y = 0;
			float z = windIntensity * ((rand() % 2) * -1) * ((float)rand()) / (float)RAND_MAX;
			if ((rand() % 2) == 0) z *= -1;

			beginningWind = glfwGetTime();
			currentWind = glm::vec3{ x, y, z };
			return currentWind;
		}
	}

private:
	float beginningWind = -1;
	glm::vec3 currentWind{ 0,0,0 };

	bool windStillBlow()
	{
		if (beginningWind == -1)
		{
			return false;

		}
		else
		{
			auto now = glfwGetTime();
			bool isWindStillBlowing = (now - beginningWind) < 0.1f;
			return isWindStillBlowing;
		}
	}
}wind;

int main( void )
{
	// Initialise GLFW
	if( !glfwInit() )
	{
		fprintf( stderr, "Failed to initialize GLFW\n" );
		getchar();
		return -1;
	}

	glfwWindowHint(GLFW_SAMPLES, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // To make MacOS happy; should not be needed
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	// Open a window and create its OpenGL context
	window = glfwCreateWindow( 1024, 768, "Tutorial 14 - Render To Texture", NULL, NULL);
	if( window == NULL ){
		fprintf( stderr, "Failed to open GLFW window. If you have an Intel GPU, they are not 3.3 compatible. Try the 2.1 version of the tutorials.\n" );
		getchar();
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);
    
    // We would expect width and height to be 1024 and 768
    int windowWidth = 1024;
    int windowHeight = 768;
    // But on MacOS X with a retina screen it'll be 1024*2 and 768*2, so we get the actual framebuffer size:
    glfwGetFramebufferSize(window, &windowWidth, &windowHeight);

	// Initialize GLEW
	glewExperimental = true; // Needed for core profile
	if (glewInit() != GLEW_OK) {
		fprintf(stderr, "Failed to initialize GLEW\n");
		getchar();
		glfwTerminate();
		return -1;
	}

	// Ensure we can capture the escape key being pressed below
	glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
    // Hide the mouse and enable unlimited mouvement
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    
    // Set the mouse at the center of the screen
    glfwPollEvents();
    glfwSetCursorPos(window, 1024/2, 768/2);

	// Dark blue background
	glClearColor(0.05f, 0.2f, 0.05f, 0);

	// Enable depth test
	glEnable(GL_DEPTH_TEST);
	// Accept fragment if it closer to the camera than the former one
	glDepthFunc(GL_LESS); 

	// Cull triangles which normal is not towards the camera
	glEnable(GL_CULL_FACE);
	glCullFace(GL_BACK);

	GLuint VertexArrayID;
	glGenVertexArrays(1, &VertexArrayID);
	glBindVertexArray(VertexArrayID);
	/////////////////////////////////////



	// Create and compile our GLSL program from the shaders
	GLuint particleProgramID = LoadShaders("Particle.vertexshader", "Particle.fragmentshader");

	// Vertex shader
	GLuint CameraRight_worldspace_ID = glGetUniformLocation(particleProgramID, "CameraRight_worldspace");
	GLuint CameraUp_worldspace_ID = glGetUniformLocation(particleProgramID, "CameraUp_worldspace");
	GLuint ViewProjMatrixID = glGetUniformLocation(particleProgramID, "VP");

	// fragment shader
	GLuint particleTextureID = glGetUniformLocation(particleProgramID, "myTextureSampler");


	static GLfloat* g_particule_position_size_data = new GLfloat[MaxParticles * 4];
	static GLubyte* g_particule_color_data = new GLubyte[MaxParticles * 4];

	for (int i = 0; i<MaxParticles; i++) {
		ParticlesContainer[i].life = -1.0f;
		ParticlesContainer[i].cameradistance = -1.0f;
	}

	GLuint particuleTexture = loadDDS("particle.DDS");

	// The VBO containing the 4 vertices of the particles.
	// Thanks to instancing, they will be shared by all particles.
	static const GLfloat g_vertex_buffer_data[] = {
		-0.5f, -0.5f, 0.0f,
		0.5f, -0.5f, 0.0f,
		-0.5f,  0.5f, 0.0f,
		0.5f,  0.5f, 0.0f,
	};
	GLuint billboard_vertex_buffer;
	glGenBuffers(1, &billboard_vertex_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, billboard_vertex_buffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(g_vertex_buffer_data), g_vertex_buffer_data, GL_STATIC_DRAW);

	// The VBO containing the positions and sizes of the particles
	GLuint particles_position_buffer;
	glGenBuffers(1, &particles_position_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, particles_position_buffer);
	// Initialize with empty (NULL) buffer : it will be updated later, each frame.
	glBufferData(GL_ARRAY_BUFFER, MaxParticles * 4 * sizeof(GLfloat), NULL, GL_STREAM_DRAW);

	// The VBO containing the colors of the particles
	GLuint particles_color_buffer;
	glGenBuffers(1, &particles_color_buffer);
	glBindBuffer(GL_ARRAY_BUFFER, particles_color_buffer);
	// Initialize with empty (NULL) buffer : it will be updated later, each frame.
	glBufferData(GL_ARRAY_BUFFER, MaxParticles * 4 * sizeof(GLubyte), NULL, GL_STREAM_DRAW);

	double lastTime = glfwGetTime();





	/////////////////////////////////////
	// Create and compile our GLSL program from the shaders
	GLuint depth_programID = LoadShaders("depth.vertexshader", "depth.fragmentshader");

	// Get a handle for our "MVP" uniform
	GLuint MatrixDepthID = glGetUniformLocation(depth_programID, "MVP");
	/////////////////////////////////////


	// Create and compile our GLSL program from the shaders
	GLuint programID = LoadShaders( "StandardShadingRTT.vertexshader", "StandardShadingRTT.fragmentshader" );

	// Get a handle for our "MVP" uniform
	GLuint MatrixID = glGetUniformLocation(programID, "MVP");
	GLuint ViewMatrixID = glGetUniformLocation(programID, "V");
	GLuint ModelMatrixID = glGetUniformLocation(programID, "M");
	GLuint DepthBiasID = glGetUniformLocation(programID, "DepthBiasMVP");

	// Get a handle for our "myTextureSampler" uniform
	GLuint TextureID  = glGetUniformLocation(programID, "myTextureSampler");

	// Get a handle for our "LightPosition" uniform
	GLuint LightID = glGetUniformLocation(programID, "LightPosition_worldspace");

	// Load the texture
	GLuint Texture = loadBMP_custom("candle.bmp");

	// Read our .obj file
	std::vector<glm::vec3> vertices;
	std::vector<glm::vec2> uvs;
	std::vector<glm::vec3> normals;
	bool res = loadOBJ("candle.obj", vertices, uvs, normals);

	std::vector<unsigned short> indices;
	std::vector<glm::vec3> indexed_vertices;
	std::vector<glm::vec2> indexed_uvs;
	std::vector<glm::vec3> indexed_normals;
	indexVBO(vertices, uvs, normals, indices, indexed_vertices, indexed_uvs, indexed_normals);

	// Load it into a VBO

	GLuint vertexbuffer;
	glGenBuffers(1, &vertexbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
	glBufferData(GL_ARRAY_BUFFER, indexed_vertices.size() * sizeof(glm::vec3), &indexed_vertices[0], GL_STATIC_DRAW);

	GLuint uvbuffer;
	glGenBuffers(1, &uvbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
	glBufferData(GL_ARRAY_BUFFER, indexed_uvs.size() * sizeof(glm::vec2), &indexed_uvs[0], GL_STATIC_DRAW);

	GLuint normalbuffer;
	glGenBuffers(1, &normalbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, normalbuffer);
	glBufferData(GL_ARRAY_BUFFER, indexed_normals.size() * sizeof(glm::vec3), &indexed_normals[0], GL_STATIC_DRAW);

	// Generate a buffer for the indices as well
	GLuint elementbuffer;
	glGenBuffers(1, &elementbuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementbuffer);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned short), &indices[0], GL_STATIC_DRAW);


	// ---------------------------------------------
	// Render to Texture - specific code begins here
	// ---------------------------------------------

	// The framebuffer, which regroups 0, 1, or more textures, and 0 or 1 depth buffer.
	GLuint FramebufferName;
	glGenFramebuffers(1, &FramebufferName);
	glBindFramebuffer(GL_FRAMEBUFFER, FramebufferName);

	// The texture we're going to render to
	GLuint renderedTexture_texID;
	glGenTextures(1, &renderedTexture_texID);
	
	// "Bind" the newly created texture : all future texture functions will modify this texture
	glBindTexture(GL_TEXTURE_2D, renderedTexture_texID);

	// Give an empty image to OpenGL ( the last "0" means "empty" )
	glTexImage2D(GL_TEXTURE_2D, 0,GL_RGB, windowWidth, windowHeight, 0,GL_RGB, GL_UNSIGNED_BYTE, 0);

	// Poor filtering
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); 
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// Set "renderedTexture" as our colour attachement #0
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, renderedTexture_texID, 0);

	//////////////////////////////////////////



	//// Depth texture. Slower than a depth buffer, but you can sample it later in your shader
	//GLuint depthTexture;
	//glGenTextures(1, &depthTexture);
	//glBindTexture(GL_TEXTURE_2D, depthTexture);
	//glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT16, 1024, 1024, 0, GL_DEPTH_COMPONENT, GL_FLOAT, 0);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
	//glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_R_TO_TEXTURE);

	//glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthTexture, 0);




	// The framebuffer for candleFrontDepth
	GLuint candleFrontDepthBuffer;
	glGenFramebuffers(1, &candleFrontDepthBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, candleFrontDepthBuffer);

	// Texture that will contain candle front depth
	GLuint candleFrontDepth_texID;
	glGenTextures(1, &candleFrontDepth_texID);
	glBindTexture(GL_TEXTURE_2D, candleFrontDepth_texID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, windowWidth, windowHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// Set "candleFrontDepth" as our colour attachement #1
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, candleFrontDepth_texID, 0);


	// The framebuffer for candleFrontDepth
	GLuint candleBackDepthBuffer;
	glGenFramebuffers(1, &candleBackDepthBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, candleBackDepthBuffer);

	// Texture that will contain candle thickness
	GLuint candleBackDepth_texID;
	glGenTextures(1, &candleBackDepth_texID);
	glBindTexture(GL_TEXTURE_2D, candleBackDepth_texID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, windowWidth, windowHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// Set "candleFrontDepth" as our colour attachement #2
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, candleBackDepth_texID, 0);

	// The framebuffer for candleFrontDepth
	GLuint candleFrontDepthFromLightBuffer;
	glGenFramebuffers(1, &candleFrontDepthFromLightBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, candleFrontDepthFromLightBuffer);

	// Texture that will contain candle front depth from light view
	GLuint candleFrontDepthFromLight_texID;
	glGenTextures(1, &candleFrontDepthFromLight_texID);
	glBindTexture(GL_TEXTURE_2D, candleFrontDepthFromLight_texID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, windowWidth, windowHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// Set "candleFrontDepth" as our colour attachement #3
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, candleFrontDepthFromLight_texID, 0);

	// The framebuffer for candleFrontDepth
	GLuint candleBackFromLightBuffer;
	glGenFramebuffers(1, &candleBackFromLightBuffer);
	glBindFramebuffer(GL_FRAMEBUFFER, candleBackFromLightBuffer);

	// Texture that will contain candle thickness front depth from light view
	GLuint candleBackDepthFromLight_texID;
	glGenTextures(1, &candleBackDepthFromLight_texID);
	glBindTexture(GL_TEXTURE_2D, candleBackDepthFromLight_texID);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, windowWidth, windowHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// Set "candleFrontDepth" as our colour attachement #4
	glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, candleBackDepthFromLight_texID, 0);

	//////////////////////////////////////////

	// The depth buffer
	GLuint depthrenderbuffer;
	glGenRenderbuffers(1, &depthrenderbuffer);
	glBindRenderbuffer(GL_RENDERBUFFER, depthrenderbuffer);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, windowWidth, windowHeight);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthrenderbuffer);

	// Alternative : Depth texture. Slower, but you can sample it later in your shader
	GLuint depthTexture_texID;
	glGenTextures(1, &depthTexture_texID);
	glBindTexture(GL_TEXTURE_2D, depthTexture_texID);
	glTexImage2D(GL_TEXTURE_2D, 0,GL_DEPTH_COMPONENT24, windowWidth, windowHeight, 0,GL_DEPTH_COMPONENT, GL_FLOAT, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); 
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	// Depth texture alternative : 
	glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, depthTexture_texID, 0);


	// Set the list of draw buffers.
	GLenum DrawBuffers[1] = { GL_COLOR_ATTACHMENT0 };
	glDrawBuffers(1, DrawBuffers); // "1" is the size of DrawBuffers

	// Always check that our framebuffer is ok
	if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		return false;

	
	// The fullscreen quad's FBO
	static const GLfloat g_quad_vertex_buffer_data[] = { 
		-1.0f, -1.0f, 0.0f,
		 1.0f, -1.0f, 0.0f,
		-1.0f,  1.0f, 0.0f,
		-1.0f,  1.0f, 0.0f,
		 1.0f, -1.0f, 0.0f,
		 1.0f,  1.0f, 0.0f,
	};

	float cameraDepth = 100.0f;

	GLuint quad_vertexbuffer;
	glGenBuffers(1, &quad_vertexbuffer);
	glBindBuffer(GL_ARRAY_BUFFER, quad_vertexbuffer);
	glBufferData(GL_ARRAY_BUFFER, sizeof(g_quad_vertex_buffer_data), g_quad_vertex_buffer_data, GL_STATIC_DRAW);

	GLuint fronttex_loc = glGetUniformLocation(programID, "fronttexture");
	GLuint thicktex_loc = glGetUniformLocation(programID, "thicknesstexture");
	GLuint frontFromLighttex_loc = glGetUniformLocation(programID, "fronttexturefromlight");
	GLuint thickFromLighttex_loc = glGetUniformLocation(programID, "thicknesstexturefromlight");
	GLuint cameraDepth_loc = glGetUniformLocation(programID, "cameraDepth");
	GLuint depth_loc = glGetUniformLocation(programID, "depthTexture");

	// Create and compile our GLSL program from the shaders
	GLuint quad_programID = LoadShaders( "Passthrough.vertexshader", "WobblyTexture.fragmentshader" );
	GLuint tex_loc = glGetUniformLocation(quad_programID, "renderedTexture");
	GLuint frontquadtex_loc = glGetUniformLocation(quad_programID, "fronttexture");
	GLuint depthtex_loc = glGetUniformLocation(quad_programID, "depthtexture");
	GLuint timeID = glGetUniformLocation(quad_programID, "time");
	
	flammeStart = glm::vec3(0, 0, 0);
	endFlamme = glm::vec3(0.f, 3.f, 0.f);

	do{


		// Compute the MVP matrix from keyboard and mouse input
		computeMatricesFromInputs();
		glm::mat4 ProjectionMatrix = getProjectionMatrix();
		glm::mat4 ViewMatrix = getViewMatrix();
		glm::mat4 ModelMatrix = glm::mat4(1.0);
		glm::mat4 MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;



		glDisable(GL_BLEND);

		//	ICI, ON VA STOCKER LA PROFONDEUR DE LA SURFACE "FRONT" DE NOTRE OBJET

		// Use our shader
		glUseProgram(depth_programID);
		glCullFace(GL_BACK);
		// Render to our framebuffer
		glBindFramebuffer(GL_FRAMEBUFFER, candleFrontDepthBuffer);
		glViewport(0, 0, windowWidth, windowHeight); // Render on the whole framebuffer, complete from the lower left corner to the upper right
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		// Send our transformation to the currently bound shader, in the "MVP" uniform
		glUniformMatrix4fv(MatrixDepthID, 1, GL_FALSE, &MVP[0][0]);
		// 1rst attribute buffer : vertices
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
		// Index buffer
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementbuffer);
		// Draw the triangles !
		glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_SHORT, (void*)0);
		glDisableVertexAttribArray(0);


		//	ICI, ON VA STOCKER LA PROFONDEUR DE LA SURFACE "back" DE NOTRE OBJET

		// Use our shader
		glUseProgram(depth_programID);
		glCullFace(GL_FRONT);
		// Render to our framebuffer
		glBindFramebuffer(GL_FRAMEBUFFER, candleBackDepthBuffer);
		glViewport(0, 0, windowWidth, windowHeight); // Render on the whole framebuffer, complete from the lower left corner to the upper right
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		// Send our transformation to the currently bound shader, in the "MVP" uniform
		glUniformMatrix4fv(MatrixDepthID, 1, GL_FALSE, &MVP[0][0]);
		// 1rst attribute buffer : vertices
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
		// Index buffer
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementbuffer);
		// Draw the triangles !
		glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_SHORT, (void*)0);
		glDisableVertexAttribArray(0);


		//	ICI, ON VA STOCKER LA PROFONDEUR DE LA SURFACE "back" DE NOTRE OBJET

		// Use our shader
		glUseProgram(depth_programID);
		glCullFace(GL_BACK);
		// Render to our framebuffer
		glBindFramebuffer(GL_FRAMEBUFFER, depthrenderbuffer);
		glViewport(0, 0, windowWidth, windowHeight); // Render on the whole framebuffer, complete from the lower left corner to the upper right
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		// Compute the MVP matrix from keyboard and mouse input
		computeMatricesFromInputs();
		ProjectionMatrix = getProjectionMatrix();
		ViewMatrix = getViewMatrix();
		ViewMatrix[0][2] += 100;
		ModelMatrix = glm::mat4(1.0);
		MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;
		// Send our transformation to the currently bound shader, in the "MVP" uniform
		glUniformMatrix4fv(MatrixDepthID, 1, GL_FALSE, &MVP[0][0]);
		// 1rst attribute buffer : vertices
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
		// Index buffer
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementbuffer);
		// Draw the triangles !
		glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_SHORT, (void*)0);
		glDisableVertexAttribArray(0);


		//ProjectionMatrix = glm::perspective(glm::radians(FoV), 4.0f / 3.0f, 0.1f, 100.0f);
		//// Camera matrix
		//ViewMatrix = glm::lookAt(
		//	position,           // Camera is here
		//	position + direction, // and looks here : at the same position, plus "direction"
		//	up                  // Head is up (set to 0,-1,0 to look upside-down)
		//);

		glm::vec3 lightPos = endFlamme + wind.generateWind()*4.0f;
		//glm::mat4 depthProjectionMatrix = glm::perspective<float>(glm::radians(45.0), 1.0, 0.1f, 20.0);
		//glm::mat4 depthViewMatrix = glm::lookAt(lightPos, glm::vec3(0, -1, 0), glm::vec3(0, 0, -1));
		////	ProjectionMatrix = glm::perspective(

		//glm::mat4 depthModelMatrix = glm::mat4(1.0);
		//glm::mat4 depthMVP = depthProjectionMatrix * depthViewMatrix * depthModelMatrix;



		//	ICI, ON VA STOCKER LA PROFONDEUR DE LA SURFACE "back" DE NOTRE OBJET

		// Use our shader
		glUseProgram(depth_programID);
		glCullFace(GL_BACK);
		// Render to our framebuffer
		glBindFramebuffer(GL_FRAMEBUFFER, candleFrontDepthFromLightBuffer);
		glViewport(0, 0, windowWidth, windowHeight); // Render on the whole framebuffer, complete from the lower left corner to the upper right
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		// Compute the MVP matrix from keyboard and mouse input
		computeMatricesFromInputs();

		ProjectionMatrix = glm::perspective<float>(glm::radians(45.0), 1.0, 0.1f, 15);
		ViewMatrix = glm::lookAt(lightPos, glm::vec3(0, -1, 0), glm::vec3(0, 0, -1));
		ModelMatrix = glm::mat4(1.0);
		glm::mat4 depthMVP = MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;

		// Send our transformation to the currently bound shader, in the "MVP" uniform
		glUniformMatrix4fv(MatrixDepthID, 1, GL_FALSE, &MVP[0][0]);
		// 1rst attribute buffer : vertices
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
		// Index buffer
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementbuffer);
		// Draw the triangles !
		glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_SHORT, (void*)0);
		glDisableVertexAttribArray(0);


		//	ICI, ON VA STOCKER LA PROFONDEUR DE LA SURFACE "back" DE NOTRE OBJET

		// Use our shader
		glUseProgram(depth_programID);
		glCullFace(GL_FRONT);
		// Render to our framebuffer
		glBindFramebuffer(GL_FRAMEBUFFER, candleBackFromLightBuffer);
		glViewport(0, 0, windowWidth, windowHeight); // Render on the whole framebuffer, complete from the lower left corner to the upper right
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		// Compute the MVP matrix from keyboard and mouse input
		computeMatricesFromInputs();

		//ProjectionMatrix = getProjectionMatrix();
		//ViewMatrix = getViewMatrix();
		//ModelMatrix = glm::mat4(1.0);
		//MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;

		// Send our transformation to the currently bound shader, in the "MVP" uniform
		glUniformMatrix4fv(MatrixDepthID, 1, GL_FALSE, &MVP[0][0]);
		// 1rst attribute buffer : vertices
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
		// Index buffer
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementbuffer);
		// Draw the triangles !
		glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_SHORT, (void*)0);
		glDisableVertexAttribArray(0);

		//	Remettre la camera à sa position initiale



		// Enable blending
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		// Render to our framebuffer
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glViewport(0, 0, windowWidth, windowHeight); // Render on the whole framebuffer, complete from the lower left corner to the upper right

		// Clear the screen
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		glCullFace(GL_BACK);


		// Use our shader
		glUseProgram(programID);

		// Set our "renderedTexture" sampler to use Texture Unit 0
		glUniform1i(tex_loc, 0);
		// Bind our texture in Texture Unit 0
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, renderedTexture_texID);

		glUniform1i(fronttex_loc, 1);
		glActiveTexture(GL_TEXTURE1);
		glBindTexture(GL_TEXTURE_2D, candleFrontDepth_texID);

		glUniform1i(thicktex_loc, 2);
		glActiveTexture(GL_TEXTURE2);
		glBindTexture(GL_TEXTURE_2D, candleBackDepth_texID);

		glUniform1i(frontFromLighttex_loc, 3);
		glActiveTexture(GL_TEXTURE3);
		glBindTexture(GL_TEXTURE_2D, candleFrontDepthFromLight_texID);

		glUniform1i(thickFromLighttex_loc, 4);
		glActiveTexture(GL_TEXTURE4);
		glBindTexture(GL_TEXTURE_2D, candleBackDepthFromLight_texID);

		glUniform1i(depth_loc, 5);
		glActiveTexture(GL_TEXTURE5);
		glBindTexture(GL_TEXTURE_2D, depthTexture_texID);

		glUniform1f(cameraDepth_loc, 100.0f);

		glUniform1f(timeID, (float)(glfwGetTime()*10.0f));

		// Compute the MVP matrix from keyboard and mouse input
		computeMatricesFromInputs();
		ProjectionMatrix = getProjectionMatrix();
		ViewMatrix = getViewMatrix();
		ModelMatrix = glm::mat4(1.0);
		MVP = ProjectionMatrix * ViewMatrix * ModelMatrix;

		glm::mat4 biasMatrix(
			0.5, 0.0, 0.0, 0.0,
			0.0, 0.5, 0.0, 0.0,
			0.0, 0.0, 0.5, 0.0,
			0.5, 0.5, 0.5, 1.0
		);

		glm::mat4 depthBiasMVP = biasMatrix*depthMVP;

		// Send our transformation to the currently bound shader, 
		// in the "MVP" uniform
		glUniformMatrix4fv(MatrixID, 1, GL_FALSE, &MVP[0][0]);
		glUniformMatrix4fv(ModelMatrixID, 1, GL_FALSE, &ModelMatrix[0][0]);
		glUniformMatrix4fv(ViewMatrixID, 1, GL_FALSE, &ViewMatrix[0][0]);
		glUniformMatrix4fv(DepthBiasID, 1, GL_FALSE, &depthBiasMVP[0][0]);

		glUniform3f(LightID, lightPos.x, lightPos.y, lightPos.z);

		// Bind our texture in Texture Unit 6
		glActiveTexture(GL_TEXTURE6);
		glBindTexture(GL_TEXTURE_2D, Texture);
		// Set our "myTextureSampler" sampler to use Texture Unit 6
		glUniform1i(TextureID, 6);

		// 1rst attribute buffer : vertices
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, vertexbuffer);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

		// 2nd attribute buffer : UVs
		glEnableVertexAttribArray(1);
		glBindBuffer(GL_ARRAY_BUFFER, uvbuffer);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);

		// 3rd attribute buffer : normals
		glEnableVertexAttribArray(2);
		glBindBuffer(GL_ARRAY_BUFFER, normalbuffer);
		glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);

		glVertexAttribDivisor(0, 0);
		glVertexAttribDivisor(1, 0);
		glVertexAttribDivisor(2, 0);

		// Index buffer
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, elementbuffer);

		// Draw the triangles !
		glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_SHORT, (void*)0);

		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);
		glDisableVertexAttribArray(2);





		//////////////////////////////////////
		//	Particle part

		// Clear the screen
		//	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

		double currentTime = glfwGetTime();
		double delta = currentTime - lastTime;
		lastTime = currentTime;

		computeMatricesFromInputs();
		ProjectionMatrix = getProjectionMatrix();
		ViewMatrix = getViewMatrix();

		// We will need the camera's position in order to sort the particles
		// w.r.t the camera's distance.
		// There should be a getCameraPosition() function in common/controls.cpp, 
		// but this works too.
		glm::vec3 CameraPosition(glm::inverse(ViewMatrix)[3]);

		glm::mat4 ViewProjectionMatrix = ProjectionMatrix * ViewMatrix;

		// Generate 10 new particule each millisecond,
		// but limit this to 16 ms (60 fps), or if you have 1 long frame (1sec),
		// newparticles will be huge and the next frame even longer.
		int newparticles = (int)(delta*10000.0);
		if (newparticles > (int)(0.016f*10000.0))
			newparticles = (int)(0.016f*10000.0);

		const float particuleLifeTime = 1.f;
		const auto endFlamme = glm::vec3(0.f, 2.6f, 0.f);
		for (int i = 0; i<newparticles; i++) {
			int particleIndex = FindUnusedParticle();
			ParticlesContainer[particleIndex].life = particuleLifeTime; // This particle will live 1 seconds.

																		//To imit a flamme the beginning point is a circle at the top of the candle where the flamme begins
			glm::vec3 centerCircle(0, 0, 0);
			const static float cercleRadius = 0.1f;
			int randomAngle = rand() % 360;
			const float randomReel1 = ((float)rand()) / (float)RAND_MAX;
			const float randomReel2 = ((float)rand()) / (float)RAND_MAX;
			float randomXDelta, randomZDelta;
			//50% of the time, the particule will be of the edge of the circle for beautif effect
			if (rand() % 2)
			{
				//Somewhere inside the circle
				randomXDelta = randomReel1 * (cos(randomAngle) * cercleRadius);
				randomZDelta = randomReel2 * (sin(randomAngle) * cercleRadius);
			}
			else
			{
				//On the edge of the circle
				randomXDelta = cos(randomAngle) * cercleRadius;
				randomZDelta = sin(randomAngle) * cercleRadius;
			}

			//is the flamme particule near the center of the flamme
			ParticlesContainer[particleIndex].pos = glm::vec3(centerCircle.x + randomXDelta, centerCircle.y + 0, centerCircle.z + randomZDelta);
			ParticlesContainer[particleIndex].beginningPos = ParticlesContainer[particleIndex].pos;
			ParticlesContainer[particleIndex].isCentered = max(abs(randomXDelta), abs(randomZDelta)) < (cercleRadius / 2);

			//calculate tangent to indicated the curves of particule
			const float curvature = 20;
			glm::vec3 flammeDirection = endFlamme - ParticlesContainer[particleIndex].pos;
			ParticlesContainer[particleIndex].speed = flammeDirection;

			ParticlesContainer[particleIndex].tangent1 = glm::vec3(curvature*randomXDelta, 5.f, 0 + curvature*randomZDelta)
				- ParticlesContainer[particleIndex].pos;
			ParticlesContainer[particleIndex].tangent2 = glm::vec3(-curvature*endFlamme.x, endFlamme.y, endFlamme.z - curvature*randomZDelta)
				- endFlamme;

			//Flamme begins blue at the beginning
			float randomGreen = 20 + (rand() % 30);
			float darkenCenter = max(abs(randomXDelta), abs(randomZDelta)) / cercleRadius;
			ParticlesContainer[particleIndex].r = darkenCenter * (randomGreen / 3);
			ParticlesContainer[particleIndex].g = darkenCenter * (randomGreen);
			ParticlesContainer[particleIndex].b = darkenCenter * (100 - (rand() % 20));
			ParticlesContainer[particleIndex].a = (rand() % 256) / 2;

			ParticlesContainer[particleIndex].isBlue = true;

			const float randomReelSize = ((float)rand()) / (float)RAND_MAX;
			ParticlesContainer[particleIndex].size = 0.2f + (randomReelSize*0.05f);
		}


		// Simulate all particles
		int ParticlesCount = 0;
		for (int i = 0; i<MaxParticles; i++) {

			Particle& p = ParticlesContainer[i];

			//The flamme begins blue but it must change to light yellow or red later
			if (p.life < 0.95f && p.isBlue)
			{
				bool isNonCenterHotFlame = (rand() % 10 == 0);
				if (p.isCentered || isNonCenterHotFlame)
				{
					int randomRed = 255 - (rand() % 10);
					p.r = randomRed;
					p.g = randomRed / 2;
					p.b = 0;
					p.a = (rand() % 256) / 2;
				}
				else
				{
					int randomYellow = 255 - (rand() % 10);
					p.r = randomYellow;
					p.g = randomYellow;
					p.b = 100 + (rand() % 155);
					p.a = (rand() % 256) / 2;
				}
				p.isBlue = false;
			}

			if (p.life > 0.0f) {

				p.life -= delta;
				p.u = 1 - ((max(p.life, 0.f)) / particuleLifeTime); //Normalized u used for curvature interpolation

																	//Diminish particules size at the end of the flamme, so it looks good
				if (p.u > 0.7f)
				{
					p.size = 0.5 * ((1.f - p.u) / 0.7);
				}

				if (p.life > 0.0f) {

					// Simulate simple physics : gravity only, no collisions
					p.speed += glm::vec3(0.0f, -9.81f, 0.0f) * (float)delta * 0.5f;
					p.pos = calculateHermit(p.u, p.beginningPos, endFlamme, p.tangent1, p.tangent2);
					p.pos += 3 * (p.u) * wind.generateWind();
					p.cameradistance = glm::length(p.pos - CameraPosition);

					// Fill the GPU buffer
					g_particule_position_size_data[4 * ParticlesCount + 0] = p.pos.x;
					g_particule_position_size_data[4 * ParticlesCount + 1] = p.pos.y;
					g_particule_position_size_data[4 * ParticlesCount + 2] = p.pos.z;

					g_particule_position_size_data[4 * ParticlesCount + 3] = p.size;

					g_particule_color_data[4 * ParticlesCount + 0] = p.r;
					g_particule_color_data[4 * ParticlesCount + 1] = p.g;
					g_particule_color_data[4 * ParticlesCount + 2] = p.b;
					g_particule_color_data[4 * ParticlesCount + 3] = p.a;

				}
				else {
					//Particles that just died will be put at the end of the buffer in SortParticles();
					p.cameradistance = -1.0f;
				}

				ParticlesCount++;

			}
		}

		SortParticles();


		//printf("%d ",ParticlesCount);


		// Update the buffers that OpenGL uses for rendering.
		// There are much more sophisticated means to stream data from the CPU to the GPU, 
		// but this is outside the scope of this tutorial.
		// http://www.opengl.org/wiki/Buffer_Object_Streaming


		glBindBuffer(GL_ARRAY_BUFFER, particles_position_buffer);
		glBufferData(GL_ARRAY_BUFFER, MaxParticles * 4 * sizeof(GLfloat), NULL, GL_STREAM_DRAW); // Buffer orphaning, a common way to improve streaming perf. See above link for details.
		glBufferSubData(GL_ARRAY_BUFFER, 0, ParticlesCount * sizeof(GLfloat) * 4, g_particule_position_size_data);

		glBindBuffer(GL_ARRAY_BUFFER, particles_color_buffer);
		glBufferData(GL_ARRAY_BUFFER, MaxParticles * 4 * sizeof(GLubyte), NULL, GL_STREAM_DRAW); // Buffer orphaning, a common way to improve streaming perf. See above link for details.
		glBufferSubData(GL_ARRAY_BUFFER, 0, ParticlesCount * sizeof(GLubyte) * 4, g_particule_color_data);


		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		// Use our shader
		glUseProgram(particleProgramID);

		// Bind our texture in Texture Unit 0
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, particuleTexture);
		// Set our "myTextureSampler" sampler to use Texture Unit 0
		glUniform1i(particleTextureID, 0);

		// Same as the billboards tutorial
		glUniform3f(CameraRight_worldspace_ID, ViewMatrix[0][0], ViewMatrix[1][0], ViewMatrix[2][0]);
		glUniform3f(CameraUp_worldspace_ID, ViewMatrix[0][1], ViewMatrix[1][1], ViewMatrix[2][1]);

		glUniformMatrix4fv(ViewProjMatrixID, 1, GL_FALSE, &ViewProjectionMatrix[0][0]);

		// 1rst attribute buffer : vertices
		glEnableVertexAttribArray(0);
		glBindBuffer(GL_ARRAY_BUFFER, billboard_vertex_buffer);
		glVertexAttribPointer(
			0,                  // attribute. No particular reason for 0, but must match the layout in the shader.
			3,                  // size
			GL_FLOAT,           // type
			GL_FALSE,           // normalized?
			0,                  // stride
			(void*)0            // array buffer offset
		);

		// 2nd attribute buffer : positions of particles' centers
		glEnableVertexAttribArray(1);
		glBindBuffer(GL_ARRAY_BUFFER, particles_position_buffer);
		glVertexAttribPointer(
			1,                                // attribute. No particular reason for 1, but must match the layout in the shader.
			4,                                // size : x + y + z + size => 4
			GL_FLOAT,                         // type
			GL_FALSE,                         // normalized?
			0,                                // stride
			(void*)0                          // array buffer offset
		);

		// 3rd attribute buffer : particles' colors
		glEnableVertexAttribArray(2);
		glBindBuffer(GL_ARRAY_BUFFER, particles_color_buffer);
		glVertexAttribPointer(
			2,                                // attribute. No particular reason for 1, but must match the layout in the shader.
			4,                                // size : r + g + b + a => 4
			GL_UNSIGNED_BYTE,                 // type
			GL_TRUE,                          // normalized?    *** YES, this means that the unsigned char[4] will be accessible with a vec4 (floats) in the shader ***
			0,                                // stride
			(void*)0                          // array buffer offset
		);

		// These functions are specific to glDrawArrays*Instanced*.
		// The first parameter is the attribute buffer we're talking about.
		// The second parameter is the "rate at which generic vertex attributes advance when rendering multiple instances"
		// http://www.opengl.org/sdk/docs/man/xhtml/glVertexAttribDivisor.xml
		glVertexAttribDivisor(0, 0); // particles vertices : always reuse the same 4 vertices -> 0
		glVertexAttribDivisor(1, 1); // positions : one per quad (its center)                 -> 1
		glVertexAttribDivisor(2, 1); // color : one per quad                                  -> 1

									 // Draw the particules !
									 // This draws many times a small triangle_strip (which looks like a quad).
									 // This is equivalent to :
									 // for(i in ParticlesCount) : glDrawArrays(GL_TRIANGLE_STRIP, 0, 4), 
									 // but faster.
		glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, ParticlesCount);

		glDisableVertexAttribArray(0);
		glDisableVertexAttribArray(1);
		glDisableVertexAttribArray(2);

		//////////////////////////////////////



		// Swap buffers
		glfwSwapBuffers(window);
		glfwPollEvents();

	}
	// Check if the ESC key was pressed or the window was closed
	while( (glfwGetKey(window, GLFW_KEY_ESCAPE) || glfwGetKey(window, GLFW_KEY_F5)) != GLFW_PRESS &&
		   glfwWindowShouldClose(window) == 0 );

	// Cleanup VBO and shader
	glDeleteBuffers(1, &vertexbuffer);
	glDeleteBuffers(1, &uvbuffer);
	glDeleteBuffers(1, &normalbuffer);
	glDeleteBuffers(1, &elementbuffer);
	glDeleteProgram(programID);
	glDeleteProgram(depth_programID);
	glDeleteProgram(quad_programID);
	glDeleteTextures(1, &Texture);

	glDeleteFramebuffers(1, &FramebufferName);
	glDeleteRenderbuffers(1, &candleFrontDepthBuffer);
	glDeleteRenderbuffers(1, &candleBackDepthBuffer);
	glDeleteRenderbuffers(1, &candleFrontDepthFromLightBuffer);
	glDeleteRenderbuffers(1, &candleBackFromLightBuffer);
	glDeleteRenderbuffers(1, &depthrenderbuffer);


	glDeleteTextures(1, &renderedTexture_texID);
	glDeleteTextures(1, &candleFrontDepth_texID);
	glDeleteTextures(1, &candleBackDepth_texID);
	glDeleteTextures(1, &candleFrontDepthFromLight_texID);
	glDeleteTextures(1, &candleBackDepthFromLight_texID);
	glDeleteTextures(1, &depthTexture_texID);
	glDeleteBuffers(1, &quad_vertexbuffer);
	glDeleteVertexArrays(1, &VertexArrayID);


	// Close OpenGL window and terminate GLFW
	glfwTerminate();

	return 0;
}

