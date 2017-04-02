#include <iostream>
#include <vector>
#include "SOIL.h"
#include "Utility.h"
#include <ctime>

//======================================================
// ВНИМАНИЕ! Большая часть кода так или иначе была 
// вдохновлена либо сайтом https://learnopengl.com, либо 
// публично анонсированными идеями других студентов
// (например, использование координаты TexCoord.y для
// имитации ambient occlusion), что не является 
// никаким намеренным плагиатом.
//======================================================


using namespace std;

const uint GRASS_INSTANCES = 10000; // Количество травинок
const uint MAX_FPS = 60;
GL::Camera camera;               // Мы предоставляем Вам реализацию камеры. В OpenGL камера - это просто 2 матрицы. Модельно-видовая матрица и матрица проекции. // ###
                                 // Задача этого класса только в том чтобы обработать ввод с клавиатуры и правильно сформировать эти матрицы.
                                 // Вы можете просто пользоваться этим классом для расчёта указанных матриц.


GLuint grassPointsCount; // Количество вершин у модели травинки
GLuint grassTexture;     // Текстура для травы
GLuint grassShader;      // Шейдер, рисующий траву
GLuint grassVAO;         // VAO для травы (что такое VAO почитайте в доках)
GLuint grassVariance;    // Буфер для смещения координат травинок
GLuint grassRotation;    // Буфер для поворота координат травинок
float  grassGukCoeff = 2.0;    // Коэффициент Гука для травы 
float  grassExtCoeff = 0.01;    // Коэффициент угасания скорости
vector<VM::vec4> grassVarianceData(GRASS_INSTANCES); // Вектор со смещениями для координат травинок
vector<VM::vec2> grassScaleData(GRASS_INSTANCES); // Вектор масштабирования
vector<VM::vec2> grassRotationData(GRASS_INSTANCES); // Вектор с углами поворота для координат травинок
vector<VM::vec3> grassVelocityData(GRASS_INSTANCES); // Вектор скорости
vector<VM::vec2> grassWindData(GRASS_INSTANCES); // sin - cos
vector<VM::vec4> grassColorData(GRASS_INSTANCES); // Вектор с цветами каждой травинки
VM::vec2 windData = VM::vec2(0.0, 0.1); // angle - power

GLuint groundTexture;
GLuint groundShader; // Шейдер для земли
GLuint groundVAO; // VAO для земли

// Размеры экрана
uint screenWidth = 800;
uint screenHeight = 600;

// Это для захвата мышки. Вам это не потребуется (это не значит, что нужно удалять эту строку)
bool captureMouse = true;
bool windTrigger = 0;
bool msaaTrigger = 1;

double prev_time, delta_time;

// Функция, рисующая замлю
void DrawGround() {
    // Используем шейдер для земли
    glUseProgram(groundShader);                                                  CHECK_GL_ERRORS

    // Устанавливаем юниформ для шейдера. В данном случае передадим перспективную матрицу камеры
    // Находим локацию юниформа 'camera' в шейдере
    GLint cameraLocation = glGetUniformLocation(groundShader, "camera");         CHECK_GL_ERRORS
    // Устанавливаем юниформ (загружаем на GPU матрицу проекции?)                                                     // ###
    glUniformMatrix4fv(cameraLocation, 1, GL_TRUE, camera.getMatrix().data().data()); CHECK_GL_ERRORS

    glBindTexture(GL_TEXTURE_2D, groundTexture);
    // Подключаем VAO, который содержит буферы, необходимые для отрисовки земли
    glBindVertexArray(groundVAO);                                                CHECK_GL_ERRORS

    // Рисуем землю: 2 треугольника (6 вершин)
    glDrawArrays(GL_TRIANGLES, 0, 6);                                            CHECK_GL_ERRORS

    // Отсоединяем VAO
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);                                           CHECK_GL_ERRORS
    // Отключаем шейдер
    glUseProgram(0);                                                             CHECK_GL_ERRORS
}

