// for Windows
#define _CRT_SECURE_NO_WARNINGS

#include <GL/glew.h>
#include <GL/glut.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <vector>
#include "bunny.h"
#include "vec3.h"

using namespace std;

static void display(void);
static void idle(void);
static int getShaderSource(const char *fileName, GLenum shaderType, GLuint *compiledProgram);
static int useShaders(GLuint VertShader, GLuint FragShader, GLuint *program);
static int freeShaders(GLuint VertShader, GLuint FragShader, GLuint program);
static int transferData(void *data, int num, GLenum bufferType, GLuint *buffer);
static int bindAttributeVariable(GLuint program, GLuint VBO, const char *name);
static int bindUniformVariable4x4(GLuint program, float *data, const char *name);
static int loadBunny(const char *filename, bunny *b);
static int freeBunny(bunny *b);
static void multiply4x4(float A[16], float B[16], float AB[16]); // A * B = AB
static void createOrthogonal(float Left, float Right, float Top, float Bottom, float Near, float Far, float Matrix[16]);
static void createLookAt(float position[3], float orientation[3], float up[3], float Matrix[16]);

const float PI = 3.14159f;

static float vertices[] = {
	-1.0f, 0.0f, 0.0f,
	1.0f, 0.0f, 0.0f,
	0.0f, 1.0f, 0.0f
};

static float rotationMatrix[] = {
	1.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 1.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 1.0f
};

static float expantionMatrix[] = {
	1.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 1.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 1.0f
};

static bunny b;

static GLuint program;
GLuint VBO; // Vertex Buffer Object
GLuint IBO; // Index Buffer Object
GLuint normalVector;

int main(int argc, char *argv[]) {
	GLenum err;
	GLuint vShader, fShader;
	int returnValue;

	// スタンフォードバニーの読み込み
	returnValue = loadBunny("bun_zipper.ply", &b);
	if (returnValue == -1) {
		return -1;
	}

	// GLUTの初期化
	glutInit(&argc, argv);

	// ウィンドウの作成
	glutCreateWindow(argv[0]);

	// glewの初期化
	err = glewInit();
	if (err != GLEW_OK) {
		return -1;
	}

	// 描画関数の設定
	glutDisplayFunc(display);

	// アイドル関数の設定
	glutIdleFunc(idle);

	// シェーダを読み込む
	returnValue = getShaderSource("main.vert", GL_VERTEX_SHADER, &vShader);
	if (returnValue == -1) {
		printf("Error: getShaderSource\n");
		return -1;
	}

	getShaderSource("main.frag", GL_FRAGMENT_SHADER, &fShader);
	if (returnValue == -1) {
		printf("Error: getShaderSource\n");
		return -1;
	}

	useShaders(vShader, fShader, &program);
	if (returnValue == -1) {
		printf("Error: useShaders");
		return -1;
	}

	// データをGPUに転送する
	transferData(b.vertices, b.vertexNum, GL_ARRAY_BUFFER, &VBO);
	transferData(b.normalVectors, b.vectorNum, GL_ARRAY_BUFFER, &normalVector);
	transferData(b.vertexIndices, b.indexNum, GL_ELEMENT_ARRAY_BUFFER, &IBO);

	// VBOとバーテックスシェーダのin変数とを関連付ける
	bindAttributeVariable(program, VBO, "position");
	bindAttributeVariable(program, normalVector, "normal");

	// メインループ
	glutMainLoop();

	// シェーダオブジェクト、シェーダプログラムを破棄する
	freeShaders(vShader, fShader, program);
	program = 0;

	// バッファを削除する
	glDeleteBuffers(1, &VBO);
	glDeleteBuffers(1, &IBO);

	// スタンフォードバニーを削除する
	freeBunny(&b);

	return 0;
}

