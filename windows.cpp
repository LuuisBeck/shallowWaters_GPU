//
// Created by luchin on 29-07-21.
//

// A simple introductory program; its main window contains a static picture
// of a triangle, whose three vertices are red, green and blue.  The program
// illustrates viewing with default viewing parameters only.

#include <GL/glut.h>
#include <GL/gl.h>
using namespace glm;
if( !glfwInit() )
{
    fprintf( stderr, "Error al inicializar GLFW\n" );
    return -1;
}

Windows::Windows(int height, int width, int grid_height, int grid_width, int antiAliasing){

    glfwWindowHint(GLFW_SAMPLES, antiAliasing); // 4x antialiasing
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3); // Queremos OpenGL 3.3
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // Para hacer feliz a MacOS ; Aunque no debería ser necesaria
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE); //No queremos el viejo OpenGL

    //Crear una ventana y su contexto OpenGL
    Windows::window = glfwCreateWindow(height, width, "Tutorial 01", NULL, NULL);
    if( window == NULL ){
        fprintf( stderr, "Falla al abrir una ventana GLFW. Si usted tiene una GPU Intel, está no es compatible con 3.3. Intente con la versión 2.1 de los tutoriales.\n" );
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window); // Inicializar GLEW
    glewExperimental=true; // Se necesita en el perfil de base.
    if (glewInit() != GLEW_OK) {
        fprintf(stderr, "Falló al inicializar GLEW\n");
        return -1;
    }

    // Capturar la tecla ESC cuando sea presionada
    glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);

    do{
        // No vamos a pintar nada, nos vemos en el tutorial 2 !

        // Intercambiar buffers
        glfwSwapBuffers(window);
        glfwPollEvents();

    } // Revisar que la tecla ESC fue presionada y cerrar la ventana
    while( glfwGetKey(window, GLFW_KEY_ESCAPE ) != GLFW_PRESS &&
    glfwWindowShouldClose(window) == 0 );


}

int Windows::getWidth() {}
int Windows::getHeight(){}
int Windows::getGridHeight(){}
int Windows::getGridWidth(){}

// Clears the current window and draws a triangle.
void display() {

    // Set every pixel in the frame buffer to the current clear color.
    glClear(GL_COLOR_BUFFER_BIT);

    // Drawing is done by specifying a sequence of vertices.  The way these
    // vertices are connected (or not connected) depends on the argument to
    // glBegin.  GL_POLYGON constructs a filled polygon.
    glBegin(GL_POLYGON);
    glColor3f(1, 0, 0); glVertex3f(-0.6, -0.75, 0.5);
    glColor3f(0, 1, 0); glVertex3f(0.6, -0.75, 0);
    glColor3f(0, 0, 1); glVertex3f(0, 0.75, 0);
    glEnd();

    // Flush drawing command buffer to make drawing happen as soon as possible.
    glFlush();
}

// Initializes GLUT, the display mode, and main window; registers callbacks;
// enters the main event loop.
int main(int argc, char** argv) {

    // Use a single buffered window in RGB mode (as opposed to a double-buffered
    // window or color-index mode).
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_SINGLE | GLUT_RGB);

    // Position window at (80,80)-(480,380) and give it a title.
    glutInitWindowPosition(80, 80);
    glutInitWindowSize(400, 300);
    glutCreateWindow("A Simple Triangle");

    // Tell GLUT that whenever the main window needs to be repainted that it
    // should call the function display().
    glutDisplayFunc(display);

    // Tell GLUT to start reading and processing events.  This function
    // never returns; the program only exits when the user closes the main
    // window or kills the process.
    glutMainLoop();
}