// Обновление смещения травинок
void UpdateGrassVariance() {
    // Генерация случайных смещений

    for (uint i = 0; i < GRASS_INSTANCES; ++i) {
        grassVelocityData[i].x *= (1.0 - grassExtCoeff);
        grassVelocityData[i].z *= (1.0 - grassExtCoeff);
        if (windTrigger) {
             grassVelocityData[i].x += grassWindData[i].x * grassScaleData[i].y*delta_time;
             grassVelocityData[i].z += grassWindData[i].y * grassScaleData[i].y*delta_time;
         }
         grassVelocityData[i].x -= grassGukCoeff * grassVarianceData[i].x * delta_time;
         grassVelocityData[i].z -= grassGukCoeff * grassVarianceData[i].z * delta_time;
         grassVarianceData[i].x += grassVelocityData[i].x * delta_time;
         grassVarianceData[i].z += grassVelocityData[i].z * delta_time;
     }
    // Привязываем буфер, содержащий смещения
    glBindBuffer(GL_ARRAY_BUFFER, grassVariance);                                CHECK_GL_ERRORS
    // Загружаем данные в видеопамять
    glBufferData(GL_ARRAY_BUFFER, sizeof(VM::vec4) * GRASS_INSTANCES, grassVarianceData.data(), GL_STATIC_DRAW); CHECK_GL_ERRORS
    // Отвязываем буфер
    glBindBuffer(GL_ARRAY_BUFFER, 0);                                            CHECK_GL_ERRORS
}

void ComputeWindData()
{
    float wind_sin = sin(windData.x), wind_cos = cos(windData.x);
    float wind_pow = windData.y;
    for (uint i = 0; i < GRASS_INSTANCES; ++i) {
        float grass_sin = grassRotationData[i].x;
        float grass_cos = grassRotationData[i].y;
        
        float proj_sin = fabs(wind_sin*grass_cos - wind_cos*grass_sin);
        grassWindData[i] = VM::vec2(wind_cos, wind_sin) * \
                                    wind_pow*proj_sin;
    }
}

// Рисование травы
void DrawGrass() {
    // Тут то же самое, что и в рисовании земли
    glUseProgram(grassShader);                                                   CHECK_GL_ERRORS
    GLint cameraLocation = glGetUniformLocation(grassShader, "camera");          CHECK_GL_ERRORS
    glUniformMatrix4fv(cameraLocation, 1, GL_TRUE, camera.getMatrix().data().data()); CHECK_GL_ERRORS
    glBindTexture(GL_TEXTURE_2D, grassTexture);
    glBindVertexArray(grassVAO);                                                 CHECK_GL_ERRORS
    // Обновляем смещения для травы
    UpdateGrassVariance();
    // Отрисовка травинок в количестве GRASS_INSTANCES
    glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, grassPointsCount, \
                          GRASS_INSTANCES);   CHECK_GL_ERRORS
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindVertexArray(0);                                                        CHECK_GL_ERRORS
    glUseProgram(0);                                                             CHECK_GL_ERRORS
}

// Эта функция вызывается для обновления экрана
void RenderLayouts() {
    // Включение буфера глубины

    do {
        delta_time = float(clock())/CLOCKS_PER_SEC - prev_time;
    } while (delta_time < 1.0/MAX_FPS);

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    // Очистка буфера глубины и цветового буфера
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // Рисуем меши
    DrawGrass();
    DrawGround();
    glutSwapBuffers();
    prev_time = float(clock())/CLOCKS_PER_SEC;
}

// Завершение программы
void FinishProgram() {
    glutDestroyWindow(glutGetWindow());
}

GLuint loadSkybox(vector<const GLchar*> faces)
{
    GLuint textureID;
    glGenTextures(1, &textureID);
    glActiveTexture(GL_TEXTURE0);

    int width,height;
    unsigned char* image;
    
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);
    for(GLuint i = 0; i < faces.size(); i++)
    {
        image = SOIL_load_image(faces[i], &width, &height, 0, SOIL_LOAD_RGB);
        glTexImage2D(
            GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0,
            GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, image
        );
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    return textureID;
}  