static void display(void) {
	static float degree = 0.0;
	float rad;
	GLfloat white[] = { 1.0f, 1.0f, 1.0f, 1.0f };
	GLfloat transformMatrix[16];
	GLfloat orthogonalMatrix[16];
	GLfloat LookAtMatrix[16];
	GLfloat tmpMatrix[16];
	GLfloat resultMatrix[16];

	// ウィンドウを白で塗りつぶす
	glClearBufferfv(GL_COLOR, 0, white);

	// 平行投影変換行列を生成する
	createOrthogonal(-0.5f, 0.5f, 0.5f, -0.5f, 0.0f, 10.0f, orthogonalMatrix);
	
	// 視野変換行列を生成する
	float position[3] = {0.0f, 0.0f, 0.0f};
	float orientation[3] = {0.0f, 0.0f, -1.0f};
	float up[3] = {0.0f, 1.0f, 0.0f};
	createLookAt(position, orientation, up, LookAtMatrix);

	// 回転行列を生成する
	rad = degree * PI / 180.0;
	rotationMatrix[0] = cosf(rad);
	rotationMatrix[2] = sinf(rad);
	rotationMatrix[8] = -sinf(rad);
	rotationMatrix[10] = cosf(rad);

	// 変換行列をuniform変数に関連付ける
	multiply4x4(rotationMatrix, expantionMatrix, transformMatrix);
	multiply4x4(LookAtMatrix, orthogonalMatrix, tmpMatrix);
	multiply4x4(tmpMatrix, transformMatrix, resultMatrix);
	bindUniformVariable4x4(program, resultMatrix, "transformMatrix");

	// 描画
	glDrawElements(GL_TRIANGLES, b.indexNum, GL_UNSIGNED_INT, 0);

	glFlush();

	if (fabs(degree - 360.0) < 0.01) {
		degree = 0.0;
	}
	else {
		degree += 0.01f;
	}

	return;
}

void idle(void) {
	glutPostRedisplay();
}

static int getShaderSource(const char *fileName, GLenum shaderType, GLuint *compiledProgram) {
	int capacity = 1024;
	int usage = 0;
	FILE *fp;
	GLuint shader;
	GLint compileStatus;
	int returnValue;
	GLchar Log[1024];
	GLsizei length;

	// とりあえず1KB確保
	char *source = (char *) malloc(sizeof(char) * capacity);
	if (source == NULL) {
		printf("Error: malloc\n");
		return -1;
	}

	// ファイルオープン
	fp = fopen(fileName, "r");
	if (fp == NULL) {
		printf("Error: fopen\n");
		return -1;
	}

	// 終端までファイルを読み取る
	while (1) {
		int num;

		// バッファが満杯であれば、realloc
		if (capacity == usage) {
			char *tmpp;

			tmpp = (char *) realloc(source, capacity * 2);
			if (tmpp == NULL) {
				printf("Error: realloc\n");
				return -1;
			}
			source = tmpp;
			capacity *= 2;
		}

		// 文字列読み込み
		num = fread(&source[usage], sizeof(char), capacity - usage, fp);
		usage += num;

		// 読み取れるものがなくなったら終了
		if (num == 0) {
			// 念のため、Null terminateしておく
			source[usage] = '\0';
			usage++;

			break;
		}
	}

	// ファイルをクローズ
	returnValue = fclose(fp);
	if (returnValue == EOF) {
		printf("Error: fclose\n");
		return -1;
	}
	fp = NULL;

	// シェーダオブジェクトを作成する
	shader = glCreateShader(shaderType);

	// コンパイル
	const char* src = source;
	glShaderSource(shader, 1, &src, &usage);
	glCompileShader(shader);

	// コンパイルステータスをgetする
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compileStatus);
	if (compileStatus == GL_FALSE) {
		glGetShaderInfoLog(shader, 1024 * sizeof(GLchar), &length, Log);
		printf("Error: glGetShaderiv\n%s\n", Log);
		return -1;
	}

	// freeする
	free(source);
	source = NULL;

	// コンパイル済みシェーダオブジェクトを返す
	*compiledProgram = shader;

	return 0;
}

static int useShaders(GLuint VertShader, GLuint FragShader, GLuint *program) {
	GLuint shaderp;
	GLint linkStatus;
	GLchar Log[1024];
	GLsizei length;

	// シェーダプログラムを作成
	shaderp = glCreateProgram();

	// バーテックスシェーダとフラグメントシェーダをアタッチする
	glAttachShader(shaderp, VertShader);
	glAttachShader(shaderp, FragShader);

	// シェーダをリンクする
	glLinkProgram(shaderp);

	// リンクステータスをgetする
	glGetProgramiv(shaderp, GL_LINK_STATUS, &linkStatus);
	if (linkStatus == GL_FALSE) {
		glGetProgramInfoLog(shaderp, 1024 * sizeof(GLchar), &length, Log);
		printf("Error: glGetProgramiv\n%s\n", Log);
		return -1;
	}

	// シェーダプログラムを適用する
	glUseProgram(shaderp);

	// シェーダプログラムを返す
	*program = shaderp;

	return 0;
}

static int freeShaders(GLuint VertShader, GLuint FragShader, GLuint program) {
	// シェーダオブジェクトを削除する
	glDeleteShader(VertShader);
	glDeleteShader(FragShader);

	// シェーダプログラムを削除する
	glDeleteProgram(program);

	return 0;
}