// Обработка события нажатия клавиши (специальные клавиши обрабатываются в функции SpecialButtons)
void KeyboardEvents(unsigned char key, int x, int y) {
    if (key == 27) {
        FinishProgram();
    } else if (key == 'z') {
        camera.goForward();
    } else if (key == 'x') {
        camera.goBack();
    } else if (key == 'm') {
        captureMouse = !captureMouse;
        if (captureMouse) {
            glutWarpPointer(screenWidth / 2, screenHeight / 2);
            glutSetCursor(GLUT_CURSOR_NONE);
        } else {
            glutSetCursor(GLUT_CURSOR_RIGHT_ARROW);
        }
    } else if (key == 'w') {
        windTrigger = 1 - windTrigger;
        cout << "wind:" << windTrigger << endl;
    } else if (key == 'j') {
        windData.x += M_PI/16.0;
        ComputeWindData();
    } else if (key == 'l') {
        windData.x -= M_PI/16.0;
        cout << "curr angle: PI*" << windData.x/M_PI << endl;
        ComputeWindData();
    } else if (key == 'a') {
        msaaTrigger = 1 - msaaTrigger;
        if (msaaTrigger) {
            glEnable(GL_MULTISAMPLE);  CHECK_GL_ERRORS
        } else {
            glDisable(GL_MULTISAMPLE);  CHECK_GL_ERRORS
        }
    }
}

// Обработка события нажатия специальных клавиш
void SpecialButtons(int key, int x, int y) {
    if (key == GLUT_KEY_RIGHT) {
        camera.rotateY(0.05);
    } else if (key == GLUT_KEY_LEFT) {
        camera.rotateY(-0.05);
    } else if (key == GLUT_KEY_UP) {
        camera.rotateTop(-0.05);
    } else if (key == GLUT_KEY_DOWN) {
        camera.rotateTop(0.05);
    }
}

void IdleFunc() {
    glutPostRedisplay();
}

// Обработка события движения мыши
void MouseMove(int x, int y) {
    if (captureMouse) {
        int centerX = screenWidth / 2,
            centerY = screenHeight / 2;
        if (x != centerX || y != centerY) {
            camera.rotateY((x - centerX) / 1000.0f);
            camera.rotateTop((y - centerY) / 1000.0f);
            glutWarpPointer(centerX, centerY);
        }
    }
}

// Обработка нажатия кнопки мыши
void MouseClick(int button, int state, int x, int y) {
}

// Событие изменение размера окна
void windowReshapeFunc(GLint newWidth, GLint newHeight) {
    glViewport(0, 0, newWidth, newHeight);
    screenWidth = newWidth;
    screenHeight = newHeight;

    camera.screenRatio = (float)screenWidth / screenHeight;
}

// Инициализация окна
void InitializeGLUT(int argc, char **argv) {
    glutInit(&argc, argv);
    glutSetOption(GLUT_MULTISAMPLE, 4);
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE | GLUT_DEPTH | GLUT_MULTISAMPLE);
    glutInitContextVersion(3, 0);
    glutInitContextProfile(GLUT_CORE_PROFILE);
    glutInitWindowPosition(-1, -1);
    glutInitWindowSize(screenWidth, screenHeight);
    glutCreateWindow("Computer Graphics 3");
    glutWarpPointer(400, 300);
    glutSetCursor(GLUT_CURSOR_NONE);

    glutDisplayFunc(RenderLayouts);
    glutKeyboardFunc(KeyboardEvents);
    glutSpecialFunc(SpecialButtons);
    glutIdleFunc(IdleFunc);
    glutPassiveMotionFunc(MouseMove);
    glutMouseFunc(MouseClick);
    glutReshapeFunc(windowReshapeFunc);
}

// Генерация позиций травинок (эту функцию вам придётся переписать)
vector<VM::vec2> GenerateGrassPositions() {
    vector<VM::vec2> grassPositions(GRASS_INSTANCES);
    int sq_grass = sqrt(GRASS_INSTANCES);
    for (uint i = 0; i < GRASS_INSTANCES; ++i) {
        grassPositions[i] = VM::vec2((i % sq_grass)/(float)(sq_grass), \
                                     (i / sq_grass)/(float)(sq_grass)) + \
                                     VM::vec2(0.5, 0.5)/(float)(sq_grass) + \
                                     VM::vec2((((float)(rand()) / RAND_MAX) - 0.5) / 70.0, \
                                              (((float)(rand()) / RAND_MAX) - 0.5) / 70.0);
    }
    return grassPositions;
}