static int transferData(void *data, int num, GLenum bufferType, GLuint *buffer) {
	// バッファを作成する
	glGenBuffers(1, buffer);

	// バッファにデータをセットする
	glBindBuffer(bufferType, *buffer);
	glBufferData(bufferType, sizeof(float) * num, data, GL_STATIC_DRAW);

	return 0;
}

static int bindAttributeVariable(GLuint program, GLuint VBO, const char *name) {
	GLint attr;

	// VBOをバインドする
	glBindBuffer(GL_ARRAY_BUFFER, VBO);

	// VBOとin変数を関連付ける
	attr = glGetAttribLocation(program, name);
	glEnableVertexAttribArray(attr);
	glVertexAttribPointer(attr, 3, GL_FLOAT, GL_FALSE, 0, 0);

	return 0;
}

static int bindUniformVariable4x4(GLuint program, float *data, const char *name) {
	GLint uniform;

	// uniform変数の場所をgetする
	uniform = glGetUniformLocation(program, name);

	// 4x4の行列をuniform変数に関連付ける
	glUniformMatrix4fv(uniform, 1, GL_FALSE, data);

	return 0;
}

static int loadBunny(const char *filename, bunny *b) {
	FILE *fp;
	char *fgetsReturn;
	int fcloseReturn;
	char *buf;
	int strLength;
	int cmpResult;
	float x, y, z, confidence, intensity;
	int tmp, ix, iy, iz;
	float *vertices;
	unsigned int *indices;
	float *vertNormals;

	//ファイルオープン
	fp = fopen(filename, "r");
	if (fp == NULL) {
		printf("Error: fopen\n");
		return -1;
	}

	// 読み取り用バッファを1KB確保
	buf = new char[1024];
	if (buf == NULL) {
		printf("Error: malloc");
		return -1;
	}

	// end_headerまで読み飛ばす
	while (1) {
		// 行を読み込む
		fgetsReturn = fgets(buf, sizeof(char) * 1024, fp);
		if (fgetsReturn == NULL) {
			printf("Error: fgets");
			return -1;
		}

		// 文字列比較
		strLength = strlen(buf);
		cmpResult = strncmp(buf, "end_header\n", strLength);
		if (cmpResult == 0) {
			// ヘッダを読み終えた
			break;
		}
		
	}

	// 読み取り用バッファをfreeする
	delete[] buf;

	// 頂点配列を確保する。35947 * 3 * 4 = 431,364バイト必要。
	// 使用後、freeすること。
	const int vertNum = 35947 * 3; // 要素数
	vertices = new float[vertNum];
	if (vertices == NULL) {
		return -1;
	}

	// ひたすら読み込んでいく
	for (int i = 0; i < 35947; i++) {
		fscanf(fp, "%f %f %f %f %f", &x, &y, &z, &confidence, &intensity);
		vertices[3 * i + 0] = x;
		vertices[3 * i + 1] = y;
		vertices[3 * i + 2] = z;
	}

	// 頂点インデックス配列を確保する。69451 * 3 * 4 = 833,412バイト必要。
	// 使用後、freeすること。
	const int idxNum = 69451 * 3;	// 要素数
	indices = new unsigned int[idxNum];
	if (indices == NULL) {
		return -1;
	}

	// ひたすら読み込む
	for (int i = 0; i < 69451; i++) {
		fscanf(fp, "%d %d %d %d", &tmp, &ix, &iy, &iz);
		indices[3 * i + 0] = ix;
		indices[3 * i + 1] = iy;
		indices[3 * i + 2] = iz;
	}

	// ファイルクローズ
	fcloseReturn = fclose(fp);
	if (fcloseReturn == EOF) {
		return -1;
	}
	fp = NULL;

	// 面法線ベクトルを三角形から計算する
	vec3 *surfNormals = new vec3[idxNum / 3];

	for (int i = 0; i < idxNum / 3; i++) {
		vec3 point1, point2, point3;
		point1.x = vertices[indices[3 * i + 0] + 0];
		point1.y = vertices[indices[3 * i + 0] + 1];
		point1.z = vertices[indices[3 * i + 0] + 2];

		point2.x = vertices[indices[3 * i + 1] + 0];
		point2.y = vertices[indices[3 * i + 1] + 1];
		point2.z = vertices[indices[3 * i + 1] + 2];

		point3.x = vertices[indices[3 * i + 2] + 0];
		point3.y = vertices[indices[3 * i + 2] + 1];
		point3.z = vertices[indices[3 * i + 2] + 2];

		vec3 vec1, vec2;
		vec1.subtract(point2, point1);
		vec2.subtract(point3, point2);

		vec3 normal;
		normal.cross(vec1, vec2);
		normal.normalize();

		surfNormals[i].x = normal.x;
		surfNormals[i].y = normal.y;
		surfNormals[i].z = normal.z;
	}

	// 頂点法線ベクトル配列を確保する。431,364バイト必要。
	// 使用後、freeすること。
	const int normNum = vertNum;
	vertNormals = new float[vertNum];

	// 頂点法線ベクトルを求める
	vector<vector<unsigned int>> point(vertNum, vector<unsigned int>(10, 0));

	for (int i = 0; i < idxNum / 3; i++) {
		unsigned int p1 = indices[3 * i + 0];
		unsigned int p2 = indices[3 * i + 1];
		unsigned int p3 = indices[3 * i + 2];

		point[p1].push_back(i);
		point[p2].push_back(i);
		point[p3].push_back(i);
	}

	for (int i = 0; i < normNum / 3; i++) {
		vec3 vertNormal(0.0f, 0.0f, 0.0f);

		for (unsigned int j = 0; j < point[i].size(); j++) {
			vec3 tmp(surfNormals[point[i][j]].x, 
			         surfNormals[point[i][j]].y, 
			         surfNormals[point[i][j]].z);
			vertNormal.add(vertNormal, tmp);
		}

		vertNormal.normalize();

		vertNormals[3 * i + 0] = vertNormal.x;
		vertNormals[3 * i + 1] = vertNormal.y;
		vertNormals[3 * i + 2] = vertNormal.z;
	}

	// 面法線ベクトル配列をfreeする
	delete[] surfNormals;
	surfNormals = NULL;

	// 頂点配列を返す
	b->vertices = vertices;
	b->vertexNum = vertNum;

	// 頂点インデックス配列を返す
	b->vertexIndices = indices;
	b->indexNum = idxNum;

	// 法線ベクトル配列を返す
	b->normalVectors = vertNormals;
	b->vectorNum = normNum;

	return 0;
}