// Генерация масштаба травинок (эту функцию вам придётся переписать)
vector<VM::vec2> GenerateGrassScales() {
    vector<VM::vec2> grassScales(GRASS_INSTANCES);

    for (uint i = 0; i < GRASS_INSTANCES; ++i) {
        grassScales[i] = VM::vec2(((float(rand()) / RAND_MAX) + 0.5) , \
                                  ((float(rand()) / RAND_MAX) + 0.5));
    }
    return grassScales;
}


// Здесь вам нужно будет генерировать меш
vector<VM::vec4> GenMesh(uint n) {
    return {
        VM::vec4(  0, 0, 0, 1),
        VM::vec4(0.4, 0, 0, 1),
        VM::vec4(0.22, 0.25, 0, 1),
        VM::vec4(0.58, 0.25, 0, 1),
        VM::vec4(0.35, 0.5, 0.003, 1),
        VM::vec4(0.77, 0.5, 0.003, 1),
        VM::vec4(0.56, 0.75, 0.007, 1),
        VM::vec4(0.84, 0.75, 0.007, 1),
        VM::vec4(0.7, 1, 0.015, 1)
    };
}

vector<VM::vec2> GenTexCoords(uint n)
{
    return {
        VM::vec2(  0,    0),
        VM::vec2(  1,    0),
        VM::vec2(  0, 0.25),
        VM::vec2(  1, 0.25),
        VM::vec2(  0, 0.50),
        VM::vec2(  1, 0.50),
        VM::vec2(  0, 0.75),
        VM::vec2(  1, 0.75),
        VM::vec2(0.5,    1)
    };
}

// Создание травы
void CreateGrass() {
    uint LOD = 1;
    // Создаём меш
    vector<VM::vec4> grassPoints = GenMesh(LOD);
    vector<VM::vec2> grassTexCoords = GenTexCoords(LOD);
    // Сохраняем количество вершин в меше травы
    grassPointsCount = grassPoints.size();
    // Создаём позиции для травинок
    vector<VM::vec2> grassPositions = GenerateGrassPositions();
    grassScaleData = GenerateGrassScales();
    // Инициализация смещений для травинок
    for (uint i = 0; i < GRASS_INSTANCES; ++i) {
        grassVarianceData[i] = VM::vec4(0, 0, 0, 0);
        float rand_angle = 2*M_PI * (float(rand()) / RAND_MAX);

        float grass_sin = sin(rand_angle), grass_cos = cos(rand_angle);
        grassRotationData[i] = VM::vec2(grass_sin, grass_cos);
        grassVelocityData[i] = VM::vec3(((float(rand()) / RAND_MAX) - 0.5)/50.0, 0, \
                                        ((float(rand()) / RAND_MAX) - 0.5)/50.0);
        grassColorData[i] = VM::vec4( (float(rand()) / RAND_MAX)*0.45, 
                                      (float(rand()) / RAND_MAX)*0.705+0.295, \
                                      (float(rand()) / RAND_MAX)*0.05, 0.0);
    }
    ComputeWindData();
    /* Компилируем шейдеры
    Эта функция принимает на вход название шейдера 'shaderName',
    читает файлы shaders/{shaderName}.vert - вершинный шейдер
    и shaders/{shaderName}.frag - фрагментный шейдер,
    компилирует их и линкует.
    */



    //===================================================[Создаём текстурку :3]
    int height;
    int width;

    unsigned char *image = SOIL_load_image("./Texture/grass.jpg", &width, &height, 0, SOIL_LOAD_RGBA);
 
    grassShader = GL::CompileShaderProgram("grass");

    glGenTextures(1, &grassTexture);                                                  CHECK_GL_ERRORS
    glBindTexture(GL_TEXTURE_2D, grassTexture);                                       CHECK_GL_ERRORS
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image); CHECK_GL_ERRORS
    glGenerateMipmap(GL_TEXTURE_2D);                                              CHECK_GL_ERRORS

    SOIL_free_image_data(image);
    glBindTexture(GL_TEXTURE_2D, 0);                                             CHECK_GL_ERRORS 
    //===================================================[]
    // Здесь создаём буфер
    GLuint pointsBuffer;
    // Это генерация одного буфера (в pointsBuffer хранится идентификатор буфера)
    glGenBuffers(1, &pointsBuffer);                                              CHECK_GL_ERRORS
    // Привязываем сгенерированный буфер
    glBindBuffer(GL_ARRAY_BUFFER, pointsBuffer);                                 CHECK_GL_ERRORS
    // Заполняем буфер данными из вектора
    glBufferData(GL_ARRAY_BUFFER, sizeof(VM::vec4) * grassPoints.size(), grassPoints.data(), GL_STATIC_DRAW); CHECK_GL_ERRORS

    // Создание VAO
    // Генерация VAO
    glGenVertexArrays(1, &grassVAO);                                             CHECK_GL_ERRORS
    // Привязка VAO
    glBindVertexArray(grassVAO);                                                 CHECK_GL_ERRORS

    // Получение локации параметра 'point' в шейдере
    GLuint pointsLocation = glGetAttribLocation(grassShader, "point");           CHECK_GL_ERRORS
    // Подключаем массив атрибутов к данной локации
    glEnableVertexAttribArray(pointsLocation);                                   CHECK_GL_ERRORS
    // Устанавливаем параметры для получения данных из массива (по 4 значение типа float на одну вершину)
    glVertexAttribPointer(pointsLocation, 4, GL_FLOAT, GL_FALSE, 0, 0);          CHECK_GL_ERRORS
    //===================================================[Текстурки ¯\_(ツ)_/¯]
    GLuint texBuffer; // Для текстур!
    glGenBuffers(1, &texBuffer);                                                 CHECK_GL_ERRORS
    glBindBuffer(GL_ARRAY_BUFFER, texBuffer);                                    CHECK_GL_ERRORS
    glBufferData(GL_ARRAY_BUFFER, sizeof(VM::vec2) * grassTexCoords.size(), grassTexCoords.data(), GL_STATIC_DRAW); CHECK_GL_ERRORS

    GLuint texIndex = glGetAttribLocation(grassShader, "texcoords");            CHECK_GL_ERRORS
    glEnableVertexAttribArray(texIndex);                                         CHECK_GL_ERRORS
    glVertexAttribPointer(texIndex, 2, GL_FLOAT, GL_FALSE, 0, 0);                CHECK_GL_ERRORS
    //====================================================
    // Создаём буфер для позиций травинок
    GLuint positionBuffer;
    glGenBuffers(1, &positionBuffer);                                            CHECK_GL_ERRORS
    // Здесь мы привязываем новый буфер, так что дальше вся работа будет с ним до следующего вызова glBindBuffer
    glBindBuffer(GL_ARRAY_BUFFER, positionBuffer);                               CHECK_GL_ERRORS
    glBufferData(GL_ARRAY_BUFFER, sizeof(VM::vec2) * grassPositions.size(), grassPositions.data(), GL_STATIC_DRAW); CHECK_GL_ERRORS

    GLuint positionLocation = glGetAttribLocation(grassShader, "position");      CHECK_GL_ERRORS
    glEnableVertexAttribArray(positionLocation);                                 CHECK_GL_ERRORS
    glVertexAttribPointer(positionLocation, 2, GL_FLOAT, GL_FALSE, 0, 0);        CHECK_GL_ERRORS
    // Здесь мы указываем, что нужно брать новое значение из этого буфера для каждого инстанса (для каждой травинки)
    glVertexAttribDivisor(positionLocation, 1);                                  CHECK_GL_ERRORS

    // Создаём буфер для смещения травинок
    glGenBuffers(1, &grassVariance);                                            CHECK_GL_ERRORS
    glBindBuffer(GL_ARRAY_BUFFER, grassVariance);                               CHECK_GL_ERRORS
    glBufferData(GL_ARRAY_BUFFER, sizeof(VM::vec4) * GRASS_INSTANCES, grassVarianceData.data(), GL_STATIC_DRAW); CHECK_GL_ERRORS

    GLuint varianceLocation = glGetAttribLocation(grassShader, "variance");      CHECK_GL_ERRORS
    glEnableVertexAttribArray(varianceLocation);                                 CHECK_GL_ERRORS
    glVertexAttribPointer(varianceLocation, 4, GL_FLOAT, GL_FALSE, 0, 0);        CHECK_GL_ERRORS
    glVertexAttribDivisor(varianceLocation, 1);                                  CHECK_GL_ERRORS

    // Создаём буфер для поворота травинок
    glGenBuffers(1, &grassRotation);                                            CHECK_GL_ERRORS
    glBindBuffer(GL_ARRAY_BUFFER, grassRotation);                               CHECK_GL_ERRORS
    glBufferData(GL_ARRAY_BUFFER, sizeof(VM::vec2) * GRASS_INSTANCES, grassRotationData.data(), GL_STATIC_DRAW); CHECK_GL_ERRORS

    GLuint rotationLocation = glGetAttribLocation(grassShader, "rotation");      CHECK_GL_ERRORS
    glEnableVertexAttribArray(rotationLocation);                                 CHECK_GL_ERRORS
    glVertexAttribPointer(rotationLocation, 2, GL_FLOAT, GL_FALSE, 0, 0);        CHECK_GL_ERRORS
    glVertexAttribDivisor(rotationLocation, 1);                                  CHECK_GL_ERRORS

    // Создаём буфер для цвета травинок
    GLuint colorBuffer;
    glGenBuffers(1, &colorBuffer);                                            CHECK_GL_ERRORS
    glBindBuffer(GL_ARRAY_BUFFER, colorBuffer);                               CHECK_GL_ERRORS
    glBufferData(GL_ARRAY_BUFFER, sizeof(VM::vec4) * GRASS_INSTANCES, grassColorData.data(), GL_STATIC_DRAW); CHECK_GL_ERRORS

    GLuint colorLocation = glGetAttribLocation(grassShader, "colors");      CHECK_GL_ERRORS
    glEnableVertexAttribArray(colorLocation);                                 CHECK_GL_ERRORS
    glVertexAttribPointer(colorLocation, 4, GL_FLOAT, GL_FALSE, 0, 0);        CHECK_GL_ERRORS
    glVertexAttribDivisor(colorLocation, 1);                                  CHECK_GL_ERRORS

     // Создаём буфер для масштаба травинок
    GLuint scaleBuffer;
    glGenBuffers(1, &scaleBuffer);                                            CHECK_GL_ERRORS
    // Здесь мы привязываем новый буфер, так что дальше вся работа будет с ним до следующего вызова glBindBuffer
    glBindBuffer(GL_ARRAY_BUFFER, scaleBuffer);                               CHECK_GL_ERRORS
    glBufferData(GL_ARRAY_BUFFER, sizeof(VM::vec2) * grassScaleData.size(), grassScaleData.data(), GL_STATIC_DRAW); CHECK_GL_ERRORS

    GLuint scaleLocation = glGetAttribLocation(grassShader, "scale");      CHECK_GL_ERRORS
    glEnableVertexAttribArray(scaleLocation);                                 CHECK_GL_ERRORS
    glVertexAttribPointer(scaleLocation, 2, GL_FLOAT, GL_FALSE, 0, 0);        CHECK_GL_ERRORS
    // Здесь мы указываем, что нужно брать новое значение из этого буфера для каждого инстанса (для каждой травинки)
    glVertexAttribDivisor(scaleLocation, 1);                                  CHECK_GL_ERRORS


    // Отвязываем VAO
    glBindVertexArray(0);                                                        CHECK_GL_ERRORS
    // Отвязываем буфер
    glBindBuffer(GL_ARRAY_BUFFER, 0);                                            CHECK_GL_ERRORS
}