static int freeBunny(bunny *b) {
	delete[] b->vertices;
	b->vertices = NULL;
	b->vertexNum = 0;

	delete[] b->vertexIndices;
	b->vertexIndices = NULL;
	b->indexNum = 0;

	delete[] b->normalVectors;
	b->normalVectors = NULL;
	b->vectorNum = 0;

	return 0;
}

static void multiply4x4(float A[16], float B[16], float AB[16]) {
	float tmp;

	// ゼロクリア
	memset(AB, 0, sizeof(float) * 16);

	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			tmp = 0.0f;

			for (int k = 0; k < 4; k++) {
				tmp += A[4 * i + k] * B[4 * k + j];
			}

			AB[4 * i + j] = tmp;
		}
	}

	return;
}

static void createOrthogonal(float Left, float Right, float Top, float Bottom, float Near, float Far, float Matrix[16]) {
	Matrix[0] = 2.0f / (Right - Left);
	Matrix[1] = 0.0f;
	Matrix[2] = 0.0f;
	Matrix[3] = -(Right + Left) / (Right - Left);
	Matrix[4] = 0.0f;
	Matrix[5] = 2.0f / (Top - Bottom);
	Matrix[6] = 0.0f;
	Matrix[7] = -(Top + Bottom) / (Top - Bottom);
	Matrix[8] = 0.0f;
	Matrix[9] = 0.0f;
	Matrix[10] = -2.0f / (Far - Near);
	Matrix[11] = -(Far + Near) / (Far - Near);
	Matrix[12] = 0.0f;
	Matrix[13] = 0.0f;
	Matrix[14] = 0.0f;
	Matrix[15] = 1.0f;

	return;
}

static void createLookAt(float position[3], float orientation[3], float up[3], float Matrix[16]) {
	float v[3];

	// orientation × up
	v[0] = orientation[1] * up[2] - orientation[2] * up[1];
	v[1] = orientation[2] * up[0] - orientation[0] * up[2];
	v[2] = orientation[0] * up[1] - orientation[1] * up[0];

	Matrix[0] = orientation[0];
	Matrix[1] = orientation[1];
	Matrix[2] = orientation[2];
	Matrix[3] = -position[0] * orientation[0] - position[1] * orientation[1] - position[2] * orientation[2];
	Matrix[4] = up[0];
	Matrix[5] = up[1];
	Matrix[6] = up[2];
	Matrix[7] = -position[0] * up[0] - position[1] * up[1] - position[2] * up[2];
	Matrix[8] = v[0];
	Matrix[9] = v[1];
	Matrix[10] = v[2];
	Matrix[11] = -position[0] * v[0] - position[1] * v[1] - position[2] * v[2];
	Matrix[12] = 0.0f;
	Matrix[13] = 0.0f;
	Matrix[14] = 0.0f;
	Matrix[15] = 1.0f;

	return;
}