// Создаём камеру (Если шаблонная камера вам не нравится, то можете переделать, но я бы не стал)
void CreateCamera() {
    camera.angle = 45.0f / 180.0f * M_PI;
    camera.direction = VM::vec3(0, 0.3, -1);
    camera.position = VM::vec3(0.5, 0.2, 0);
    camera.screenRatio = (float)screenWidth / screenHeight;
    camera.up = VM::vec3(0, 1, 0);
    camera.zfar = 50.0f;
    camera.znear = 0.05f;
}

// Создаём замлю
void CreateGround() {
//=====================================[ТОЧКИ!!!]
// Точки меша и текстуры соответсвенно
// ᕕ( ᐛ )ᕗ
    vector<VM::vec4> meshPoints = {
        VM::vec4(0, 0, 0, 1),
        VM::vec4(1, 0, 0, 1),
        VM::vec4(1, 0, 1, 1),
        VM::vec4(0, 0, 0, 1),
        VM::vec4(1, 0, 1, 1),
        VM::vec4(0, 0, 1, 1),
    };

    vector<VM::vec2> texPoints = {
        VM::vec2(0, 0),    
        VM::vec2(1, 0),    
        VM::vec2(1, 1),   
        VM::vec2(0, 0),   
        VM::vec2(1, 1),    
        VM::vec2(0, 1),    
    };
//===================================================[Создаём текстурку :3]
    int height;
    int width;

    unsigned char *image = SOIL_load_image("./Texture/ground_grass.jpg", &width, &height, 0, SOIL_LOAD_RGBA);
 
    groundShader = GL::CompileShaderProgram("ground");

    glGenTextures(1, &groundTexture);                                                  CHECK_GL_ERRORS
    glBindTexture(GL_TEXTURE_2D, groundTexture);                                       CHECK_GL_ERRORS
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, image); CHECK_GL_ERRORS
    glGenerateMipmap(GL_TEXTURE_2D);                                              CHECK_GL_ERRORS

    SOIL_free_image_data(image);
    glBindTexture(GL_TEXTURE_2D, 0);                                             CHECK_GL_ERRORS 


//=================================================[Массивы вершин]
    glGenVertexArrays(1, &groundVAO);                                            CHECK_GL_ERRORS
    glBindVertexArray(groundVAO);                                                CHECK_GL_ERRORS

//=================================================[Буферы!!]
    GLuint pointsBuffer; // Для точек!
    glGenBuffers(1, &pointsBuffer);                                              CHECK_GL_ERRORS
    glBindBuffer(GL_ARRAY_BUFFER, pointsBuffer);                                 CHECK_GL_ERRORS
    glBufferData(GL_ARRAY_BUFFER, sizeof(VM::vec4) * meshPoints.size(), meshPoints.data(), GL_STATIC_DRAW); CHECK_GL_ERRORS

    GLuint pointIndex = glGetAttribLocation(groundShader, "point");                   CHECK_GL_ERRORS
    cout << pointIndex << endl;
    glEnableVertexAttribArray(pointIndex);                                            CHECK_GL_ERRORS
    glVertexAttribPointer(pointIndex, 4, GL_FLOAT, GL_FALSE, 0, 0);                   CHECK_GL_ERRORS


    GLuint texBuffer; // Для текстур!
    glGenBuffers(1, &texBuffer);                                                 CHECK_GL_ERRORS
    glBindBuffer(GL_ARRAY_BUFFER, texBuffer);                                    CHECK_GL_ERRORS
    glBufferData(GL_ARRAY_BUFFER, sizeof(VM::vec2) * texPoints.size(), texPoints.data(), GL_STATIC_DRAW); CHECK_GL_ERRORS

    GLuint texIndex = glGetAttribLocation(groundShader, "texcoords");            CHECK_GL_ERRORS
    glEnableVertexAttribArray(texIndex);                                         CHECK_GL_ERRORS
    glVertexAttribPointer(texIndex, 2, GL_FLOAT, GL_FALSE, 0, 0);                CHECK_GL_ERRORS


//===============================================[Очищаем всё!]
    glBindVertexArray(0);                                                        CHECK_GL_ERRORS
    glBindBuffer(GL_ARRAY_BUFFER, 0);                                            CHECK_GL_ERRORS
}

int main(int argc, char **argv)
{
    srand(time(0));
    try {
        cout << "Start" << endl;
        InitializeGLUT(argc, argv);
        cout << "GLUT inited" << endl;
        glewInit();
        cout << "glew inited" << endl;
        CreateCamera();
        cout << "Camera created" << endl;
        CreateGrass();
        cout << "Grass created" << endl;
        CreateGround();
        cout << "Ground created" << endl;
        glutMainLoop();
    } catch (string s) {
        cout << s << endl;
    }
}
